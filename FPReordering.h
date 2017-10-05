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

#include "json.h"

#include <cstdlib>
#include <string>

// Declaration of used methods.
void fpreordering(clang::tooling::ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, const unsigned long amountOfReorderings);

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

class FunctionData {
    struct FunctionParam {
        std::string name;
        std::string type;

        FunctionParam(std::string name, std::string type) : name(name), type(type) {}
    };

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
        Json::Value getJSON(const std::vector<unsigned>& ordering) {
            Json::Value items(Json::arrayValue);
            for (unsigned iii = 0; iii < params.size(); iii++) {
                const FunctionParam& param = params[ordering[iii]];
                Json::Value v;
                v["position"] = iii;
                v["name"] = param.name;
                v["type"] = param.type;
                items.append(v);
            }
            return items;
        }
        unsigned nrOfItems() {
            return params.size();
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
    explicit FPReordering(const std::string& bd, const std::string& od) : Reordering(bd, od) {}

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
    explicit FPReorderingAnalyser(clang::ASTContext& Context, FPReordering& reordering)
      : astContext(Context), reordering(reordering) { }

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
    explicit FPReorderingRewriter(clang::ASTContext& Context, FPReordering& reordering, clang::Rewriter& rewriter)
      : astContext(Context), reordering(reordering), rewriter(rewriter)
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We need to rewrite calls to these reorered functions.
    bool VisitCallExpr(clang::CallExpr* CE);

    // We want to investigate FunctionDecl's.
    bool VisitFunctionDecl(clang::FunctionDecl* D);
};

#endif
