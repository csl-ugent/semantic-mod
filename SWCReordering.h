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
void swcreordering(SemanticData* semanticData, clang::Rewriter* rewriter, clang::tooling::ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings);

// Semantic analyser, willl analyse different nodes within the AST.
class SWCReorderingAnalyser : public clang::RecursiveASTVisitor<SWCReorderingAnalyser> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    SemanticData* semanticData;
    std::string baseDirectory;
public:
    explicit SWCReorderingAnalyser(clang::CompilerInstance *CI,
                                  SemanticData* semanticData,
                                  std::string baseDirectory)
      : astContext(&(CI->getASTContext())),
        semanticData(semanticData),
        baseDirectory(baseDirectory)
    { }

    // We want to investigate switch statements.
    virtual bool VisitSwitchStmt(clang::SwitchStmt* CS);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class SWCReorderingRewriter : public clang::RecursiveASTVisitor<SWCReorderingRewriter> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
public:
    explicit SWCReorderingRewriter(clang::CompilerInstance *CI,
                                   SemanticData* semanticData,
                                   clang::Rewriter* rewriter)
      : astContext(&(CI->getASTContext())),
        semanticData(semanticData),
        rewriter(rewriter) // Initialize private members.
    {
        rewriter->setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());
    }

    // We want to rewrite switch statements.
    virtual bool VisitSwitchStmt(clang::SwitchStmt* CS);
};

// SWCReordering AST Consumer.
class SWCReorderingASTConsumer : public clang::ASTConsumer {
private:
    SWCReorderingRewriter *visitorRewriter;
    SWCReorderingAnalyser *visitorAnalysis;

    SemanticData* semanticData;
    clang::Rewriter* rewriter;
    clang::CompilerInstance *CI;
    std::string baseDirectory;
    Phase::Type phaseType;
public:
    // Override the constructor in order to pass CI.
    explicit SWCReorderingASTConsumer(clang::CompilerInstance *CI,
                                     SemanticData* semanticData,
                                     clang::Rewriter* rewriter,
                                     std::string baseDirectory,
                                     Phase::Type phaseType)
        : semanticData(semanticData),
          rewriter(rewriter),
          CI(CI),
          baseDirectory(baseDirectory),
          phaseType(phaseType)
    {
        // Visitor depends on the phase we are in.
        if (phaseType ==  Phase::Analysis) {
            visitorAnalysis = new SWCReorderingAnalyser(this->CI, semanticData, baseDirectory);
        } else if (phaseType == Phase::Rewrite) {
            visitorRewriter = new SWCReorderingRewriter(this->CI, semanticData, rewriter);
        }
    }

    // Override this to call our SemanticAnalyser on the entire source file.
    virtual void HandleTranslationUnit(clang::ASTContext &Context);
};

#endif
