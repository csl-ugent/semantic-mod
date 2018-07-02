#include "FPReordering.h"
#include "SemanticUtil.h"
#include "StructReordering.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <string>
#include <vector>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

// Options for semantic-mod
static cl::OptionCategory MainCategory("semantic-mod options");
static cl::opt<std::string> OutputDirectory("od", cl::cat(MainCategory));
static cl::opt<std::string> BaseDirectory("bd", cl::cat(MainCategory));
static cl::opt<unsigned> NumberOfVersions("nr_of_versions", cl::cat(MainCategory));
static cl::opt<std::string> TransformationType("transtype", cl::cat(MainCategory));
static cl::opt<unsigned> Seed("seed", cl::init((unsigned)0), cl::desc("The seed for the PRNG."), cl::cat(MainCategory));

// Entry point of our tool.
int main(int argc, const char **argv) {

    // Default options parser.
    CommonOptionsParser OptionsParser(argc, argv, MainCategory);

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

    // We determine what kind of transformation to apply.
    if (TransformationType == "StructReordering") {
        structReordering(&Tool, BaseDirectory, OutputDirectory, NumberOfVersions);

    } else if (TransformationType == "FPReordering") {
        fpreordering(&Tool, BaseDirectory, OutputDirectory, NumberOfVersions);
    }

    // Succes.
    return EXIT_SUCCESS;
}
