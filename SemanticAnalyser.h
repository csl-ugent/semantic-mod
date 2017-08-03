#ifndef _SEMANTICANALYSER
#define _SEMANTICANALYSER

#include "SemanticData.h"

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

// Semantic analyser, willl analyse different nodes within the AST.
class SemanticAnalyser : public clang::RecursiveASTVisitor<SemanticAnalyser> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
public:
    explicit SemanticAnalyser(clang::CompilerInstance *CI,
                              SemanticData* semanticData,
                              clang::Rewriter* rewriter)
      : astContext(&(CI->getASTContext())),
        semanticData(semanticData),
        rewriter(rewriter) // Initialize private members.
    {
        rewriter->setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());
    }

    // Detection of record declarations (can be structs, unions or classes).
    virtual bool VisitRecordDecl(clang::RecordDecl *D);
};

// Semantic analyser AST consumer: will apply the semantic analysers
// to the different translation units.
class SemanticAnalyserASTConsumer : public clang::ASTConsumer {
private:
    SemanticAnalyser *visitor;
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
public:
    // Override the constructor in order to pass CI.
    explicit SemanticAnalyserASTConsumer(clang::CompilerInstance *CI,
                                         SemanticData* semanticData,
                                         clang::Rewriter* rewriter)
        : visitor(new SemanticAnalyser(CI, semanticData, rewriter)),
          semanticData(semanticData),
          rewriter(rewriter) // Initialize the visitor
    {}

    // Override this to call our SemanticAnalyser on the entire source file.
    virtual void HandleTranslationUnit(clang::ASTContext &Context);
};

// Semantic analyser frontend action: action that will start the consumer.
class SemanticAnalyserFrontendAction : public clang::ASTFrontendAction {
private:
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
public:
    explicit SemanticAnalyserFrontendAction(SemanticData* semanticData, clang::Rewriter* rewriter)
    : semanticData(semanticData),
      rewriter(rewriter)
    {}

    virtual clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file);
};

// Frontend action factory.
class SemanticAnalyserFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
private:
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
public:
    SemanticAnalyserFrontendActionFactory(SemanticData* semanticData, clang::Rewriter* rewriter)
        : semanticData(semanticData),
          rewriter(rewriter)
    {}
    clang::FrontendAction* create() override;
};

#endif
