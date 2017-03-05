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

#include "SemanticAnalyser.h"
#include "SemanticData.h"
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
void structReordering(SemanticData* semanticData, Rewriter* rewriter, ClangTool* Tool);
void writeChangesToOutput(Rewriter* rewriter, std::string subfolderPrefix, int version);

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


void structReordering(SemanticData* semanticData, Rewriter* rewriter, ClangTool* Tool) {

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

        // We check if the struct/reordering already exists.
        StructOrdering orderingStruct = StructOrdering({chosenStruct->getName(), ordering});

        std::vector<StructOrdering>::iterator it;
        int found = 0;
        for (it = chosen.begin(); it != chosen.end(); ++it) {
            if (it->name == orderingStruct.name && it->chosen == orderingStruct.chosen) {
                found = 1;
                break;
            }
        }

        // Retry...
        if (found == 1) {
            continue;
        }

        // Debug information.
        llvm::outs() << "Chosen struct: " << orderingStruct.name << "\n";
        llvm::outs() << "Chosen ordering: " << "\n";
        {
            std::vector<int>::iterator it;
            for (it = orderingStruct.chosen.begin(); it != orderingStruct.chosen.end(); ++it) {
                llvm::outs() << *it << " ";
            }
            llvm::outs() << "\n";
        }

        // We create new structData information.
        StructData* structData = new StructData(chosenStruct->getName());

        // We obtain all field types.
        std::vector<FieldData> fieldData = chosenStruct->getFieldData();

        // We select the fields according to the given ordering.
        std::vector<int>::iterator itTwo;
        for (itTwo = ordering.begin(); itTwo != ordering.end(); ++itTwo) {

            // The chosen field.
            int position = *itTwo;
            FieldData chosenField = fieldData[position];
            structData->addFieldData(chosenField.position,
                                    chosenField.fieldName,
                                    chosenField.fieldType,
                                    chosenField.sourceRange);
        }

        // We add the modified structure information to our semantic data.
        semanticData->getStructReordering()->addStructReorderingData(chosenStruct->getName(), structData);

        // We run the rewriter tool.
        if (amountChosen != 2) {
            int result = Tool->run(new SemanticAnalyserFrontendActionFactory(semanticData, rewriter, false));
        }

        // We write the changes to the output.
        writeChangesToOutput(rewriter, "structreordering_", amountChosen+1);

        // We need to clear the set of structures that have been rewritten already.
        semanticData->getStructReordering()->clearRewritten();

        // We clear the structure reodering map.
        // And add the ordering structure to the chosen vector.
        // We increase the amount of configurations we have chosen.
        semanticData->getStructReordering()->clearStructReorderings();
        chosen.push_back(orderingStruct);
        amountChosen++;
    }
}

// Method which is used to write the changes
void writeChangesToOutput(Rewriter* rewriter, std::string subfolderPrefix, int version) {

    // We construct the full output directory.
    std::stringstream s;
    s << subfolderPrefix << "v" << version;

    std::string outputDirectory = s.str();
    std::string fullPath = OutputDirectory + "/" + outputDirectory;

    // Debug
    llvm::outs() << "Full path: " << fullPath << "\n";

    // We check if this directory exists, if it doesn't we will create it.
    const int dir_err = system(("mkdir -p " + fullPath).c_str());
    if (-1 == dir_err)
    {
        llvm::outs() << "Error creating directory!\n";
        return;
    }

    // Output stream.
    std::ofstream outputFile;

    // We write the results to a new location.
    for (Rewriter::buffer_iterator I = rewriter->buffer_begin(), E = rewriter->buffer_end(); I != E; ++I) {

        std::string output = std::string(I->second.begin(), I->second.end());
        llvm::outs() << "Output: " << output << "\n";

        // Get the file name.
        StringRef fileNameRef = rewriter->getSourceMgr().getFileEntryForID(I->first)->getName();
        std::string fileName = std::string(fileNameRef.data());
        llvm::outs() << "Obtained filename: " << fileName << "\n";

        fileName = fileName.substr(fileName.find_last_of("/\\") + 1); /* until the end automatically... */

        llvm::outs() << "Filename: " << fileName << "\n";

        // Write changes to the file.
        //outputFile.open((fullPath + "/" + fileName).c_str());

        //outputFile.write(output.c_str(), output.length());
        //outputFile.close();
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
    int result = Tool.run(new SemanticAnalyserFrontendActionFactory(semanticData, rewriter, true));

    // PHASE TWO: applying the semantic modifications.
    llvm::outs() << "Phase two: applying semantic modifications... \n";

    // We apply the struct reordering.
    structReordering(semanticData, rewriter, &Tool);

    // Free used memory.
    delete semanticData;
    delete rewriter;

    // Succes.
    return result;
}
