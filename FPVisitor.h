#include "SemanticFrontendAction.h"

#include "clang/AST/Decl.h"

// Semantic analyser, willl analyse different nodes within the AST.
class FPAnalyser : public clang::RecursiveASTVisitor<FPAnalyser> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    FPVersion& version;
public:
    explicit FPAnalyser(clang::ASTContext& Context, FPVersion& version)
      : astContext(Context), version(version) { }

    // We want to investigate Function declarations and invocations
    bool VisitBinaryOperator(clang::BinaryOperator* DRE);
    bool VisitCallExpr(clang::CallExpr* CE);
    bool VisitFunctionDecl(clang::FunctionDecl* D);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class FPReorderingRewriter : public clang::RecursiveASTVisitor<FPReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    FPVersion& version;
    clang::Rewriter& rewriter;
public:
    explicit FPReorderingRewriter(clang::ASTContext& Context, FPVersion& version, clang::Rewriter& rewriter)
      : astContext(Context), version(version), rewriter(rewriter)
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We need to rewrite calls to these reordered functions.
    bool VisitCallExpr(clang::CallExpr* CE);

    // We want to investigate FunctionDecl's.
    bool VisitFunctionDecl(clang::FunctionDecl* D);
};
