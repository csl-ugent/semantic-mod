#ifndef _SEMANTICANALYSER
#define _SEMANTICANALYSER

#include <string>
#include <sstream>
#include <cstdlib>
#include <sstream>
#include <fstream>

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

std::string stmt2str(clang::Stmt *stm, clang::SourceManager* sm, const clang::LangOptions* lopt);

// Semantic analyser, willl analyse different nodes within the AST.
class SemanticAnalyser : public clang::RecursiveASTVisitor<SemanticAnalyser> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    SemanticData* semanticData;
    std::string baseDirectory;
public:
    explicit SemanticAnalyser(clang::CompilerInstance *CI,
                              SemanticData* semanticData,
                              std::string baseDirectory)
      : astContext(&(CI->getASTContext())),
        semanticData(semanticData), // Initialize private members.
        baseDirectory(baseDirectory)
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

    // Detection of variable declarations.
    virtual bool VisitDeclStmt(clang::DeclStmt *D);
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
    std::string baseDirectory;
public:
    // Override the constructor in order to pass CI.
    explicit SemanticAnalyserASTConsumer(clang::CompilerInstance *CI,
                                         SemanticData* semanticData,
                                         clang::Rewriter* rewriter,
                                         bool analysis,
                                         std::string baseDirectory)
        : semanticData(semanticData),
          rewriter(rewriter), // Initialize the visitor
          analysis(analysis),
          CI(CI),
          baseDirectory(baseDirectory)
    {
        // Visitor depends on the fact we are analysing or not.
        if (this->analysis) {
            visitorAnalysis = new SemanticAnalyser(this->CI, semanticData, baseDirectory);
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
    int version;
    std::string outputPrefix;
    std::string baseDirectory;
public:
    explicit SemanticAnalyserFrontendAction(SemanticData* semanticData, clang::Rewriter* rewriter, bool analysis, int version, std::string outputPrefix, std::string baseDirectory)
    : semanticData(semanticData),
      rewriter(rewriter),
      analysis(analysis),
      version(version),
      outputPrefix(outputPrefix),
      baseDirectory(baseDirectory)
    {}

    virtual clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file);
    virtual void EndSourceFileAction();
    virtual void writeChangesToOutput();
};

// Frontend action factory.
class SemanticAnalyserFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
private:
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
    bool analysis;
    int version;
    std::string outputPrefix;
    std::string baseDirectory;
public:
    SemanticAnalyserFrontendActionFactory(SemanticData* semanticData, clang::Rewriter* rewriter, bool analysis, std::string baseDirectory, int version = 0, std::string outputPrefix = "")
        : semanticData(semanticData),
          rewriter(rewriter),
          analysis(analysis),
          version(version),
          outputPrefix(outputPrefix),
          baseDirectory(baseDirectory)
    {}
    clang::FrontendAction* create() override;
};

#endif
