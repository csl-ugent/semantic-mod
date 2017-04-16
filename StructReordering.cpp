#include "StructReordering.h"
#include "json.h"
#include "SemanticUtil.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace llvm;
using namespace clang::tooling;

// Method which applies the structure reordering.
typedef struct structOrdering_ {
    std::string name;
    std::vector<int> chosen;
    StructData* chosenStruct;
} StructOrdering;

// Override this to call our SemanticAnalyser on the entire source file.
void StructReorderingASTConsumer::HandleTranslationUnit(ASTContext &Context) {

     // Visitor depends on the phase we are in.
     if (phaseType == Phase::Analysis) {
         this->visitorAnalysis->TraverseDecl(Context.getTranslationUnitDecl());
     } else if (phaseType == Phase::Rewrite) {
         this->visitorRewriter->TraverseDecl(Context.getTranslationUnitDecl());
     } else if (phaseType == Phase::PreTransformationAnalysis) {
         this->visitorPreTransformationAnalysis->TraverseDecl(Context.getTranslationUnitDecl());
     }
}

// AST visitor, used for analysis.
bool StructReorderingAnalyser::VisitTranslationUnitDecl(clang::TranslationUnitDecl* TD) {
    clang::DeclContext::decl_iterator it;
    for(it = TD->decls_begin(); it != TD->decls_end(); ++it) {

        // Current delcaration we are considering.
        Decl* decl = *it;

        // We check if the Decl is a RecordDecl.
        if (RecordDecl* D = dyn_cast<RecordDecl>(decl)) {

            // We detect structures here.
            if (D->isStruct()) {

                // We analyse the structure.
                std::string structName = D->getNameAsString();

                // We check if we already found this struct or not.
                // If we didn't found this struct before, we add an entry for it.
                StructReordering* structReordering = this->semanticData->getStructReordering();
                if (structReordering->isInStructMap(structName)) {
                    continue; // Structure already analysed...
                } else {
                    // Some debug information.
                    //D->dump();
                    //llvm::outs() << "\n";
                }

                // We create new structData information.
                StringRef fileNameRef = astContext->getSourceManager().getFilename(
                    D->getLocation());

                std::string fileNameStr;
                if (fileNameRef.data()) {
                    fileNameStr = std::string(fileNameRef.data());

                    // We make sure the file is contained in our base directory...
                    if (fileNameStr.find(this->baseDirectory) == std::string::npos) {

                        // Declaration is not contained in a header located in
                        // the base directory...
                        continue;
                    }
                } else {

                    // Invalid struct to analyse... (header name cannot be found?)
                    continue;
                }

                // We get the type for this declaration.
                const Type* type = D->getTypeForDecl();
                StructData* structData = new StructData(structName, fileNameStr);

                // Analysing the members of the structure.
                std::string fieldName;
                std::string fieldType;
                SourceRange sourceRange;
                int position = 0;
                for(RecordDecl::field_iterator jt = D->field_begin(); jt != D->field_end();++jt)
                {
                    fieldName = jt->getNameAsString();
                    fieldType = jt->getType().getAsString();
                    sourceRange = jt->getSourceRange();

                    // We add the field information to the structure information.
                    structData->addFieldData(position, fieldName, fieldType, sourceRange);
                    position++;
                }

                // We iterate over possible declarations within the struct.
                for (clang::DeclContext::decl_iterator kt = D->decls_begin(); kt != TD->decls_end(); ++kt) {

                    // Current delcaration we are considering.
                    Decl* innerDecl = *kt;

                    // We check if the Decl is a RecordDecl.
                    if (RecordDecl* innerD = dyn_cast<RecordDecl>(innerDecl)) {
                        std::string recordText = location2str(innerD->getLocStart(), innerD->getLocEnd(), &astContext->getSourceManager(), &astContext->getLangOpts());
                        structData->addFieldDefinition(recordText);
                    }
                }

                // We add it to the StructData map.
                structReordering->addStructData(structData->getName(), structData);
            }
        }
    }
    return true;
}

void detectStructsRecursively(const Type* type, SemanticData* semanticData);

