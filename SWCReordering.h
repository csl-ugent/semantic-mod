#ifndef _SWCREORDERING
#define _SWCREORDERING

#include <string>
#include <sstream>
#include <cstdlib>
#include <sstream>
#include <fstream>

#include "Semantic.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/ASTConsumer.h"
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

// SWCReordering AST Consumer.
class SWCReorderingASTConsumer : public clang::ASTConsumer {
private:
    SWCReorderingRewriter *visitorRewriter;
    SWCReorderingAnalyser *visitorAnalysis;

    SWCReordering& reordering;
    clang::Rewriter* rewriter;
    clang::CompilerInstance *CI;
    std::string baseDirectory;
    Phase::Type phaseType;
public:
    // Override the constructor in order to pass CI.
    explicit SWCReorderingASTConsumer(clang::CompilerInstance *CI,
                                     Reordering& r,
                                     clang::Rewriter* rewriter,
                                     std::string baseDirectory,
                                     Phase::Type phaseType)
        : reordering(static_cast<SWCReordering&>(r)),
          rewriter(rewriter),
          CI(CI),
          baseDirectory(baseDirectory),
          phaseType(phaseType)
    {
        // Visitor depends on the phase we are in.
        if (phaseType ==  Phase::Analysis) {
            visitorAnalysis = new SWCReorderingAnalyser(this->CI, reordering, baseDirectory);
        } else if (phaseType == Phase::Rewrite) {
            visitorRewriter = new SWCReorderingRewriter(this->CI, reordering, rewriter);
        }
    }

    // Override this to call our SemanticAnalyser on the entire source file.
    void HandleTranslationUnit(clang::ASTContext &Context);
};

#endif
