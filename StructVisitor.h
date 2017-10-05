#include "SemanticFrontendAction.h"

#include "clang/AST/Decl.h"

// Semantic analyser, willl analyse different nodes within the AST.
class StructReorderingAnalyser : public clang::RecursiveASTVisitor<StructReorderingAnalyser> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    StructReordering& reordering;
public:
    explicit StructReorderingAnalyser(clang::ASTContext& Context, StructReordering& reordering)
      : astContext(Context), reordering(reordering) { }

    // We want to investigate all possible struct declarations and uses
    void detectStructsRecursively(const clang::Type* origType);
    bool VisitRecordDecl(clang::RecordDecl* D);
    bool VisitVarDecl(clang::VarDecl* D);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class StructReorderingRewriter : public clang::RecursiveASTVisitor<StructReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    StructReordering& reordering;
    clang::Rewriter& rewriter;
public:
    explicit StructReorderingRewriter(clang::ASTContext& Context, StructReordering& reordering, clang::Rewriter& rewriter)
      : astContext(Context), reordering(reordering), rewriter(rewriter)
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We want to investigate top-level things.
    bool VisitRecordDecl(clang::RecordDecl* D);
};
