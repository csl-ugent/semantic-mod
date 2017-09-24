#include "FPReordering.h"
#include "json.h"
#include "SemanticUtil.h"
#include <sstream>
#include <string>

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace llvm;
using namespace clang::tooling;

// Struct which applies the function reordering.
typedef struct functionOrdering_ {
    std::string name;
    std::vector<int> chosen;
    FunctionData* chosenFunction;
} FunctionOrdering;


// AST visitor, used for analysis.
bool FPReorderingAnalyser::VisitFunctionDecl(clang::FunctionDecl* FD) {
    // We make sure we iterate over the definition.
    if (FD->hasBody()) {

        // We obtain the function name.
        std::string functionName = FD->getNameAsString();

        // If we haven't already selected the function, check if the function is eligible:
        // - can't be main (TODO: This should actually check for exported functions, not just main)
        // - can't be variadic (perhaps we can handle this in the future)
        // - has to have enough parameters, so at least 2
        if (!reordering.isInFunctionMap(functionName) && !FD->isMain() && !FD->isVariadic() && FD->param_size() > 1) {

            // We create new functionData information.
            StringRef fileNameRef = astContext->getSourceManager().getFilename(FD->getLocation());

            std::string fileNameStr;
            if (fileNameRef.data()) {
                fileNameStr = std::string(fileNameRef.data());

                // We make sure the file is contained in our base directory...
                if (fileNameStr.find(this->baseDirectory) == std::string::npos) {

                    // Declaration is not contained in a header located in
                    // the base directory...
                    return true;
                }
            }
            else
                // Invalid function to analyse... (header name cannot be found?)
                return true;

            // DEBUG information.
            llvm::outs() << "Found valid function: " << functionName << "\n";

            // We create a function data object.
            FunctionData* functionData = new FunctionData(functionName, fileNameStr);

            // We iterate over all parameters.
            clang::FunctionDecl::param_const_iterator it;
            std::string fieldName;
            std::string fieldType;
            SourceRange sourceRange;
            int position = 0;
            for (it = FD->param_begin(); it != FD->param_end(); ++it) {

                // We obtain the parameter variable declaration.
                ParmVarDecl *param = *it;

                // We obtain the relevant parameter information.
                fieldName = param->getNameAsString();
                fieldType = param->getType().getAsString();
                sourceRange = param->getSourceRange();

                functionData->addFieldData(position, fieldName, fieldType, sourceRange);
                position++;
            }

            reordering.addFunctionData(functionData->getName(), functionData);
        }
    }

    return true;
}

// AST rewriter, used for rewriting source code.
bool FPReorderingRewriter::VisitCallExpr(clang::CallExpr* CE) {

    // We try to get the callee of this function call.
    if (FunctionDecl* FD = CE->getDirectCallee()) {

        // We obtain the name of the function.
        std::string functionName = FD->getNameAsString();

        // We check if this function is eligible for reordering.
        if (reordering.isInFunctionReorderingMap(functionName)) {
            // DEBUG.
            llvm::outs() << "Call to function: " << functionName << " has to be rewritten!\n";

            // Get the reordering
            FunctionData* functionData = reordering.getFunctionReorderings()[functionName];
            std::vector<FieldData> fieldData = functionData->getFieldData();

            for (unsigned iii = 0; iii < CE->getNumArgs(); iii++)
            {
                // Get the substitute for this argument
                FieldData substitute = fieldData[iii];

                // We replace the field arg with another argument based on the reordering information.
                this->rewriter->ReplaceStmt(CE->getArg(iii), CE->getArg(substitute.position));
            }
        }
    }
    return true;
}

