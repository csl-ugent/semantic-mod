#ifndef _FPREORDERING
#define _FPREORDERING

#include <string>
#include <cstdlib>

#include "Semantic.h"
#include "SemanticData.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"

// Declaration of used methods.
void fpreordering(clang::Rewriter* rewriter, clang::tooling::ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings);

struct FunctionParam {
        std::string name;
        std::string type;

        FunctionParam(std::string name, std::string type) : name(name), type(type) {}
};

class FunctionUnique {
        std::string name;
        std::string fileName;
        bool global;

    public:
        FunctionUnique(const clang::FunctionDecl* FD, const clang::ASTContext& astContext)
            : name(FD->getNameAsString()), fileName(astContext.getSourceManager().getFilename(FD->getLocation()).str()), global(FD->isGlobal()) {}
        std::string getName() const { return name;}
        std::string getFileName() const { return fileName;}
        bool operator== (const FunctionUnique& function) const
        {
            // If the names differ, it can't be the same function
            if (name != function.name)
                return false;

            // If one of the two is global and the other isn't, it can't be the same function
            if (global ^ function.global)
                return false;

            // If both functions are local, yet from a different file, they are different
            if ((!global && !function.global) && (fileName != function.fileName))
                return false;

            return true;
        }
};

class FunctionData {
    public:
        FunctionUnique function;
        std::vector<FunctionParam> params;

        FunctionData(const FunctionUnique& function, const clang::FunctionDecl* FD)
            : function(function)
        {
            for (unsigned iii = 0; iii < FD->getNumParams(); iii++) {
                const clang::ParmVarDecl* param = FD->getParamDecl(iii);
                params.emplace_back(param->getNameAsString(), param->getType().getAsString());
            }
        }
};

// Struct which applies the function reordering.
struct FPTransformation {
    FunctionUnique function;
    std::vector<unsigned> ordering;
    FPTransformation(FunctionUnique function, std::vector<unsigned>& ordering)
        : function(function), ordering(ordering) {}
};

// Function parameter reordering semantic modification.
class FPReordering : public Reordering {
public:
    explicit FPReordering() {}

    // Map containing all information regarding different functions.
    std::vector<FunctionData> candidates;

    // The transformation to apply
    FPTransformation* transformation;
};

// Semantic analyser, willl analyse different nodes within the AST.
class FPReorderingAnalyser : public clang::RecursiveASTVisitor<FPReorderingAnalyser> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    FPReordering& reordering;
    std::string baseDirectory;
public:
    explicit FPReorderingAnalyser(clang::CompilerInstance *CI,
                                  FPReordering& reordering,
                                  std::string baseDirectory)
      : astContext(CI->getASTContext()),
        reordering(reordering),
        baseDirectory(baseDirectory)
    { }

    // We want to investigate FunctionDecl's.
    bool VisitFunctionDecl(clang::FunctionDecl* FD);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class FPReorderingRewriter : public clang::RecursiveASTVisitor<FPReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    FPReordering& reordering;
    clang::Rewriter* rewriter;
public:
    explicit FPReorderingRewriter(clang::CompilerInstance *CI,
                                  FPReordering& reordering,
                                  clang::Rewriter* rewriter)
      : astContext(CI->getASTContext()),
        reordering(reordering),
        rewriter(rewriter) // Initialize private members.
    {
        rewriter->setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We need to rewrite calls to these reorered functions.
    bool VisitCallExpr(clang::CallExpr* CE);

    // We want to investigate FunctionDecl's.
    bool VisitFunctionDecl(clang::FunctionDecl* FD);
};

// AST Consumer
typedef ReorderingASTConsumer<FPReordering, FPReorderingAnalyser, FPReorderingRewriter> FPReorderingASTConsumer;

#endif
