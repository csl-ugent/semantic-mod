#ifndef _FPREORDERING
#define _FPREORDERING

#include <string>
#include <sstream>
#include <cstdlib>
#include <sstream>
#include <fstream>

#include "Semantic.h"
#include "SemanticData.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/LangOptions.h"

// Declaration of used methods.
void fpreordering(clang::Rewriter* rewriter, clang::tooling::ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings);

// Semantic analyser, willl analyse different nodes within the AST.
class FPReorderingAnalyser : public clang::RecursiveASTVisitor<FPReorderingAnalyser> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    FPReordering& reordering;
    std::string baseDirectory;
public:
    explicit FPReorderingAnalyser(clang::CompilerInstance *CI,
                                  FPReordering& reordering,
                                  std::string baseDirectory)
      : astContext(&(CI->getASTContext())),
        reordering(reordering),
        baseDirectory(baseDirectory)
    { }

    // We want to investigate FunctionDecl's.
    bool VisitFunctionDecl(clang::FunctionDecl* FD);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class FPReorderingRewriter : public clang::RecursiveASTVisitor<FPReorderingRewriter> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    FPReordering& reordering;
    clang::Rewriter* rewriter;
public:
    explicit FPReorderingRewriter(clang::CompilerInstance *CI,
                                  FPReordering& reordering,
                                  clang::Rewriter* rewriter)
      : astContext(&(CI->getASTContext())),
        reordering(reordering),
        rewriter(rewriter) // Initialize private members.
    {
        rewriter->setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());
    }

    // We need to rewrite calls to these reorered functions.
    bool VisitCallExpr(clang::CallExpr* CE);

    // We want to investigate FunctionDecl's.
    bool VisitFunctionDecl(clang::FunctionDecl* FD);
};

// AST Consumer
typedef ReorderingASTConsumer<FPReordering, FPReorderingAnalyser, FPReorderingRewriter> FPReorderingASTConsumer;

#endif
