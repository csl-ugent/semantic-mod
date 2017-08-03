#include "SemanticAnalyser.h"

using namespace clang;
using namespace llvm;

// Structure visitor.
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

        // Rewriter example...
        this->rewriter->ReplaceText(sourceRange, "replaced");
    }
    return true;
}

// Override this to call our SemanticAnalyser on the entire source file.
void SemanticAnalyserASTConsumer::HandleTranslationUnit(ASTContext &Context) {
    /* We can use ASTContext to get the TranslationUnitDecl, which is
         a single Decl that collectively represents the entire source file */
    this->visitor->TraverseDecl(Context.getTranslationUnitDecl());
}

// Semantic analyser frontend action: action that will start the consumer.
ASTConsumer* SemanticAnalyserFrontendAction::CreateASTConsumer(CompilerInstance &CI, StringRef file) {
    return new SemanticAnalyserASTConsumer(&CI, this->semanticData, this->rewriter);
}

// Function used to create the semantic analyser frontend action.
FrontendAction* SemanticAnalyserFrontendActionFactory::create() {
    return new SemanticAnalyserFrontendAction(this->semanticData, this->rewriter);
}
