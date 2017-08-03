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

#include "SemanticAnalyser.h"
#include "SemanticData.h"
#include "util.h"

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
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
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

// Declaration of used methods.
void structReordering(SemanticData* semanticData, Rewriter* rewriter, ClangTool Tool);
void writeChangesToOutput();

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

// Extra output option.
static cl::opt<std::string> OutputDirectory("od");
static cl::opt<int> AmountOfReorderings("struct_reorder_am");

// Method which applies the structure reordering.
typedef struct structOrdering_ {
    std::string name;
    std::vector<int> chosen;
} StructOrdering;


void structReordering(SemanticData* semanticData, Rewriter* rewriter, ClangTool Tool) {

    // Debug information.
    llvm::outs() << "Struct reordering analysis...\n";

    // We determine the structs that will be reordered.
    int amount = 3;
    int amountChosen = 0;
    std::vector<StructOrdering> chosen;
    std::map<std::string, StructData*>::iterator it;
    std::map<std::string, StructData*> structMap = semanticData->getStructReordering()->getStructMap();

    // We choose the amount of configurations of this struct that is required.
    while (amountChosen < amount)
    {
        // We choose a struct at random.
        it = structMap.begin();
        std::advance(it, random_0_to_n(structMap.size()));
        StructData* chosenStruct = it->second;

        // We determine a random ordering of fields.
        std::vector<int> ordering;
        for (int i = 0; i < chosenStruct->getFieldDataSize(); i++) {
            ordering.push_back(i);
        }
        std::random_shuffle(ordering.begin(), ordering.end());

        // We check if the struc/reordering already exists.
        StructOrdering orderingStruct = StructOrdering({chosenStruct->getName(), ordering});
        if (chosen->find(orderingStruct) == chosen->end()) {

            // Retry...
            continue;
        }

        // We create new structData information.
        StructData* structData = new StructData(chosenStruct->getName());

        // We obtain all field types.
        std::vector<FieldData> fieldData = chosenStruct->getFieldData();

        // We select the fields according to the given ordering.
        std::vector<int>::iterator it;
        for (it = ordering.begin(); it != ordering.end(); ++it) {

            // The chosen field.
            FieldData chosenField = fieldData[*it];
            structData->addFieldData(chosenField.position,
                                    chosenField.fieldName,
                                    chosenField.fieldType,
                                    chosenField.sourceRange);
        }

        // We add the modified structure information to our semantic data.
        semanticData->getStructReordering()->addStructReorderingData(chosenStruct->getName(), structData);

        // We run the rewriter tool.
        int result = Tool.run(new SemanticAnalyserFrontendActionFactory(semanticData, rewriter));

        // We increase the amount of configurations we have chosen.
        // And add the ordering structure to the chosen vector.
        chosen.push_back(orderingStruct);
        amountChosen++;
    }
}

// Method which is used to write the changes
void writeChangesToOutput(Rewriter* rewriter, std::string subfolderPreix) {

    // Keep track of the subfolder.
    static int subfolder = 1;

    // Output stream.
    std::ofstream outputFile;

    // We write the results to a new location.
    for (Rewriter::buffer_iterator I = rewriter->buffer_begin(), E = rewriter->buffer_end(); I != E; ++I) {

        // Get the file name.
        StringRef fileNameRef = rewriter->getSourceMgr().getFileEntryForID(I->first)->getName();
        std::string fileName = std::string(fileNameRef.data());
        fileName = fileName.substr(fileName.find_last_of("/\\") + 1); /* until the end automatically... */

        llvm::outs() << "Filename: " << fileName << "\n";

        // Write changes to the file.
        outputFile.open((OutputDirectory + "/" + fileName).c_str());
        std::string output = std::string(I->second.begin(), I->second.end());
        outputFile.write(output.c_str(), output.length());
        outputFile.close();
    }
}

// Entry point of our tool.
int main(int argc, const char **argv) {

    // Default options parser.
    CommonOptionsParser OptionsParser(argc, argv);

    // Retrieve source path list from options parser.
    const std::vector<std::string> srcPathList = OptionsParser.getSourcePathList();

    // Initialize the Tool.
    ClangTool Tool(OptionsParser.getCompilations(), srcPathList);

    // The data that will be available after the analysis phase.
    SemanticData* semanticData = new SemanticData();

    // The rewriter that will be used for source-to-source transformations.
    Rewriter* rewriter = new Rewriter();

    // PHASE ONE: analysis of the source code (semantic analyser).
    llvm::outs() << "Phase one: analysis phase...\n";
    int result = Tool.run(new SemanticAnalyserFrontendActionFactory(semanticData, rewriter));

    // PHASE TWO: applying the semantic modifications.
    llvm::outs() << "Phase two: applying semantic modifications... \n";

    // Free used memory.
    delete semanticData;
    delete rewriter;

    // Succes.
    return result;
}
