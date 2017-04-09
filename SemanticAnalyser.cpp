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

std::string handleStructInitsRecursively(InitListExpr* init, const Type* type, std::string currentIdentifier, SemanticData* semanticData, ASTContext* astContext, bool* shouldBeRewritten, int currentDepth, int* arrayDepth, std::stringstream* arraySizeAddition) {

    // The replacement string we are building.
    std::stringstream replacementstr;

    // We check if the original declaration is a struct type.
    if (type != NULL && type->isStructureType()) {

        // Extra things coming....
        replacementstr << "\n";

        // We try to obtain the TagDecl.
        TagDecl* tagDecl = type->getAsTagDecl();

        if (tagDecl != NULL) {

            // Retrieve struct reordering information.
            StructReordering* structReordering = semanticData->getStructReordering();

            // We obtain the name of the struct.
            std::string structName = tagDecl->getNameAsString();

            // We check if this is a structure and if this structure is being reordened or not.
            if (!(*shouldBeRewritten) && structReordering->isInStructReorderingMap(structName)) {
                *shouldBeRewritten = true;
            }

            clang::InitListExpr::iterator it;
            RecordDecl::field_iterator jt = type->getAsStructureType()->getDecl()->field_begin();

            for (it = init->begin(); it != init->end(); ++it, ++jt) {

                // Obtain the corresponding statement.
                Stmt* initStmt = *it;

                // We need to evaluate the current field we are considering.
                FieldDecl* fieldTwo = *jt;

                // We check if this expression is nested or not.
                if (InitListExpr* initTwo = dyn_cast<InitListExpr>(initStmt)) {

                    // We obtain the type information.
                    const Type* typeTwo = (jt->getType()).getTypePtrOrNull();

                    // Handle the initlistexpr recursively.
                    replacementstr << handleStructInitsRecursively(initTwo, typeTwo, currentIdentifier + "." + fieldTwo->getNameAsString(), semanticData, astContext, shouldBeRewritten, currentDepth+1, arrayDepth, arraySizeAddition);

                } else { // Regular field.

                    // The content of the statement.
                    std::string fieldContent = stmt2str(initStmt, &astContext->getSourceManager(), &astContext->getLangOpts());

                    // We update the string with a new statement.
                    if (fieldContent != "") {
                        replacementstr << currentIdentifier << "." << fieldTwo->getNameAsString() << " = " << fieldContent << ";";
                    }
                }
            }
        }
    } else if (type != NULL && type->isUnionType()) { // Handle unions.

        clang::InitListExpr::iterator it;
        for (it = init->begin(); it != init->end(); ++it) { // Should only be one though...

            // Obtain the corresponding statement.
            Expr* initExpr = dyn_cast<Expr>(*it);

            // NULL check.
            if (initExpr != NULL) {

                // We obtain the corresponding type information.
                QualType exprType = initExpr->getType();

                // Find a FieldDecl with this type.
                {
                    RecordDecl::field_iterator jt;
                    for (jt = type->getAsUnionType()->getDecl()->field_begin(); jt != type->getAsUnionType()->getDecl()->field_end(); ++jt) {

                        // We need to evaluate the current field we are considering.
                        FieldDecl* fieldTwo = *jt;

                        // We obtain the type information.
                        const QualType typeTwo = jt->getType();

                        // Found field with same type!
                        if (exprType == typeTwo) {

                            // We check if this expression is nested or not.
                            if (InitListExpr* initTwo = dyn_cast<InitListExpr>(initExpr)) {

                                // We obtain the type information.
                                const Type* typeTwoInf = typeTwo.getTypePtrOrNull();

                                // Handle the initlistexpr recursively.
                                replacementstr << handleStructInitsRecursively(initTwo, typeTwoInf, currentIdentifier + "." + fieldTwo->getNameAsString(), semanticData, astContext, shouldBeRewritten, currentDepth+1, arrayDepth, arraySizeAddition);

                            } else { // Regular field.

                                // The content of the statement.
                                std::string fieldContent = stmt2str(initExpr, &astContext->getSourceManager(), &astContext->getLangOpts());

                                // We update the string with a new statement.
                                if (fieldContent != "") {
                                    replacementstr << currentIdentifier << "." << fieldTwo->getNameAsString() << " = " << fieldContent << ";";
                                }
                            }

                            // Break out of the same type field looking loop.
                            break;
                        }
                    }
                }
            } else {
                llvm::outs() << "Union doesn't have expression field!\n"; // SHOULD NOT HAPPEN.
            }
        }

    }  else if (type != NULL && type->isArrayType()) { // Handle arrays.

        if (type->isConstantArrayType() && currentDepth == (*arrayDepth)) {
            const ConstantArrayType* constType = dyn_cast<ConstantArrayType>(type);
            (*arraySizeAddition) << "[" << *((constType->getSize()).getRawData()) << "]";
            (*arrayDepth)++;
        }

        // Iterate over fields in array.
        clang::InitListExpr::iterator it;
        int i = 0;
        for (it = init->begin(); it != init->end(); ++it) {

            // Obtain the corresponding statement.
            Stmt* initStmt = *it;

            // We obtain the type information.
            const Type* typeTwo = (type->getAsArrayTypeUnsafe()->getElementType()).getTypePtrOrNull();

            // We check if this expression is nested or not.
            if (InitListExpr* initTwo = dyn_cast<InitListExpr>(initStmt)) {

                // Handle the initlistexpr recursively.
                std::stringstream newIdentifier;
                newIdentifier << currentIdentifier << "[" << i << "]";
                replacementstr << handleStructInitsRecursively(initTwo, typeTwo, newIdentifier.str(), semanticData, astContext, shouldBeRewritten, currentDepth+1, arrayDepth, arraySizeAddition);
            }
            i++;
        }
    }

    // We return the string itself.
    return replacementstr.str();
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

// AST visitor, used for analysis.
bool SemanticTransformationAnalyser::VisitTranslationUnitDecl(clang::TranslationUnitDecl* TD) {

    clang::DeclContext::decl_iterator it;
    for(it = TD->decls_begin(); it != TD->decls_end(); ++it) {

        // Current delcaration we are considering.
        Decl* decl = *it;

        // We check if the Decl is a FunctionDecl.
        if (FunctionDecl* FD = dyn_cast<FunctionDecl>(decl)) {

            // We try to find the function name "main".
            std::string functionName = FD->getNameAsString();
            if (functionName == "main") {

                // Try to find the file in which this function is located.
                StringRef fileNameRef = astContext->getSourceManager().getFilename(FD->getLocation());
                std::string fileNameStr = std::string(fileNameRef.data());

                // Debug
                llvm::outs() << "Found main function in file: " << fileNameStr << "\n";

                // Set the main file path.
                semanticData->setMainFilePath(fileNameStr);
            }
        // We look for VarDecl's.
        } else if (VarDecl* D = dyn_cast<VarDecl>(decl)) {

            // Whenever he has an initializer.
            if (D->hasInit()) {

                // Identifier name.
                std::string identifierName = D->getNameAsString();

                // Variable to keep track if we should rewrite this or not.
                bool shouldBeRewritten = false;

                // The replacement string we are building (in case it should be rewritten).
                std::stringstream replacementstr;
                std::stringstream arraySizeAddition;
                int arrayDepth = 0;
                replacementstr << "\n\n{";

                // Initializer was actually an InitListExpr.
                if (InitListExpr* init = dyn_cast<InitListExpr>(D->getInit())) {

                    // We obtain the type information.
                    const Type* type = (D->getType()).getTypePtrOrNull();
                    replacementstr << handleStructInitsRecursively(init, type, identifierName, this->semanticData, this->astContext, &shouldBeRewritten, 0, &arrayDepth, &arraySizeAddition);
                }

                replacementstr << "\n} // Initialisation rewritten by semantic-mod tool. \n";

                // DEBUG.
                if (shouldBeRewritten) {
                    llvm::outs() << "(Structreordering) global variable: " << identifierName << " needs a transformation!\n";
                    llvm::outs() << "Replacement: " << replacementstr.str() << "\n";
                }
            }

        }
    }
    return true;
}

// AST visitor, used for rewriting.
bool SemanticRewriter::VisitDeclStmt(clang::DeclStmt *DeclStmt) {

    // Vars used.
    clang::DeclStmt::decl_iterator it;
    Decl* decl;
    VarDecl* D;
    InitListExpr* init;

    // Iterate over all declarations in this DeclStmt.
    for(it = DeclStmt->decl_begin(); it != DeclStmt->decl_end(); ++it) {

        // Current declaration.
        decl = *it;
        D = dyn_cast<VarDecl>(decl);

        // We check if the Decl is a VarDecl / make sure VarDeclr has an initializer.
        if (D && D->hasInit()) {

            // Identifier name.
            std::string identifierName = D->getNameAsString();

            // Variable to keep track if we should rewrite this or not.
            bool shouldBeRewritten = false;

            // The replacement string we are building (in case it should be rewritten).
            std::stringstream replacementstr;
            std::stringstream arraySizeAddition;
            int arrayDepth = 0;
            replacementstr << "\n{";

            // Initializer was actually an InitListExpr.
            if (init = dyn_cast<InitListExpr>(D->getInit())) {

                // We obtain the type information.
                const Type* type = (D->getType()).getTypePtrOrNull();
                replacementstr << handleStructInitsRecursively(init, type, identifierName, this->semanticData, this->astContext, &shouldBeRewritten, 0, &arrayDepth, &arraySizeAddition);

                // Extra ending replacement.
                replacementstr << "\n} // Initialisation rewritten by semantic-mod tool. \n";

                // Rewrite operations and DEBUG.
                if (shouldBeRewritten) {

                    // Rewriting.
                    this->rewriter->RemoveTextAfterToken(SourceRange(D->getLocation(), init->getRBraceLoc()));

                    if (type != NULL && type->isArrayType() && type->isConstantArrayType()) { // Handle arrays (special case).
                        this->rewriter->InsertTextAfterToken(D->getLocation(), arraySizeAddition.str());
                    }

                    this->rewriter->InsertTextAfterToken(DeclStmt->getEndLoc(), replacementstr.str());
                }
            }
        }
    }
    return true;
}


// AST visitor, used for rewriting the source code.
bool SemanticRewriter::VisitRecordDecl(RecordDecl *D) {

    // We detect structures here.
    if (D->isStruct()) {

        // We analyse the structure name.
        std::string structName = D->getNameAsString();

        StructReordering* structReordering = this->semanticData->getStructReordering();
        if (structReordering->needsToBeRewritten(structName)) {

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
     if (this->type == ANALYSIS) {
         this->visitorAnalysis->TraverseDecl(Context.getTranslationUnitDecl());
     } else if (this->type == TRANSFORMATION_ANALYSIS){
         this->transformationAnalyser->TraverseDecl(Context.getTranslationUnitDecl());
     } else if (this->type == REWRITE) {
         this->visitorRewriter->TraverseDecl(Context.getTranslationUnitDecl());
     }
}

// Semantic analyser frontend action: action that will start the consumer.
ASTConsumer* SemanticAnalyserFrontendAction::CreateASTConsumer(CompilerInstance &CI, StringRef file) {
    return new SemanticAnalyserASTConsumer(&CI, this->semanticData, this->rewriter, this->type, this->baseDirectory);
}

// Semantic analyser frontend action: action that will start the consumer.
void SemanticAnalyserFrontendAction::EndSourceFileAction () {

    // We obtain the filename.
    std::string fileName = std::string(this->getCurrentFile().data());

    // Whenever we are NOT doing analysis we should write out the changes.
    if (this->type == REWRITE && rewriter->buffer_begin() != rewriter->buffer_end()) {
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
    return new SemanticAnalyserFrontendAction(this->semanticData, this->rewriter, this->type, this->version, this->outputPrefix, this->baseDirectory);
}
