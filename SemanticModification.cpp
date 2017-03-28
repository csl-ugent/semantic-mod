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

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

// Extra output option.
static cl::opt<std::string> OutputDirectory("od");
static cl::opt<std::string> BaseDirectory("bd");
static cl::opt<int> AmountOfReorderings("sr_am");

// Method which applies the structure reordering.
typedef struct structOrdering_ {
    std::string name;
    std::vector<int> chosen;
} StructOrdering;

// Method used for the structreordering semantic transformation.
void structReordering(SemanticData* semanticData, Rewriter* rewriter, ClangTool* Tool) {

    // Debug information.
    llvm::outs() << "Struct reordering analysis...\n";

    // We determine the structs that will be reordered.
    int amount = AmountOfReorderings;
    int amountChosen = 0;
    std::vector<StructOrdering> chosen;
    std::map<std::string, StructData*>::iterator it;
    std::map<std::string, StructData*> structMap = semanticData->getStructReordering()->getStructMap();

    // We need to determine the maximum amount of reorderings that is actually
    // possible with the given input source files (based on the analysis phase).
    {
        StructData* current;
        int totalReorderings = 0;
        for (it = structMap.begin(); it != structMap.end(); ++it) {
            // We set the current StructData we are iterating over.
            current = it->second;

            // We increase the total possible reorderings, based on
            // the factorial of the number of fields.
            totalReorderings += factorial(current->getFieldDataSize());
        }

        // Debug.
        llvm::outs() << "Total reorderings possible with " << structMap.size() << " structs is: " << totalReorderings << "\n";

        // We might need to cap the total possible reorderings.
        if (amount > totalReorderings) {
            amount = totalReorderings;
        }
    }

    // Debug.
    llvm::outs() << "Amount of reorderings is set to: " << amount << "\n";

    // We choose the amount of configurations of this struct that is required.
    while (amountChosen < amount)
    {
        // Things we should write to an output file.
        Json::Value output;

        // We choose a struct at random.
        it = structMap.begin();
        std::advance(it, random_0_to_n(structMap.size()));
        StructData* chosenStruct = it->second;

        // Output name.
        output["struct_reordering"]["file_struct_name"] = chosenStruct->getName();
        output["struct_reordering"]["file_name"] = chosenStruct->getFileName();
        Json::Value vec(Json::arrayValue);

        // We determine a random ordering of fields.
        std::vector<int> ordering;
        for (int i = 0; i < chosenStruct->getFieldDataSize(); i++) {
            Json::Value field;
            FieldData data = chosenStruct->getFieldData()[i];
            field["position"] = data.position;
            field["name"] = data.fieldName;
            field["type"] = data.fieldType;
            vec.append(field);
            ordering.push_back(i);
        }
        output["struct_reordering"]["original"]["fields"] = vec;
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
        StructData* structData = new StructData(chosenStruct->getName(), chosenStruct->getFileName());

        // We obtain all field types.
        std::vector<FieldData> fieldData = chosenStruct->getFieldData();

        // We select the fields according to the given ordering.
        std::vector<int>::iterator itTwo;
        {
            Json::Value vec(Json::arrayValue);
            int i = 0;
            for (itTwo = ordering.begin(); itTwo != ordering.end(); ++itTwo) {

                // The chosen field.
                Json::Value field;
                int position = *itTwo;
                FieldData chosenField = fieldData[position];
                structData->addFieldData(chosenField.position,
                                        chosenField.fieldName,
                                        chosenField.fieldType,
                                        chosenField.sourceRange);
                field["original_position"] = position;
                field["name"] = chosenField.fieldName;
                field["type"] = chosenField.fieldType;
                vec.append(field);
                i++;
            }
            output["struct_reordering"]["modified"]["fields"] = vec;
        }

        // We add the modified structure information to our semantic data.
        semanticData->getStructReordering()->addStructReorderingData(chosenStruct->getName(), structData);

        std::string outputPrefix = OutputDirectory + "/struct_r_";
        int result = Tool->run(new SemanticAnalyserFrontendActionFactory(semanticData, rewriter, false, BaseDirectory, amountChosen+1, OutputDirectory + "/struct_r_"));

        // We run the rewriter tool.
        // We write some information regarding the performed transformations to output.
        writeJSONToFile(outputPrefix, amountChosen+1, "transformations.json", output);

        // We clear the structure reodering map.
        // And add the ordering structure to the chosen vector.
        // We increase the amount of configurations we have chosen.
        // We need to clear the set of structures that have been rewritten already.
        semanticData->getStructReordering()->clearRewritten();
        semanticData->getStructReordering()->clearStructReorderings();
        chosen.push_back(orderingStruct);
        amountChosen++;
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

    std::vector<CompileCommand>::iterator it;
    for (it = OptionsParser.getCompilations().getAllCompileCommands().begin(); it != OptionsParser.getCompilations().getAllCompileCommands().end(); ++it) {
        CompileCommand command = *it;

        std::vector<std::string>::iterator it2;

        for (it2 = command.CommandLine.begin(); it2 != command.CommandLine.end(); ++it2) {
            llvm::outs() << "Command line: " << *it2 << "\n";
        }
    }

    // The data that will be available after the analysis phase.
    SemanticData* semanticData = new SemanticData();

    // The rewriter that will be used for source-to-source transformations.
    Rewriter* rewriter = new Rewriter();

    // PHASE ONE: analysis of the source code (semantic analyser).
    llvm::outs() << "Phase one: analysis phase...\n";
    int result = Tool.run(new SemanticAnalyserFrontendActionFactory(semanticData, rewriter, true, BaseDirectory));

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