// AST rewriter, used for rewriting source code.
bool FPReorderingRewriter::VisitFunctionDecl(clang::FunctionDecl* FD) {

    // We obtain the function name.
    std::string functionName = FD->getNameAsString();

    // We check if this function is eligible for reordering.
    if (reordering.isInFunctionReorderingMap(functionName)) {

        // We make sure we do not rewrite some function that has already
        // been rewritten (unless it is the definition!).
        if (reordering.hasBeenRewritten(functionName) &&
            !FD->isThisDeclarationADefinition()) {
            return true;
        }

        // DEBUG.
        llvm::outs() << "Rewriting function: " << functionName << " definition: " << FD->hasBody() << "\n";

        // We need to rewrite it.
        FunctionData* functionData = reordering.getFunctionReorderings()[functionName];

        // We obtain the fields data of the function.
        std::vector<FieldData> fieldData = functionData->getFieldData();

        {
            // We iterate over all parameters in the function declaration.
            clang::FunctionDecl::param_iterator it;
            FieldData substitute;
            SourceRange sourceRange;
            int i = 0;
            for (it = FD->param_begin(); it != FD->param_end(); ++it, ++i) {

                // We obtain the parameter variable declaration.
                ParmVarDecl *param = *it;

                // We get the substitute field.
                substitute = fieldData[i];

                // We obtain the relevant parameter information.
                sourceRange = param->getSourceRange();
                std::string substituteStr;
                if (param->getNameAsString() == "") { // It is possible to define a function without parameter argument names...

                    // In this case the sourcerange covers the field separator and closing parenthesis as well.
                    if (i != functionData->getFieldDataSize() - 1) {
                        substituteStr = substitute.fieldType + " " + substitute.fieldName + ",";
                    } else {
                        substituteStr = substitute.fieldType + " " + substitute.fieldName + ")";
                    }
                } else {
                    substituteStr = substitute.fieldType + " " + substitute.fieldName;
                }

                // We replace the field with the new field information.
                this->rewriter->ReplaceText(sourceRange, substituteStr);
            }
        }

        // We add this function to the set of rewritten functions.
        reordering.functionRewritten(functionName);
    }
    return true;
}

