#ifndef _SEMANTIC
#define _SEMANTIC

#include <string>
#include <sstream>
#include <cstdlib>
#include <sstream>
#include <fstream>

#include "SemanticData.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
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

// Enumeration of types of transformations.
namespace Transformation {
    enum Type {
        StructReordering, FPReordering, SWCReordering
    };
}

// Phases of an ASTConsumer.
namespace Phase {
    enum Type {
        Analysis, PreTransformationAnalysis, Rewrite
    };
}

// Default version of the pre-transformation analyser, for those reordering's that don't have any
class DefaultPreTransformationAnalysis : public clang::RecursiveASTVisitor<DefaultPreTransformationAnalysis> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    Reordering& reordering;
public:
    explicit DefaultPreTransformationAnalysis(clang::CompilerInstance *CI,
                                                    Reordering& reordering)
      : astContext(&(CI->getASTContext())),
        reordering(reordering)
    { }
};

// Reordering ASTConsumer
template <typename ReorderingType, typename AnalyserType,  typename RewriterType, typename PreTransformationAnalyserType = DefaultPreTransformationAnalysis>
class ReorderingASTConsumer : public clang::ASTConsumer {
private:
    ReorderingType& reordering;
    clang::Rewriter* rewriter;
    clang::CompilerInstance *CI;
    std::string baseDirectory;
    Phase::Type phaseType;
public:
    // Override the constructor in order to pass CI.
    explicit ReorderingASTConsumer(clang::CompilerInstance *CI,
                                     Reordering& r,
                                     clang::Rewriter* rewriter,
                                     std::string baseDirectory,
                                     Phase::Type phaseType)
        : reordering(static_cast<ReorderingType&>(r)),
          rewriter(rewriter),
          CI(CI),
          baseDirectory(baseDirectory),
          phaseType(phaseType)
    {
    }

    // Override this to call our SemanticAnalyser on the entire source file.
    void HandleTranslationUnit(clang::ASTContext &Context) {
         // Visitor depends on the phase we are in.
        switch(phaseType) {
            case Phase::Analysis:
                {
                    AnalyserType visitor = AnalyserType(this->CI, reordering, baseDirectory);
                    visitor.TraverseDecl(Context.getTranslationUnitDecl());
                    break;
                }
            case Phase::PreTransformationAnalysis:
                {
                    PreTransformationAnalyserType visitor = PreTransformationAnalyserType(this->CI, reordering);
                    visitor.TraverseDecl(Context.getTranslationUnitDecl());
                    break;
                }
            case Phase::Rewrite:
                {
                    RewriterType visitor = RewriterType(this->CI, reordering, rewriter);
                    visitor.TraverseDecl(Context.getTranslationUnitDecl());
                    break;
                }
        }
    }
};

// General utility functions.
std::string location2str(clang::SourceLocation begin, clang::SourceLocation end, clang::SourceManager* sm, const clang::LangOptions* lopt);

// Semantic analyser frontend action: action that will start the consumer.
class SemanticFrontendAction : public clang::ASTFrontendAction {
private:
    Reordering& reordering;
    clang::Rewriter* rewriter;
    int version;
    std::string outputPrefix;
    std::string baseDirectory;
    Transformation::Type transType;
    Phase::Type phaseType;
public:
    explicit SemanticFrontendAction(Reordering& reordering, clang::Rewriter* rewriter, int version, std::string outputPrefix, std::string baseDirectory, Transformation::Type transType, Phase::Type phaseType)
    : reordering(reordering),
      rewriter(rewriter),
      version(version),
      outputPrefix(outputPrefix),
      baseDirectory(baseDirectory),
      transType(transType),
      phaseType(phaseType)
    {}

    clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef file);
    void EndSourceFileAction();
    void writeChangesToOutput();
};

// Frontend action factory.
class SemanticFrontendActionFactory : public clang::tooling::FrontendActionFactory
{
private:
    Reordering& reordering;
    clang::Rewriter* rewriter;
    std::string baseDirectory;
    Transformation::Type transType;
    Phase::Type phaseType;
    int version;
    std::string outputPrefix;

public:
    SemanticFrontendActionFactory(Reordering& reordering, clang::Rewriter* rewriter, std::string baseDirectory, Transformation::Type transType, Phase::Type phaseType, int version = 0, std::string outputPrefix = "")
        : reordering(reordering),
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
