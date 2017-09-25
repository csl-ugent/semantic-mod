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


// AST visitor, used for analysis.
bool FPReorderingAnalyser::VisitFunctionDecl(clang::FunctionDecl* FD) {
    // We make sure we iterate over the definition.
    if (FD->isThisDeclarationADefinition()) {
        // If we haven't already selected the function, check if the function is eligible:
        // - can't be main (TODO: This should actually check for exported functions, not just main)
        // - can't be variadic (perhaps we can handle this in the future)
        // - has to have enough parameters, so at least 2
        if (!FD->isMain() && !FD->isVariadic() && FD->param_size() > 1) {
            const FunctionUnique function(FD, astContext);
            const std::string& fileName = function.getFileName();

            // We make sure the file is contained in our base directory...
            if (fileName.find(this->baseDirectory) == std::string::npos)
                return true;

            reordering.candidates.emplace_back(function, FD);
        }
    }

    return true;
}

// AST rewriter, used for rewriting source code.
bool FPReorderingRewriter::VisitCallExpr(clang::CallExpr* CE) {
    // We try to get the callee of this function call.
    if (FunctionDecl* FD = CE->getDirectCallee()) {
        // We check if this function is to be reordered
        const FPTransformation* transformation = reordering.transformation;
        const FunctionUnique function(FD, astContext);
        if (transformation->function == function) {
            llvm::outs() << "Call to function: " << FD->getNameAsString() << " has to be rewritten!\n";

            auto ordering = transformation->ordering;
            for (unsigned iii = 0; iii < CE->getNumArgs(); iii++)
            {
                // We replace the field arg with another argument based on the reordering information.
                this->rewriter->ReplaceStmt(CE->getArg(iii), CE->getArg(ordering[iii]));
            }
        }
    }
    return true;
}

// AST rewriter, used for rewriting source code.
bool FPReorderingRewriter::VisitFunctionDecl(clang::FunctionDecl* FD) {
    // Check if this is a declaration for the function that is to be reordered
    const FPTransformation* transformation = reordering.transformation;
    FunctionUnique function(FD, astContext);
    if (transformation->function == function) {
        llvm::outs() << "Rewriting function: " << FD->getNameAsString() << " definition: " << FD->isThisDeclarationADefinition() << "\n";

        // We iterate over all parameters in the function declaration.
        const auto ordering = transformation->ordering;
        for (unsigned iii = 0; iii < FD->getNumParams(); iii++) {
            const ParmVarDecl* oldParam = FD->getParamDecl(iii);
            const ParmVarDecl* newParam = FD->getParamDecl(ordering[iii]);

            std::string name = newParam->getNameAsString();
            std::string type = newParam->getType().getAsString();
            std::string substitute = type + " " + name;
            if (newParam->getNameAsString().empty()) { // It is possible to define a function without parameter argument names...
                // In this case the sourcerange covers the field separator and closing parenthesis as well
                if (iii != FD->getNumParams() -1)
                    substitute += ",";
                else
                    substitute += ")";
            }

            // We replace the field with the new field information.
            this->rewriter->ReplaceText(oldParam->getSourceRange(), substitute);
        }
    }

    return true;
}

