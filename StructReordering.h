#ifndef _STRUCTREORDERING
#define _STRUCTREORDERING

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
void structReordering(clang::Rewriter* rewriter, clang::tooling::ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings);

// Semantic analyser, willl analyse different nodes within the AST.
class StructReorderingAnalyser : public clang::RecursiveASTVisitor<StructReorderingAnalyser> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    StructReordering& reordering;
    std::string baseDirectory;
public:
    explicit StructReorderingAnalyser(clang::CompilerInstance *CI,
                              StructReordering& reordering,
                              std::string baseDirectory)
      : astContext(&(CI->getASTContext())),
        reordering(reordering), // Initialize private members.
        baseDirectory(baseDirectory)
    { }

    // We want to investigate top-level things.
    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl* TD);
};


// Semantic post transformation analyser.
class StructReorderingPreTransformationAnalysis : public clang::RecursiveASTVisitor<StructReorderingPreTransformationAnalysis> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    StructReordering& reordering;
public:
    explicit StructReorderingPreTransformationAnalysis(clang::CompilerInstance *CI,
                                                    StructReordering& reordering)
      : astContext(&(CI->getASTContext())),
        reordering(reordering)
    { }

    // Method used to detect struct types recursively and remove
    // them from the struct map.
    void detectStructsRecursively(const clang::Type* type, StructReordering& reordering);

    // We want to investigate top-level things.
    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl* TD);

    // We want to investigate (local) declaration statements.
    bool VisitDeclStmt(clang::DeclStmt *DeclStmt);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class StructReorderingRewriter : public clang::RecursiveASTVisitor<StructReorderingRewriter> {
private:
    clang::ASTContext *astContext; // Used for getting additional AST info.
    StructReordering& reordering;
    clang::Rewriter* rewriter;
public:
    explicit StructReorderingRewriter(clang::CompilerInstance *CI,
                              StructReordering& reordering,
                              clang::Rewriter* rewriter)
      : astContext(&(CI->getASTContext())),
        reordering(reordering),
        rewriter(rewriter) // Initialize private members.
    {
        rewriter->setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());
    }

    // We want to investigate top-level things.
    bool VisitTranslationUnitDecl(clang::TranslationUnitDecl* TD);
};

// StructReordering AST Consumer.
class StructReorderingASTConsumer : public clang::ASTConsumer {
private:
    StructReorderingRewriter *visitorRewriter;
    StructReorderingAnalyser *visitorAnalysis;
    StructReorderingPreTransformationAnalysis *visitorPreTransformationAnalysis;

    StructReordering& reordering;
    clang::Rewriter* rewriter;
    clang::CompilerInstance *CI;
    std::string baseDirectory;
    Phase::Type phaseType;
public:
    // Override the constructor in order to pass CI.
    explicit StructReorderingASTConsumer(clang::CompilerInstance *CI,
                                         Reordering& r,
                                         clang::Rewriter* rewriter,
                                         std::string baseDirectory,
                                         Phase::Type phaseType)
        : reordering(static_cast<StructReordering&>(r)),
          rewriter(rewriter),
          CI(CI),
          baseDirectory(baseDirectory),
          phaseType(phaseType)
    {
        // Visitor depends on the phase we are in.
        if (phaseType ==  Phase::Analysis) {
            visitorAnalysis = new StructReorderingAnalyser(this->CI, reordering, baseDirectory);
        } else if (phaseType == Phase::Rewrite) {
            visitorRewriter = new StructReorderingRewriter(this->CI, reordering, rewriter);
        } else if (phaseType == Phase::PreTransformationAnalysis) {
            visitorPreTransformationAnalysis = new StructReorderingPreTransformationAnalysis(this->CI, reordering);
        }
    }

    // Override this to call our SemanticAnalyser on the entire source file.
    void HandleTranslationUnit(clang::ASTContext &Context);
};

#endif
