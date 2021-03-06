#include "FunctionRewriting.h"
#include "SemanticUtil.h"

#include <string>

using namespace clang;
using namespace llvm;

// We use this visitor to check for function pointers. If a pointer to a function is assigned, we invalidate the function
bool FunctionUnique::Analyser::VisitBinaryOperator(clang::BinaryOperator* BE) {
    if (BE->isAssignmentOp())
    {
        // Check if the RHS is a DeclRefExpr
        const Expr* rhs = BE->getRHS();
        const Expr* sub = rhs->IgnoreParenCasts();
        const clang::DeclRefExpr* DRE = dyn_cast<DeclRefExpr>(sub);
        if (DRE) {
            // If it is, check if it refers to a function
            const clang::FunctionDecl* FD = dyn_cast<FunctionDecl>(DRE->getFoundDecl());
            if (FD) {
                const FunctionUnique candidate(FD, astContext);
                const std::string& fileName = candidate.getFileName();

                // We make sure the file is contained in our base directory...
                if (fileName.find(metadata.baseDirectory) == std::string::npos)
                    return true;

                candidates.invalidate(candidate, "function is assigned as a pointer");
            }
        }
    }
    return true;
}

bool FunctionUnique::Analyser::VisitCallExpr(clang::CallExpr* CE) {
    FunctionDecl* FD = CE->getDirectCallee();
    if (FD) {
        const FunctionUnique candidate(FD, astContext);
        const std::string& fileName = candidate.getFileName();

        // We make sure the file is contained in our base directory...
        if (fileName.find(metadata.baseDirectory) == std::string::npos)
            return true;

        if (CE->getLocStart().isMacroID()) // Invalidate the function if it's in a macro.
            candidates.invalidate(candidate, "function is used in a macro");
        else
        {
            // Check if any of the argument expression has side effects. In this case we invalidate the function.
            // If one of the arguments is a macro we can't be sure if it has any side effects or not, therefore
            // we assume the worst and invalidate the function.
            for (unsigned iii = 0; iii < CE->getNumArgs(); iii++)
            {
                auto arg = CE->getArg(iii);
                if (arg->HasSideEffects(astContext, true))
                {
                    candidates.invalidate(candidate, "in one of the function invocations, one of the arguments has side effects");
                    break;
                }
            }
        }
    }

    return true;
}

// AST visitor, used for analysis.
bool FunctionUnique::Analyser::VisitFunctionDecl(clang::FunctionDecl* FD) {
    // We make sure we iterate over the definition.
    if (FD->isThisDeclarationADefinition()) {
        // If we haven't already selected the function, check if the function is eligible:
        // - can't be main (TODO: This should actually check for exported functions, not just main)
        // - can't be variadic (perhaps we can handle this in the future)
        // - has to have enough parameters, so at least 2
        if (!FD->isMain() && !FD->isVariadic() && FD->param_size() > 1) {
            const FunctionUnique candidate(FD, astContext);
            const std::string& fileName = candidate.getFileName();

            // We make sure the file is contained in our base directory...
            if (fileName.find(metadata.baseDirectory) == std::string::npos)
                return true;

            FunctionUnique::Data& data = candidates.get(candidate);
            if (data.valid && data.empty())
            {
                llvm::outs() << "Found valid candidate: " << candidate.getName() << "\n";
                data.addParams(FD);
            }
        }
    }

    return true;
}

// AST rewriter, used for rewriting source code.
bool FPReorderingRewriter::VisitCallExpr(clang::CallExpr* CE) {
    // We try to get the callee of this function call.
    if (FunctionDecl* FD = CE->getDirectCallee()) {
        // We check if this function is to be reordered
        const FunctionUnique target(FD, astContext);
        if (transformation.target == target) {
            llvm::outs() << "Call to function: " << FD->getNameAsString() << " has to be rewritten!\n";

            auto ordering = transformation.ordering;
            for (unsigned iii = 0; iii < CE->getNumArgs(); iii++)
            {
                const SourceRange& oldRange = CE->getArg(iii)->getSourceRange();
                const SourceRange& newRange = CE->getArg(ordering[iii])->getSourceRange();
                const SourceRange oldRangeExpanded(astContext.getSourceManager().getExpansionRange(oldRange.getBegin()).first, astContext.getSourceManager().getExpansionRange(oldRange.getEnd()).second);
                const SourceRange newRangeExpanded(astContext.getSourceManager().getExpansionRange(newRange.getBegin()).first, astContext.getSourceManager().getExpansionRange(newRange.getEnd()).second);

                // We replace the argument with another one based on the ordering.
                const std::string substitute = location2str(newRangeExpanded, astContext);
                rewriter.ReplaceText(oldRangeExpanded, substitute);
            }
        }
    }
    return true;
}

