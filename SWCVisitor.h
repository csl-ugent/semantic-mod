#include "SemanticFrontendAction.h"

#include "clang/AST/Decl.h"

// Semantic analyser, willl analyse different nodes within the AST.
class SWCReorderingAnalyser : public clang::RecursiveASTVisitor<SWCReorderingAnalyser> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    SWCReordering& reordering;
public:
    explicit SWCReorderingAnalyser(clang::ASTContext& Context, SWCReordering& reordering)
      : astContext(Context), reordering(reordering) { }

    // We want to investigate switch statements.
    bool VisitSwitchStmt(clang::SwitchStmt* CS);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class SWCReorderingRewriter : public clang::RecursiveASTVisitor<SWCReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    SWCReordering& reordering;
    clang::Rewriter& rewriter;
public:
    explicit SWCReorderingRewriter(clang::ASTContext& Context, SWCReordering& reordering, clang::Rewriter& rewriter)
      : astContext(Context), reordering(reordering), rewriter(rewriter)
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We want to rewrite switch statements.
    bool VisitSwitchStmt(clang::SwitchStmt* CS);
};