void detectStructsRecursively(const Type* type, SemanticData* semanticData) {

    // We obtain all the struct data from the previous phase.
    StructReordering* structReordering = semanticData->getStructReordering();

    // We check if the original declaration is a struct type.
    if (type != NULL && type->isStructureType()) {

        // We obtain the record declaration by considering it a struct type.
        RecordDecl* recordDecl = type->getAsStructureType()->getDecl();

        // The name of the structure.
        std::string structName = recordDecl->getNameAsString();

        // We check if the struct is in the struct map.
        if (structReordering->isInStructMap(structName)) {

            // We remove it from the struct map.
            structReordering->removeStructData(structName);
            llvm::outs() << "Removed struct: " << structName << "!\n";
        }

        // We further investigate the fields of the struct.
        for(RecordDecl::field_iterator jt = recordDecl->field_begin(); jt != recordDecl->field_end();++jt)
        {
            // We obtain the type of the field.
            FieldDecl* field = *jt;
            const Type* subType = (field->getType()).getTypePtrOrNull();

            // We handle the field recursively.
            detectStructsRecursively(subType, semanticData);
        }
    } else if (type != NULL && type->isUnionType()) { // Handle unions.

        // We obtain the record declaration by considering it as a union type.
        RecordDecl* recordDecl = type->getAsUnionType()->getDecl();

        // We further investigate the fields of the union.
        for(RecordDecl::field_iterator jt = recordDecl->field_begin(); jt != recordDecl->field_end();++jt)
        {
            // We obtain the type of the field.
            FieldDecl* field = *jt;
            const Type* subType = (field->getType()).getTypePtrOrNull();

            // We handle the field recursively.
            detectStructsRecursively(subType, semanticData);
        }
    } else if (type != NULL && type->isArrayType()) { // Handle arrays.

        // We obtain the element type information.
        const Type* subType = (type->getAsArrayTypeUnsafe()->getElementType()).getTypePtrOrNull();

        // We handle the type recursively.
        detectStructsRecursively(subType, semanticData);
    }
}

// AST visitor, used for performing analysis pre transformation.
bool StructReorderingPreTransformationAnalysis::VisitTranslationUnitDecl(clang::TranslationUnitDecl* TD) {
    clang::DeclContext::decl_iterator it;
    for(it = TD->decls_begin(); it != TD->decls_end(); ++it) {

        // Current declaration we are considering.
        Decl* decl = *it;
        if (VarDecl* D = dyn_cast<VarDecl>(decl)) {

            // We obtain the type information.
            const Type* type = (D->getType()).getTypePtrOrNull();

            // We detect structs recursively and remove them from the struct map.
            detectStructsRecursively(type, this->semanticData);
        }
    }
    return true;
}

// AST visitor, used for rewriting the source code.
bool StructReorderingRewriter::VisitTranslationUnitDecl(clang::TranslationUnitDecl* TD) {
    clang::DeclContext::decl_iterator it;
    for(it = TD->decls_begin(); it != TD->decls_end(); ++it) {

        // Current delcaration we are considering.
        Decl* decl = *it;

        // We check if the Decl is a RecordDecl.
        if (RecordDecl* D = dyn_cast<RecordDecl>(decl)) {


            // We detect structures here.
            if (D->isStruct()) {

                // We analyse the structure name.
                std::string structName = D->getNameAsString();

                StructReordering* structReordering = this->semanticData->getStructReordering();
                if (structReordering->isInStructReorderingMap(structName)) {

                    // We make sure we do not rewrite some structure that has already
                    // been rewritten.
                    if (structReordering->hasBeenRewritten(structName)) {
                        return true;
                    }

                    // We need to rewrite it.
                    StructData* structData = structReordering->getStructReorderings()[structName];

                    // We obtain the fields data of the structure.
                    std::vector<FieldData> fieldData = structData->getFieldData();


                    {
                        // FieldData iterator.
                        std::vector<FieldData>::iterator it;
                        SourceRange lowerBound = fieldData[0].sourceRange;
                        SourceRange upperBound = fieldData[0].sourceRange;;
                        std::stringstream replacement;
                        FieldData substitute;

                        // Iterate over all reordered elements.
                        for (int i = 0; i < fieldData.size(); i++) {

                            // We get the substitute field.
                            substitute = fieldData[i];

                            // We update the replacement string.
                            replacement << substitute.fieldType + " " + substitute.fieldName;
                            if (i != fieldData.size() - 1) {
                                replacement << ";";
                            }

                            // We check upper/lowerbounds on declarations.
                            if (lowerBound.getBegin().getRawEncoding() > substitute.sourceRange.getBegin().getRawEncoding()) {
                                lowerBound = substitute.sourceRange;
                            }

                            if (upperBound.getEnd().getRawEncoding() < substitute.sourceRange.getEnd().getRawEncoding()) {
                                upperBound = substitute.sourceRange;
                            }
                        }

                        // We replace the declarations with a new declaration string.
                        this->rewriter->ReplaceText(SourceRange(lowerBound.getBegin(), upperBound.getEnd()), replacement.str());


                        // We add possible field definitions to the struct.
                        if (structData->hasFieldDefinitions()) {

                            std::set<std::string> definitions = structData->getFieldDefinitions();
                            std::set<std::string>::reverse_iterator it;
                            FieldDecl* firstField = *(D->field_begin());
                            for (it = definitions.rbegin(); it != definitions.rend(); ++it) {
                                std::string definition = *it;
                                this->rewriter->InsertTextBefore(firstField->getLocStart(),
                                                                 definition + ";");
                            }
                        }
                    }

                    // We add this structure to the set of rewritten structures.
                    structReordering->structRewritten(structName);
                }
            }
        }
    }
    return true;
}

