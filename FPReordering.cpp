#include "FPReordering.h"
#include "SemanticUtil.h"

#include "clang/Tooling/Tooling.h"

#include "json.h"

#include <algorithm>
#include <sstream>
#include <string>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// We use this visitor to check for function pointers. If a pointer to a function is assigned, we invalidate the function
bool FPReorderingAnalyser::VisitBinaryOperator(clang::BinaryOperator* BE) {
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
                if (fileName.find(reordering.baseDirectory) == std::string::npos)
                    return true;

                reordering.invalidateCandidate(candidate, "function is assigned as a pointer");
            }
        }
    }
    return true;
}

bool FPReorderingAnalyser::VisitCallExpr(clang::CallExpr* CE) {
    FunctionDecl* FD = CE->getDirectCallee();
    if (FD) {
        const FunctionUnique candidate(FD, astContext);
        const std::string& fileName = candidate.getFileName();

        // We make sure the file is contained in our base directory...
        if (fileName.find(reordering.baseDirectory) == std::string::npos)
            return true;

        if (CE->getLocStart().isMacroID()) // Invalidate the function if it's in a macro.
            reordering.invalidateCandidate(candidate, "function is used in a macro");
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
                    reordering.invalidateCandidate(candidate, "in one of the function invocations, one of the arguments has side effects");
                    break;
                }
            }
        }
    }

    return true;
}

// AST visitor, used for analysis.
bool FPReorderingAnalyser::VisitFunctionDecl(clang::FunctionDecl* FD) {
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
            if (fileName.find(reordering.baseDirectory) == std::string::npos)
                return true;

            FunctionData& data = reordering.candidates[candidate];
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
        const FPTransformation* transformation = reordering.transformation;
        const FunctionUnique target(FD, astContext);
        if (transformation->target == target) {
            llvm::outs() << "Call to function: " << FD->getNameAsString() << " has to be rewritten!\n";

            auto ordering = transformation->ordering;
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
    const FPTransformation* transformation = reordering.transformation;
    FunctionUnique target(FD, astContext);
    if (transformation->target == target) {
        llvm::outs() << "Rewriting function: " << FD->getNameAsString() << " definition: " << FD->isThisDeclarationADefinition() << "\n";

        // We iterate over all parameters in the function declaration.
        const auto ordering = transformation->ordering;
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

// Method used for the function parameter semantic transformation.
void fpreordering(ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, const unsigned long numberOfReorderings) {
    // We run the analysis phase and get the valid candidates
    FPReordering reordering(baseDirectory, outputDirectory);
    Tool->run(new AnalysisFrontendActionFactory<FPReordering, FPReorderingAnalyser>(reordering));
    std::vector<FunctionUnique> candidates;
    for (const auto& it : reordering.candidates) {
        if (it.second.valid)
        {
            llvm::outs() << "Valid candidate: " << it.first.getName() << "\n";
            candidates.push_back(it.first);
        }
    }

    // We need to determine the maximum amount of reorderings that is actually
    // possible with the given input source files (based on the analysis phase).
    std::map<unsigned, unsigned> histogram;
    unsigned long totalReorderings = 0;
    double avgItems = 0;
    for (const auto& candidate : candidates) {
        unsigned nrOfItems = reordering.candidates[candidate].nrOfItems();

        // Keep count of the total possible reorderings and the average number of items
        totalReorderings += factorial(nrOfItems);
        avgItems += nrOfItems;

        // Look if this amount has already occured or not.
        if (histogram.find(nrOfItems) != histogram.end()) {
            histogram[nrOfItems]++;
        } else {
            histogram[nrOfItems] = 1;
        }
    }

    // Create analytics
    Json::Value analytics;
    analytics["number_of_candidates"] = candidates.size();
    analytics["avg_items"] = avgItems / candidates.size();
    analytics["entropy"] = entropyEquiprobable(totalReorderings);

    // Add histogram.
    for (const auto& it : histogram) {
        std::stringstream ss;
        ss << it.first;
        analytics["histogram"][ss.str()] = it.second;
    }

    // Output analytics
    llvm::outs() << "Total number of reorderings possible with " << candidates.size() << " candidates is: " << totalReorderings << "\n";
    llvm::outs() << "Writing analytics output...\n";
    writeJSONToFile(outputDirectory, -1, "analytics.json", analytics);

    unsigned long amountChosen = 0;
    unsigned long amount = std::min(numberOfReorderings, totalReorderings);
    std::vector<FPTransformation> transformations;
    llvm::outs() << "Amount of reorderings is set to: " << amount << "\n";
    while (amountChosen < amount)
    {
        // We choose a candidate at random.
        const auto& chosen = candidates[random_0_to_n(candidates.size())];

        // Create original ordering
        std::vector<unsigned> ordering(reordering.candidates[chosen].nrOfItems());
        std::iota(ordering.begin(), ordering.end(), 0);

        // Things we should write to an output file.
        Json::Value output;
        output["target_name"] = chosen.getName();
        output["file_name"] = chosen.getFileName();
        output["original"]["items"] = reordering.candidates[chosen].getJSON(ordering);// We output the original order.

        // Make sure the modified ordering isn't the same as the original
        std::vector<unsigned> original_ordering = ordering;
        do {
            std::random_shuffle(ordering.begin(), ordering.end());
        } while (original_ordering == ordering);

        // Check if this transformation isn't duplicate. If it is, we try again
        bool found = false;
        for (const auto& t : transformations) {
            if (t.target == chosen && t.ordering == ordering) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        // Debug information.
        llvm::outs() << "Chosen target: " << chosen.getName() << "\n";
        llvm::outs() << "Chosen ordering: " << "\n";
        for (auto it : ordering) {
            llvm::outs() << it << " ";
        }
        llvm::outs() << "\n";

        output["modified"]["items"] = reordering.candidates[chosen].getJSON(ordering);// We output the modified order.

        // We write some information regarding the performed transformations to output.
        writeJSONToFile(reordering.outputPrefix, amountChosen+1, "transformations.json", output);

        // Add the transformation
        transformations.emplace_back(chosen, ordering);
        amountChosen++;
    }

    // We perform the rewrite operations.
    for (unsigned long iii = 0; iii < amount; iii++)
    {
        // We select the current transformation
        reordering.transformation = &transformations[iii];

        Tool->run(new RewritingFrontendActionFactory<FPReordering, FPReorderingRewriter>(reordering, iii + 1));
    }
}
