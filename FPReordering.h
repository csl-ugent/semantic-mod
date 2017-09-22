#ifndef _FPREORDERING
#define _FPREORDERING

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
    virtual bool VisitFunctionDecl(clang::FunctionDecl* FD);
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
    virtual bool VisitCallExpr(clang::CallExpr* CE);

    // We want to investigate FunctionDecl's.
    virtual bool VisitFunctionDecl(clang::FunctionDecl* FD);
};

// FPReordering AST Consumer.
class FPReorderingASTConsumer : public clang::ASTConsumer {
private:
    FPReorderingRewriter *visitorRewriter;
    FPReorderingAnalyser *visitorAnalysis;

    FPReordering& reordering;
    clang::Rewriter* rewriter;
    clang::CompilerInstance *CI;
    std::string baseDirectory;
    Phase::Type phaseType;
public:
    // Override the constructor in order to pass CI.
    explicit FPReorderingASTConsumer(clang::CompilerInstance *CI,
                                     Reordering& r,
                                     clang::Rewriter* rewriter,
                                     std::string baseDirectory,
                                     Phase::Type phaseType)
        : reordering(static_cast<FPReordering&>(r)),
          rewriter(rewriter),
          CI(CI),
          baseDirectory(baseDirectory),
          phaseType(phaseType)
    {
        // Visitor depends on the phase we are in.
        if (phaseType ==  Phase::Analysis) {
            visitorAnalysis = new FPReorderingAnalyser(this->CI, reordering, baseDirectory);
        } else if (phaseType == Phase::Rewrite) {
            visitorRewriter = new FPReorderingRewriter(this->CI, reordering, rewriter);
        }
    }

    // Override this to call our SemanticAnalyser on the entire source file.
    virtual void HandleTranslationUnit(clang::ASTContext &Context);
};

#endif