// Method used for the structreordering semantic transformation.
void structReordering(SemanticData* semanticData, Rewriter* rewriter, ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings) {

    // Debug information.
    llvm::outs() << "Phase 1: Struct analysis\n";

    // We run the analysis phase.
    Tool->run(new SemanticFrontendActionFactory(semanticData, rewriter, baseDirectory, Transformation::StructReordering, Phase::Analysis));

    // Debug information.
    llvm::outs() << "Phase 2: Struct pre-transformation analysis\n";

    // We run the analysis phase.
    Tool->run(new SemanticFrontendActionFactory(semanticData, rewriter, baseDirectory, Transformation::StructReordering, Phase::PreTransformationAnalysis));

    // We determine the structs that will be reordered.
    int amount = amountOfReorderings;
    int amountChosen = 0;
    std::vector<StructOrdering> chosen;
    std::map<std::string, StructData*>::iterator it;
    std::map<std::string, StructData*> structMap = semanticData->getStructReordering()->getStructMap();
    std::string outputPrefix = outputDirectory + "/struct_r_";

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
        output["type"] = "struct_reordering";
        output["file_struct_name"] = chosenStruct->getName();
        output["file_name"] = chosenStruct->getFileName();
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
        output["original"]["fields"] = vec;
        std::random_shuffle(ordering.begin(), ordering.end());

        std::vector<StructOrdering>::iterator it;
        int found = 0;
        for (it = chosen.begin(); it != chosen.end(); ++it) {
            if (it->name == chosenStruct->getName() && it->chosen == ordering) {
                found = 1;
                break;
            }
        }

        // Retry...
        if (found == 1) {
            continue;
        }

        // Debug information.
        llvm::outs() << "Chosen struct: " << chosenStruct->getName() << "\n";
        llvm::outs() << "Chosen ordering: " << "\n";
        {

            std::vector<int>::iterator it;
            for (it = ordering.begin(); it != ordering.end(); ++it) {
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
        StructOrdering orderingStruct = StructOrdering({chosenStruct->getName(), ordering, structData});

        // We write some information regarding the performed transformations to output.
        writeJSONToFile(outputPrefix, amountChosen+1, "transformations.json", output);

        // We add the name of the struct that needs to be rewritten.
        structData->setFieldDefinitions(chosenStruct->getFieldDefinitions());
        semanticData->getStructReordering()->addStructNeedRewritten(chosenStruct->getName());

        // Add the orderingstruct to the list of orderingstructs and
        // increase the amount we have chosen.
        chosen.push_back(orderingStruct);
        amountChosen++;
    }

    // We perform the rewrite operations.
    int processed = 0;
    while (processed < amount)
    {

        // We select the current structreordering.
        StructOrdering selectedStructOrdering = chosen[processed];
        StructData* chosenStruct = selectedStructOrdering.chosenStruct;

        // We add the modified structure information to our semantic data.
        semanticData->getStructReordering()->addStructReorderingData(chosenStruct->getName(), chosenStruct);

        llvm::outs() << "Phase 3: performing rewrite for version: " << processed + 1 << " struct name: " << chosenStruct->getName() << "\n";
        Tool->run(new SemanticFrontendActionFactory(semanticData, rewriter, baseDirectory, Transformation::StructReordering, Phase::Rewrite, processed+1, outputPrefix));

        // We need to clear the set of structures that have been rewritten already.
        // We need to clear the set of structures that need to be reordered.
        semanticData->getStructReordering()->clearRewritten();
        semanticData->getStructReordering()->clearStructReorderings();
        processed++;
    }
}