// Method used for the function parameter semantic transformation.
void fpreordering(Rewriter* rewriter, ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings) {
    // Debug information.
    llvm::outs() << "Phase 1: Function Analysis\n";

    // We run the analysis phase and get the candidates
    FPReordering reordering;
    Tool->run(new SemanticFrontendActionFactory(reordering, rewriter, baseDirectory, Transformation::FPReordering, Phase::Analysis));
    auto candidates = reordering.candidates;

    // We need to determine the maximum amount of reorderings that is actually
    // possible with the given input source files (based on the analysis phase).
    std::map<int, int> histogram;
    long totalReorderings = 0;
    double averageParameters = 0;
    for (const auto candidate : candidates) {
        unsigned nrParams = candidate.params.size();

        // We increase the total possible reorderings, based on
        // the factorial of the number of fields.
        totalReorderings += factorial(nrParams);
        averageParameters += nrParams;

        // Look if this amount has already occured or not.
        if (histogram.find(nrParams) != histogram.end()) {
            histogram[nrParams]++;
        } else {
            histogram[nrParams] = 1;
        }
    }

    // Create analytics
    Json::Value analytics;
    analytics["amount_of_functions"] = candidates.size();
    analytics["avg_function_parameters"] = averageParameters / candidates.size();
    analytics["entropy"] = entropyEquiprobable(totalReorderings);

    // Add function parameter histogram.
    for (const auto& it : histogram) {
        std::stringstream ss;
        ss << it.first;
        analytics["function_parameters"][ss.str()] = it.second;
    }

    // Output analytics
    llvm::outs() << "Total reorderings possible with " << candidates.size() << " functions is: " << totalReorderings << "\n";
    llvm::outs() << "Writing analytics output...\n";
    writeJSONToFile(outputDirectory, -1, "analytics.json", analytics);

    // Get the output prefix
    std::string outputPrefix = outputDirectory + "function_r_";

    // We choose the amount of configurations of this struct that is required.
    unsigned long amount = amountOfReorderings;
    unsigned long amountChosen = 0;
    std::vector<FPTransformation> transformations;
    llvm::outs() << "Amount of reorderings is set to: " << amount << "\n";
    while (amountChosen < amount)
    {
        // We choose a candidate at random.
        FunctionData& chosen = candidates[random_0_to_n(candidates.size())];
        FunctionUnique& chosenFunction = chosen.function;
        std::vector<FunctionParam>& params = chosen.params;

        // Things we should write to an output file.
        Json::Value output;
        output["type"] = "function_parameter_reordering";
        output["file_function_name"] = chosenFunction.getName();
        output["file_name"] = chosenFunction.getFileName();

        // We determine a random ordering of fields.
        Json::Value original(Json::arrayValue);
        std::vector<unsigned> ordering;
        for (unsigned iii = 0; iii < params.size(); iii++) {
            const FunctionParam& param = params[iii];
            Json::Value field;
            field["position"] = iii;
            field["name"] = param.name;
            field["type"] = param.type;
            original.append(field);
            ordering.push_back(iii);
        }
        output["original"]["fields"] = original;
        std::random_shuffle(ordering.begin(), ordering.end());

        // Check if this transformation isn't duplicate. If it is, we try again
        bool found = false;
        for (auto& t : transformations) {
            if (t.function == chosenFunction && t.ordering == ordering) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        // Debug information.
        llvm::outs() << "Chosen Function: " << chosenFunction.getName() << "\n";
        llvm::outs() << "Chosen ordering: " << "\n";
        for (auto it : ordering) {
            llvm::outs() << it << " ";
        }
        llvm::outs() << "\n";

        // Create the output for the parameter reordering as decided
        Json::Value modified(Json::arrayValue);
        for (size_t iii = 0; iii < ordering.size(); iii++) {
            unsigned old_pos = ordering[iii];
            const FunctionParam& param = params[old_pos];

            // The chosen field.
            Json::Value field;
            field["position"] = iii;
            field["name"] = param.name;
            field["type"] = param.type;
            modified.append(field);
            llvm::outs() << "Position:" << old_pos << " type: " << param.type << "\n";
        }
        output["modified"]["fields"] = modified;

        // We write some information regarding the performed transformations to output.
        writeJSONToFile(outputPrefix, amountChosen+1, "transformations.json", output);

        // Add the transformation
        transformations.emplace_back(chosenFunction, ordering);
        amountChosen++;
    }

    // We perform the rewrite operations.
    for (unsigned long iii = 0; iii < amount; iii++)
    {
        // We select the current transformation
        FPTransformation* transformation = &transformations[iii];
        reordering.transformation = transformation;

        llvm::outs() << "Phase 2: performing rewrite for version: " << iii + 1 << " function name: " << transformation->function.getName() << "\n";
        Tool->run(new SemanticFrontendActionFactory(reordering, rewriter, baseDirectory, Transformation::FPReordering, Phase::Rewrite, iii + 1, outputPrefix));
    }
}
