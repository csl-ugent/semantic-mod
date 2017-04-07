#include "SemanticAnalyser.h"
#include "clang/Lex/Lexer.h"

using namespace clang;
using namespace llvm;

std::string stmt2str(clang::Stmt *stm, SourceManager* sm, const LangOptions* lopt) {
    clang::SourceLocation b(stm->getLocStart()), _e(stm->getLocEnd());
    if (b.isMacroID())
        b = sm->getSpellingLoc(b);
    if (_e.isMacroID())
        _e = sm->getSpellingLoc(_e);
    clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, *sm, *lopt));
    return std::string(sm->getCharacterData(b),
        sm->getCharacterData(e)-sm->getCharacterData(b));
}

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
            return true; // Structure already analysed...
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
                return true;
            }
        } else {

            // Invalid struct to analyse... (header name cannot be found?)
            return true;
        }
        llvm::outs() << "Filename: " << fileNameStr << "\n";

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

// AST visitor, used for rewriting.
bool SemanticRewriter::VisitDeclStmt(clang::DeclStmt *DeclStmt){
    // We need to know if the type is a struct.
    // The type of struct is what we are actually reordering.
    // Add information to structreordering:
    // - Structure that needs to have init function.
    // - + all the structure information.

    clang::DeclStmt::decl_iterator it;
    for(it = DeclStmt->decl_begin(); it != DeclStmt->decl_end(); ++it) {

        Decl* decl = *it;

        // We check if the Decl is a VarDecl.
        if (VarDecl* D = dyn_cast<VarDecl>(decl)) {

            // Make sure the VarDeclr has an initializer.
            if (D->hasInit()) {

                // The initializer was actually an InitListExpr.
                if (InitListExpr* init = dyn_cast<InitListExpr>(D->getInit())) {

                    // We obtain the type information.
                    QualType qualType = D->getType();
                    const Type* type = qualType.getTypePtrOrNull();

                    // We need to make sure the type is a struct and the struct type
                    // Is actually rewritten.
                    if (type != NULL && type->isStructureType()) {

                        // We try to obtain the TagDecl.
                        TagDecl* tagDecl = type->getAsTagDecl();

                        if (tagDecl != NULL) {

                            // We analyse the structure name.
                            std::string structName = tagDecl->getNameAsString();
                            std::string identifierName = D->getNameAsString();

                            // We check if this structure is being reordened or not.
                            StructReordering* structReordering = this->semanticData->getStructReordering();
                            if (structReordering->isInStructReorderingMap(structName)) {

                                // We have to rewrite this type of initialization.
                                llvm::outs() << "Found var declr: " << D->getNameAsString() << " type: " << D->getType().getAsString() << '\n';
                                llvm::outs() << "Type info: " << tagDecl->getNameAsString() << "\n";

                                // We obtain the struct data/field data for the given struct.
                                StructData* structData = structReordering->getStructMap()[structName];
                                std::vector<FieldData> fieldData = structData->getFieldData();

                                // Building of replacement string.
                                std::stringstream replacements;
                                replacements << " {";

                                clang::InitListExpr::iterator it;
                                int i = 0; // Field # we are iterating over.
                                for (it = init->begin(); it != init->end(); ++it) {

                                    // Obtain the corresponding statement.
                                    Stmt* initStmt = *it;

                                    // The content of the statement.
                                    std::string fieldContent = stmt2str(initStmt, &astContext->getSourceManager(), &astContext->getLangOpts());

                                    // Field data for this statement.
                                    FieldData data = fieldData[i];
                                    replacements << identifierName << "." << data.fieldName << " = " << fieldContent << ";";

                                    // Advance field #.
                                    i++;
                                }

                                // Make it a valid string.
                                replacements << "} // Automatically generated by semantic-mod tool.";
                                std::string replacement = replacements.str();

                                this->rewriter->RemoveTextAfterToken(SourceRange(D->getLocation(), init->getRBraceLoc()));
                                this->rewriter->InsertTextAfterToken(DeclStmt->getEndLoc(), replacement);
                            }
                        }
                    }
                }
            }
        }

        //this->rewriter->InsertTextAfterToken(DeclStmt->getEndLoc(), "{operation1.id=1377;operation1.value=1377.0;operation1.name=\"LUL\"}");
    }




    //this->rewriter->ReplaceText(D->getSourceRange(), "REPLACED");
    //this->rewriter->ReplaceText(SourceRange(S->getLBraceLoc(), S->getRBraceLoc()), "REPLACEMENT");
    return true;
}

// AST visitor, used for rewriting the source code.
bool SemanticRewriter::VisitRecordDecl(RecordDecl *D) {

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
    return new SemanticAnalyserASTConsumer(&CI, this->semanticData, this->rewriter, this->analysis, this->baseDirectory);
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
        outputFile.write(output.c_str(), output.length());
        outputFile.close();
    }
}

// Function used to create the semantic analyser frontend action.
FrontendAction* SemanticAnalyserFrontendActionFactory::create() {
    return new SemanticAnalyserFrontendAction(this->semanticData, this->rewriter, this->analysis, this->version, this->outputPrefix, this->baseDirectory);
}