// Method used for the function parameter semantic transformation.
void fpreordering(Rewriter* rewriter, ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings) {

    // Debug information.
    llvm::outs() << "Phase 1: Function Analysis\n";

    // We run the analysis phase.
    FPReordering reordering;
    Tool->run(new SemanticFrontendActionFactory(reordering, rewriter, baseDirectory, Transformation::FPReordering, Phase::Analysis));

    // More analytics written to output.
    Json::Value analytics;

    // We determine the functions that will be reordered.
    unsigned long amount = amountOfReorderings;
    unsigned long amountChosen = 0;
    std::vector<FunctionOrdering> chosen;
    std::map<std::string, FunctionData*>::iterator it;
    std::map<std::string, FunctionData*> functionMap = reordering.getFunctionMap();
    std::string outputPrefix = outputDirectory + "function_r_";

    // Add some analytics information.
    std::map<int, int> histogram;

    analytics["amount_of_functions"] = functionMap.size();

    // We need to determine the maximum amount of reorderings that is actually
    // possible with the given input source files (based on the analysis phase).
    {
        FunctionData* current;
        long totalReorderings = 0;
        double averageParameters = 0;
        for (it = functionMap.begin(); it != functionMap.end(); ++it) {

            // We set the current FunctionData we are iterating over.
            current = it->second;

            // We increase the total possible reorderings, based on
            // the factorial of the number of fields.
            totalReorderings += factorial(current->getFieldDataSize());
            averageParameters += current->getFieldDataSize();

            // Look if this amount has already occured or not.
            if (histogram.find(current->getFieldDataSize()) != histogram.end()) {
                histogram[current->getFieldDataSize()]++;
            } else {
                histogram[current->getFieldDataSize()] = 1;
            }
        }

        {
            // Add function parameter histogram.
            std::map<int, int>::iterator it;
            for (it = histogram.begin(); it != histogram.end(); ++it) {
                std::stringstream ss;
                ss << it->first;
                analytics["function_parameters"][ss.str()] = it->second;
            }
        }

        // Add some analytics information.
        analytics["avg_function_parameters"] = averageParameters / functionMap.size();
        analytics["entropy"] = entropyEquiprobable(totalReorderings);

        // Debug.
        llvm::outs() << "Total reorderings possible with " << functionMap.size() << " functions is: " << totalReorderings << "\n";
    }

    // Debug.
    llvm::outs() << "Writing analytics output...\n";

    // Write some debug analytics to output.
    writeJSONToFile(outputDirectory, -1, "analytics.json", analytics);

    // Debug.
    llvm::outs() << "Amount of reorderings is set to: " << amount << "\n";

    // We choose the amount of configurations of this struct that is required.
    while (amountChosen < amount)
    {
        // Things we should write to an output file.
        Json::Value output;

        // We choose a struct at random.
        it = functionMap.begin();
        std::advance(it, random_0_to_n(functionMap.size()));
        FunctionData* chosenFunction = it->second;

        // Output name.
        output["type"] = "function_parameter_reordering";
        output["file_function_name"] = chosenFunction->getName();
        output["file_name"] = chosenFunction->getFileName();
        Json::Value vec(Json::arrayValue);

        // We determine a random ordering of fields.
        std::vector<int> ordering;
        for (int i = 0; i < chosenFunction->getFieldDataSize(); i++) {
            Json::Value field;
            FieldData data = chosenFunction->getFieldData()[i];
            field["position"] = data.position;
            field["name"] = data.fieldName;
            field["type"] = data.fieldType;
            vec.append(field);
            ordering.push_back(i);
        }
        output["original"]["fields"] = vec;
        std::random_shuffle(ordering.begin(), ordering.end());

        std::vector<FunctionOrdering>::iterator it;
        int found = 0;
        for (it = chosen.begin(); it != chosen.end(); ++it) {
            if (it->name == chosenFunction->getName() && it->chosen == ordering) {
                found = 1;
                break;
            }
        }

        // Retry...
        if (found == 1) {
            continue;
        }

        // Debug information.
        llvm::outs() << "Chosen Function: " << chosenFunction->getName() << "\n";
        llvm::outs() << "Chosen ordering: " << "\n";
        {

            std::vector<int>::iterator it;
            for (it = ordering.begin(); it != ordering.end(); ++it) {
                llvm::outs() << *it << " ";
            }
            llvm::outs() << "\n";
        }

        // We create new functionData information.
        FunctionData* functionData = new FunctionData(chosenFunction->getName(), chosenFunction->getFileName());

        // We obtain all field types.
        std::vector<FieldData> fieldData = chosenFunction->getFieldData();

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
                functionData->addFieldData(chosenField.position,
                                           chosenField.fieldName,
                                           chosenField.fieldType,
                                           chosenField.sourceRange);
                field["position"] = i;
                field["name"] = chosenField.fieldName;
                field["type"] = chosenField.fieldType;
                vec.append(field);
                i++;
                llvm::outs() << "Position:" << position << " type: " << chosenField.fieldType << "\n";
            }

            output["modified"]["fields"] = vec;
        }

        // We check if the struct/reordering already exists.
        FunctionOrdering orderingFunction = FunctionOrdering({chosenFunction->getName(), ordering, functionData});

        // We write some information regarding the performed transformations to output.
        writeJSONToFile(outputPrefix, amountChosen+1, "transformations.json", output);

        // Add the orderingstruct to the list of orderingstructs and
        // increase the amount we have chosen.
        chosen.push_back(orderingFunction);
        amountChosen++;
    }

    // We perform the rewrite operations.
    unsigned long processed = 0;
    while (processed < amount)
    {

        // We select the current functionreordering.
        FunctionOrdering selectedFunctionOrdering = chosen[processed];
        FunctionData* chosenFunction = selectedFunctionOrdering.chosenFunction;

        // We add the modified function information to our semantic data.
        reordering.addFunctionReorderingData(chosenFunction->getName(), chosenFunction);

        llvm::outs() << "Phase 2: performing rewrite for version: " << processed + 1 << " function name: " << chosenFunction->getName() << "\n";
        Tool->run(new SemanticFrontendActionFactory(reordering, rewriter, baseDirectory, Transformation::FPReordering, Phase::Rewrite, processed+1, outputPrefix));

        // We need to clear the set of structures that have been rewritten already.
        // We need to clear the set of structures that need to be reordered.
        reordering.clearRewritten();
        reordering.clearFunctionReorderings();
        processed++;
    }
}
