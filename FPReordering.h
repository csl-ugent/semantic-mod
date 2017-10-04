#ifndef _FPREORDERING
#define _FPREORDERING

#include "Semantic.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/MapVector.h"

#include <cstdlib>
#include <string>

// Declaration of used methods.
void fpreordering(clang::tooling::ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings);

class FunctionUnique {
        std::string name;
        std::string fileName;
        bool global;

    public:
        FunctionUnique(const clang::FunctionDecl* D, const clang::ASTContext& astContext)
            : name(D->getNameAsString()), fileName(astContext.getSourceManager().getFilename(D->getLocation()).str()), global(D->isGlobal()) {}
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
        bool operator< (const FunctionUnique& function) const
        {
            // Create string representation of the functions so we can correctly compare
            std::string left = global ? name : name + ":" + fileName;
            std::string right = function.global ? function.name : function.name + ":" + function.fileName;

            return left < right;
        }
};

struct FunctionParam {
        std::string name;
        std::string type;

        FunctionParam(std::string name, std::string type) : name(name), type(type) {}
};

class FunctionData {
    public:
        bool valid;
        std::vector<FunctionParam> params;

        FunctionData(bool valid = true) : valid(valid) {}
        void addParams(clang::FunctionDecl* D)
        {
            for (unsigned iii = 0; iii < D->getNumParams(); iii++) {
                const clang::ParmVarDecl* param = D->getParamDecl(iii);
                params.emplace_back(param->getNameAsString(), param->getType().getAsString());
            }
        }
        bool empty() {
          return params.empty();
        }
};

// Struct which describes the transformation
struct FPTransformation {
    FunctionUnique target;
    std::vector<unsigned> ordering;
    FPTransformation(FunctionUnique target, std::vector<unsigned>& ordering)
        : target(target), ordering(ordering) {}
};

// Function parameter reordering semantic modification.
class FPReordering : public Reordering {
public:
    explicit FPReordering(const std::string& bd) : Reordering(bd) {}

    // Map containing all information regarding different functions.
    llvm::MapVector<FunctionUnique, FunctionData, std::map<FunctionUnique, unsigned>> candidates;
    void invalidateCandidate(const FunctionUnique& candidate, const std::string& reason) {
        llvm::outs() << "Invalidate candidate: " << candidate.getName() << ". Reason: " << reason << ".\n";
        FunctionData& data = candidates[candidate];
        data.valid = false;
    }

    // The transformation to apply
    FPTransformation* transformation;
};

// Semantic analyser, willl analyse different nodes within the AST.
class FPReorderingAnalyser : public clang::RecursiveASTVisitor<FPReorderingAnalyser> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    FPReordering& reordering;
public:
    explicit FPReorderingAnalyser(clang::CompilerInstance *CI, FPReordering& reordering)
      : astContext(CI->getASTContext()), reordering(reordering) { }

    // We want to investigate Function declarations and invocations
    bool VisitBinaryOperator(clang::BinaryOperator* DRE);
    bool VisitCallExpr(clang::CallExpr* CE);
    bool VisitFunctionDecl(clang::FunctionDecl* D);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class FPReorderingRewriter : public clang::RecursiveASTVisitor<FPReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    FPReordering& reordering;
    clang::Rewriter& rewriter;
public:
    explicit FPReorderingRewriter(clang::CompilerInstance *CI,
                                  FPReordering& reordering,
                                  clang::Rewriter& rewriter)
      : astContext(CI->getASTContext()),
        reordering(reordering),
        rewriter(rewriter) // Initialize private members.
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We need to rewrite calls to these reorered functions.
    bool VisitCallExpr(clang::CallExpr* CE);

    // We want to investigate FunctionDecl's.
    bool VisitFunctionDecl(clang::FunctionDecl* D);
};

// AST Consumer
typedef ReorderingASTConsumer<FPReordering, FPReorderingAnalyser, FPReorderingRewriter> FPReorderingASTConsumer;

#endif