// AST rewriter, used for rewriting source code.
bool FPReorderingRewriter::VisitFunctionDecl(clang::FunctionDecl* FD) {
    // Check if this is a declaration for the function that is to be reordered
    FunctionUnique target(FD, astContext);
    if (transformation.target == target) {
        llvm::outs() << "Rewriting function: " << FD->getNameAsString() << " definition: " << FD->isThisDeclarationADefinition() << "\n";

        // We iterate over all parameters in the function declaration.
        const auto ordering = transformation.ordering;
        for (unsigned iii = 0; iii < FD->getNumParams(); iii++) {
            const ParmVarDecl* oldParam = FD->getParamDecl(iii);
            const ParmVarDecl* newParam = FD->getParamDecl(ordering[iii]);

            std::string name = newParam->getNameAsString();
            std::string type = newParam->getType().getAsString();
            std::string substitute = type + " " + name;

            // We replace the field with the new field information.
            rewriter.ReplaceText(oldParam->getSourceRange(), substitute);
        }
    }

    return true;
}


bool FPInsertionRewriter::VisitCallExpr(clang::CallExpr* CE) {
    // We try to get the callee of this function call.
    if (FunctionDecl* FD = CE->getDirectCallee()) {
        // We check if this function is to be reordered
        const FunctionUnique target(FD, astContext);
        if (transformation.target == target) {
            llvm::outs() << "Call to function: " << FD->getNameAsString() << " has to be rewritten!\n";

            int newArg = 0;

            if (transformation.insertionPoint < CE->getNumArgs())
            {
                const SourceRange& range = CE->getArg(transformation.insertionPoint)->getSourceRange();
                const SourceRange rangeExpanded(astContext.getSourceManager().getExpansionRange(range.getBegin()).first, astContext.getSourceManager().getExpansionRange(range.getEnd()).second);

                // We replace the argument with another one based on the ordering.
                const std::string substitute = std::to_string(newArg) + ", " + location2str(rangeExpanded, astContext);
                rewriter.ReplaceText(rangeExpanded, substitute);
            }
            else
            {
                const SourceRange& range = CE->getArg(CE->getNumArgs() -1)->getSourceRange();
                const SourceRange rangeExpanded(astContext.getSourceManager().getExpansionRange(range.getBegin()).first, astContext.getSourceManager().getExpansionRange(range.getEnd()).second);

                // We replace the argument with another one based on the ordering.
                const std::string substitute = location2str(rangeExpanded, astContext) + ", " + std::to_string(newArg);
                rewriter.ReplaceText(rangeExpanded, substitute);
            }
        }
    }

    return true;
}


bool FPInsertionRewriter::VisitFunctionDecl(clang::FunctionDecl* FD) {
    // Check if this is a declaration for the function that is to be reordered
    FunctionUnique target(FD, astContext);
    if (transformation.target == target) {
        llvm::outs() << "Rewriting function: " << FD->getNameAsString() << " definition: " << FD->isThisDeclarationADefinition() << "\n";

        const ParmVarDecl* param;
        bool before;
        if (transformation.insertionPoint < FD->getNumParams())
        {
            param = FD->getParamDecl(transformation.insertionPoint);
            before = true;
        }
        else
        {
            param = FD->getParamDecl(FD->getNumParams() -1);
            before = false;
        }

        std::string name = param->getNameAsString();
        std::string type = param->getType().getAsString();
        std::string substitute = type + " " + name;
        std::string newParam = "int XXX";

        if (before)
            substitute =  newParam + ", " + substitute;
        else
            substitute = substitute + ", " + newParam;

        // We replace the field with the new field information.
        rewriter.ReplaceText(param->getSourceRange(), substitute);
    }

    return true;
}
