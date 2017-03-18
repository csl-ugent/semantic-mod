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
            //D->dump();
            //llvm::outs() << "\n";
        }

        // We create new structData information.
        StringRef fileNameRef = astContext->getSourceManager().getFilename(
            D->getLocation());

        std::string fileNameStr = std::string(fileNameRef.data());
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

// Semantic analyser frontend action: action that will start the consumer.
void SemanticAnalyserFrontendAction::EndSourceFileAction () {

    // Whenever we are NOT doing analysis we should write out the changes.
    if (!this->analysis && rewriter->buffer_begin() != rewriter->buffer_end()) {
        writeChangesToOutput();

        // We need to clear the rewriter's modifications.
        rewriter->undoChanges();
    }
}

// Method which is used to write the changes
void SemanticAnalyserFrontendAction::writeChangesToOutput() {

    // We construct the full output directory.
    std::stringstream s;
    s << this->outputPrefix << "v" << this->version;
    std::string fullPath = s.str();

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

        StringRef fileNameRef = rewriter->getSourceMgr().getFileEntryForID(I->first)->getName();
        std::string fileNameStr = std::string(fileNameRef.data());
        llvm::outs() << "Obtained filename: " << fileNameStr << "\n";
        std::string fileName = fileNameStr.substr(fileNameStr.find(baseDirectory) + baseDirectory.length() + 1); /* until the end automatically... */

        // Optionally create required subdirectories.
        {
            int subDirectoryPos = fileName.find_last_of("/\\");
            if (subDirectoryPos != std::string::npos) {
                llvm::outs() << "Creating subdirectories..." << "\n";
                std::string subdirectories = fileName.substr(0, fileName.find_last_of("/\\"));
                llvm::outs() <<  "Extracted subdirectory path: " << subdirectories << "\n";
                system(("mkdir -p " + fullPath + "/" + subdirectories).c_str());
            }
        }

        // Write changes to the file.
        std::string outputPath = fullPath + "/" + fileName;

        std::string output = std::string(I->second.begin(), I->second.end());
        outputFile.open(outputPath.c_str());
        StringRef MB = rewriter->getSourceMgr().getBufferData(I->first);
        std::string content = std::string(MB.data());
        llvm::outs() << "Changes: " << output.c_str() << "\n";

        outputFile.write(output.c_str(), output.length());
        outputFile.close();
    }
}

// Function used to create the semantic analyser frontend action.
FrontendAction* SemanticAnalyserFrontendActionFactory::create() {
    return new SemanticAnalyserFrontendAction(this->semanticData, this->rewriter, this->analysis, this->version, this->outputPrefix, this->baseDirectory);
}
