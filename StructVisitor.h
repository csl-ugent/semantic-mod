#include "SemanticFrontendAction.h"

#include "clang/AST/Decl.h"

// Semantic analyser, willl analyse different nodes within the AST.
class StructAnalyser : public clang::RecursiveASTVisitor<StructAnalyser> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    StructVersion& version;
public:
    explicit StructAnalyser(clang::ASTContext& Context, StructVersion& version)
      : astContext(Context), version(version) { }

    // We want to investigate all possible struct declarations and uses
    void detectStructsRecursively(const clang::Type* origType);
    bool VisitRecordDecl(clang::RecordDecl* D);
    bool VisitVarDecl(clang::VarDecl* D);

    typedef StructUnique Target;
};

// Semantic Rewriter, will rewrite source code based on the AST.
class StructReorderingRewriter : public clang::RecursiveASTVisitor<StructReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    StructVersion& version;
    clang::Rewriter& rewriter;
public:
    explicit StructReorderingRewriter(clang::ASTContext& Context, StructVersion& version, clang::Rewriter& rewriter)
      : astContext(Context), version(version), rewriter(rewriter)
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We want to investigate top-level things.
    bool VisitRecordDecl(clang::RecordDecl* D);

    typedef StructAnalyser Analyser;
};
