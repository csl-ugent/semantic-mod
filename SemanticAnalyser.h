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
public:
    explicit SemanticAnalyser(clang::CompilerInstance *CI,
                              SemanticData* semanticData)
      : astContext(&(CI->getASTContext())),
        semanticData(semanticData) // Initialize private members.
    { }

    // Detection of record declarations (can be structs, unions or classes).
    virtual bool VisitRecordDecl(clang::RecordDecl *D);
};


// Semantic Rewriter, will rewrite source code based on the AST.
class SemanticRewriter : public clang::RecursiveASTVisitor<SemanticRewriter> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
public:
    explicit SemanticRewriter(clang::CompilerInstance *CI,
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
    SemanticRewriter *visitorRewriter;
    SemanticAnalyser *visitorAnalysis;
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
    bool analysis;
    clang::CompilerInstance *CI;
public:
    // Override the constructor in order to pass CI.
    explicit SemanticAnalyserASTConsumer(clang::CompilerInstance *CI,
                                         SemanticData* semanticData,
                                         clang::Rewriter* rewriter,
                                         bool analysis)
        : semanticData(semanticData),
          rewriter(rewriter), // Initialize the visitor
          analysis(analysis),
          CI(CI)
    {
        // Visitor depends on the fact we are analysing or not.
        if (this->analysis) {
            visitorAnalysis = new SemanticAnalyser(this->CI, semanticData);
        } else {
            visitorRewriter = new SemanticRewriter(this->CI, semanticData, rewriter);
        }
    }

    // Override this to call our SemanticAnalyser on the entire source file.
    virtual void HandleTranslationUnit(clang::ASTContext &Context);
};

// Semantic analyser frontend action: action that will start the consumer.
class SemanticAnalyserFrontendAction : public clang::ASTFrontendAction {
private:
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
    bool analysis;
public:
    explicit SemanticAnalyserFrontendAction(SemanticData* semanticData, clang::Rewriter* rewriter, bool analysis)
    : semanticData(semanticData),
      rewriter(rewriter),
      analysis(analysis)
    {}

    virtual clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file);
};

// Frontend action factory.
class SemanticAnalyserFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
private:
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
    bool analysis;
public:
    SemanticAnalyserFrontendActionFactory(SemanticData* semanticData, clang::Rewriter* rewriter, bool analysis)
        : semanticData(semanticData),
          rewriter(rewriter),
          analysis(analysis)
    {}
    clang::FrontendAction* create() override;
};

#endif
