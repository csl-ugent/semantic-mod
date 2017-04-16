#ifndef _SEMANTIC
#define _SEMANTIC

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

// Enumeration of types of transformations.
namespace Transformation {
    enum Type {
        StructReordering
    };
}

// Phases of an ASTConsumer.
namespace Phase {
    enum Type {
        Analysis, PreTransformationAnalysis, Rewrite
    };
}

// General utility functions.
std::string location2str(clang::SourceLocation begin, clang::SourceLocation end, clang::SourceManager* sm, const clang::LangOptions* lopt);

// Semantic analyser frontend action: action that will start the consumer.
class SemanticFrontendAction : public clang::ASTFrontendAction {
private:
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
    int version;
    std::string outputPrefix;
    std::string baseDirectory;
    Transformation::Type transType;
    Phase::Type phaseType;
public:
    explicit SemanticFrontendAction(SemanticData* semanticData, clang::Rewriter* rewriter, int version, std::string outputPrefix, std::string baseDirectory, Transformation::Type transType, Phase::Type phaseType)
    : semanticData(semanticData),
      rewriter(rewriter),
      version(version),
      outputPrefix(outputPrefix),
      baseDirectory(baseDirectory),
      transType(transType),
      phaseType(phaseType)
    {}

    virtual clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file);
    virtual void EndSourceFileAction();
    virtual void writeChangesToOutput();
};

// Frontend action factory.
class SemanticFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
private:
    SemanticData* semanticData;
    clang::Rewriter* rewriter;
    std::string baseDirectory;
    Transformation::Type transType;
    Phase::Type phaseType;
    int version;
    std::string outputPrefix;

public:
    SemanticFrontendActionFactory(SemanticData* semanticData, clang::Rewriter* rewriter, std::string baseDirectory, Transformation::Type transType, Phase::Type phaseType, int version = 0, std::string outputPrefix = "")
        : semanticData(semanticData),
          rewriter(rewriter),
          baseDirectory(baseDirectory),
          transType(transType),
          phaseType(phaseType),
          version(version),
          outputPrefix(outputPrefix)

    {}
    clang::FrontendAction* create();
};

#endif
