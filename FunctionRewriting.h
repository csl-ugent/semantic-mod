#ifndef _FPREORDERING
#define _FPREORDERING

#include "SemanticData.h"
#include "SemanticVisitors.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include <string>
#include <vector>

class FunctionUnique : public TargetUnique {
    public:
        FunctionUnique(const clang::FunctionDecl* D, const clang::ASTContext& astContext)
            : TargetUnique(D->getNameAsString(), astContext.getSourceManager().getFilename(D->getLocation()).str(), D->isGlobal()) {}

        class Data : public TargetUnique::Data {
            struct FunctionParam {
                std::string name;
                std::string type;

                FunctionParam(std::string name, std::string type) : name(name), type(type) {}
            };

            public:
            std::vector<FunctionParam> params;

            Data(bool valid = true) : TargetUnique::Data(valid) {}
            void addParams(clang::FunctionDecl* D)
            {
                for (unsigned iii = 0; iii < D->getNumParams(); iii++) {
                    const clang::ParmVarDecl* param = D->getParamDecl(iii);
                    params.emplace_back(param->getNameAsString(), param->getType().getAsString());
                }
            }
            bool empty() const {
                return params.empty();
            }
            Json::Value getJSON(const std::vector<unsigned>& ordering) const {
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
            unsigned nrOfItems() const {
                return params.size();
            }
        };

        // Semantic analyser, willl analyse different nodes within the AST.
        class Analyser : public SemanticAnalyser<FunctionUnique>, public clang::RecursiveASTVisitor<Analyser> {
            public:
                explicit Analyser(clang::ASTContext& Context, const MetaData& metadata, Candidates<FunctionUnique>& candidates)
                    : SemanticAnalyser(Context, metadata, candidates) {}

                // We want to investigate Function declarations and invocations
                bool VisitBinaryOperator(clang::BinaryOperator* DRE);
                bool VisitCallExpr(clang::CallExpr* CE);
                bool VisitFunctionDecl(clang::FunctionDecl* D);
        };
};

// Semantic Rewriter, will rewrite source code based on the AST.
class FPReorderingRewriter : public SemanticRewriter, public clang::RecursiveASTVisitor<FPReorderingRewriter> {
    private:
        const ReorderingTransformation& transformation;
    public:
        explicit FPReorderingRewriter(clang::ASTContext& Context, const Transformation& transformation, clang::Rewriter& rewriter)
          : SemanticRewriter(Context, rewriter), transformation(static_cast<const ReorderingTransformation&>(transformation)) {}

        // We need to rewrite calls to these reordered functions.
        bool VisitCallExpr(clang::CallExpr* CE);

        // We want to investigate FunctionDecl's.
        bool VisitFunctionDecl(clang::FunctionDecl* D);

        typedef FunctionUnique Target;
        typedef ReorderingTransformation TransformationType;
};

// Semantic Rewriter, will rewrite source code based on the AST.
class FPInsertionRewriter : public SemanticRewriter, public clang::RecursiveASTVisitor<FPInsertionRewriter> {
    private:
        const InsertionTransformation& transformation;
    public:
        explicit FPInsertionRewriter(clang::ASTContext& Context, const Transformation& transformation, clang::Rewriter& rewriter)
          : SemanticRewriter(Context, rewriter), transformation(static_cast<const InsertionTransformation&>(transformation)) {}

        // We need to rewrite calls to these reordered functions.
        bool VisitCallExpr(clang::CallExpr* CE);

        // We want to investigate FunctionDecl's.
        bool VisitFunctionDecl(clang::FunctionDecl* D);

        typedef FunctionUnique Target;
        typedef InsertionTransformation TransformationType;
};

#endif
