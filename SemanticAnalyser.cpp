#include "SemanticAnalyser.h"

using namespace clang;
using namespace llvm;

// AST visitor, used for analysis.
bool SemanticAnalyser::VisitRecordDecl(RecordDecl *D) {

    // We detect structures here.
    if (D->isStruct()) {

        // We analyse the structure.
        std::string structName = D->getNameAsString();

        // We check if we already found this struct or not.
        // If we didn't found this struct before, we add an entry for it.
        StructReordering* structReordering = this->semanticData->getStructReordering();
        if (structReordering->isInStructMap(structName)) {
            llvm::outs() << "Structure: " << structName << " already analysed.\n";
            return true; // Structure already analysed...
        } else {
            // Some debug information.
            D->dump();
            llvm::outs() << "\n";
        }

        // We create new structData information.
        StructData* structData = new StructData(structName);

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

        // We add it to the StructData map.
        structReordering->addStructData(structData->getName(), structData);
    }
    return true;
}


// AST visitor, used for rewriting the source code.
bool SemanticRewriter::VisitRecordDecl(RecordDecl *D) {

    // We detect structures here.
    if (D->isStruct()) {

        // We analyse the structure name.
        std::string structName = D->getNameAsString();

        // Debug
        llvm::outs() << "(Rewriting) considering structure: " << structName << "\n";

        StructReordering* structReordering = this->semanticData->getStructReordering();
        if (structReordering->isInStructReorderingMap(structName)) {

            // We make sure we do not rewrite some structure that has already
            // been rewritten.
            if (structReordering->hasBeenRewritten(structName)) {
                llvm::outs() << "(Rewriting) structure: "<< structName << " has been rewritten already. \n";
                return true;
            }

            // Debug
            llvm::outs() << "(Rewriting) this structure should be rewritten!\n";

            // We need to rewrite it.
            StructData* structData = structReordering->getStructReorderings()[structName];

            // We obtain the fields data of the structure.
            std::vector<FieldData> fieldData = structData->getFieldData();

            // We rewrite the fields.
            {
                int i = 0;
                std::vector<FieldData>::iterator it;
                FieldData found; // Original field at a particular position i.
                FieldData substitute; // The field which should come at the 'found' field's position.

                // All fields need to be rewritten.
                while (i < fieldData.size()) {

                    // Search for the original field at this position.
                    for (it = fieldData.begin(); it != fieldData.end(); ++it) {
                        if (it->position == i) {
                            found = *it;
                            break;
                        }
                    }

                    // We get the substitute field.
                    substitute = fieldData[i];

                    // The actual rewriting of the source code.
                    this->rewriter->ReplaceText(found.sourceRange, substitute.fieldType + " " + substitute.fieldName);

                    // Debug
                    llvm::outs() << "(Rewriting) : field pos: " << i << " substitute: " << substitute.fieldType << " " << substitute.fieldName << "\n";

                    // We rewrote a field.
                    i++;
                }
            }

            // We add this structure to the set of rewritten structures.
            structReordering->structRewritten(structName);
        }
    }
    return true;
}


// Override this to call our SemanticAnalyser on the entire source file.
void SemanticAnalyserASTConsumer::HandleTranslationUnit(ASTContext &Context) {
    /* We can use ASTContext to get the TranslationUnitDecl, which is
         a single Decl that collectively represents the entire source file */

     // Visitor depends on the fact we are analysing or not.
     if (this->analysis) {
         this->visitorAnalysis->TraverseDecl(Context.getTranslationUnitDecl());
     } else {
         this->visitorRewriter->TraverseDecl(Context.getTranslationUnitDecl());
     }
}

// Semantic analyser frontend action: action that will start the consumer.
ASTConsumer* SemanticAnalyserFrontendAction::CreateASTConsumer(CompilerInstance &CI, StringRef file) {
    return new SemanticAnalyserASTConsumer(&CI, this->semanticData, this->rewriter, this->analysis);
}

// Function used to create the semantic analyser frontend action.
FrontendAction* SemanticAnalyserFrontendActionFactory::create() {
    return new SemanticAnalyserFrontendAction(this->semanticData, this->rewriter, this->analysis);
}
