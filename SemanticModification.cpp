#include <cstdio>
#include <memory>
#include <string>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <set>
#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "json.h"
#include "Semantic.h"
#include "StructReordering.h"
#include "SWCReordering.h"
#include "FPReordering.h"
#include "SemanticUtil.h"

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
// Matcher imports.
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
// ASTContext.
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
// Rewriter
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "llvm/Support/CommandLine.h"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

// Extra output option.
static cl::opt<std::string> OutputDirectory("od");
static cl::opt<std::string> BaseDirectory("bd");
static cl::opt<unsigned> NumberOfVersions("nr_of_versions");
static cl::opt<std::string> TransformationType("transtype");
static cl::opt<unsigned> Seed("seed", cl::init((unsigned)0), cl::desc("The seed for the PRNG."));

// Entry point of our tool.
int main(int argc, const char **argv) {

    // Default options parser.
    CommonOptionsParser OptionsParser(argc, argv);

    // If the BaseDirectory path doesn't have a trailing slash, add one
    if (*BaseDirectory.rbegin() != '/')
      BaseDirectory.append("/");

    // If the OutputDirectory path doesn't have a trailing slash, add one
    if (*OutputDirectory.rbegin() != '/')
      OutputDirectory.append("/");

    // Initialize random seed.
    init_random(Seed);

    // Retrieve source path list from options parser.
    const std::vector<std::string> srcPathList = OptionsParser.getSourcePathList();

    // Initialize the Tool.
    ClangTool Tool(OptionsParser.getCompilations(), srcPathList);

    // The rewriter that will be used for source-to-source transformations.
    Rewriter rewriter;

    // We determine what kind of transformation to apply.
    if (TransformationType == "StructReordering") {

        // We start the structreordering transformation.
        structReordering(&rewriter, &Tool, BaseDirectory, OutputDirectory, NumberOfVersions);

    } else if (TransformationType == "FPReordering") {

        // We start the function parameter reordering transformation.
        fpreordering(&rewriter, &Tool, BaseDirectory, OutputDirectory, NumberOfVersions);
    } else if (TransformationType == "SWCReordering") {

        // We start the switch case reordering transformation.
        swcreordering(&rewriter, &Tool, BaseDirectory, OutputDirectory, NumberOfVersions);
    }

    // Succes.
    return EXIT_SUCCESS;
}
