#ifndef _SWCREORDERING
#define _SWCREORDERING

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
void swcreordering(clang::Rewriter* rewriter, clang::tooling::ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings);

// Semantic analyser, willl analyse different nodes within the AST.
class SWCReorderingAnalyser : public clang::RecursiveASTVisitor<SWCReorderingAnalyser> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    SWCReordering& reordering;
    std::string baseDirectory;
public:
    explicit SWCReorderingAnalyser(clang::CompilerInstance *CI,
                                  SWCReordering& reordering,
                                  std::string baseDirectory)
      : astContext(&(CI->getASTContext())),
        reordering(reordering),
        baseDirectory(baseDirectory)
    { }

    // We want to investigate switch statements.
    bool VisitSwitchStmt(clang::SwitchStmt* CS);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class SWCReorderingRewriter : public clang::RecursiveASTVisitor<SWCReorderingRewriter> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    SWCReordering& reordering;
    clang::Rewriter* rewriter;
public:
    explicit SWCReorderingRewriter(clang::CompilerInstance *CI,
                                   SWCReordering& reordering,
                                   clang::Rewriter* rewriter)
      : astContext(&(CI->getASTContext())),
        reordering(reordering),
        rewriter(rewriter) // Initialize private members.
    {
        rewriter->setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());
    }

    // We want to rewrite switch statements.
    bool VisitSwitchStmt(clang::SwitchStmt* CS);
};

// AST Consumer
typedef ReorderingASTConsumer<SWCReordering, SWCReorderingAnalyser, SWCReorderingRewriter> SWCReorderingASTConsumer;

#endif
