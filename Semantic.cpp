#include "Semantic.h"
#include "StructReordering.h"
#include "SWCReordering.h"
#include "FPReordering.h"

#include "clang/Lex/Lexer.h"

using namespace clang;
using namespace llvm;

// Obtain source information corresponding to a statement.
std::string location2str(SourceLocation begin, SourceLocation end, SourceManager* sm, const LangOptions* lopt) {
    SourceLocation b(begin), _e(end);
    if (b.isMacroID())
        b = sm->getSpellingLoc(b);
    if (_e.isMacroID())
        _e = sm->getSpellingLoc(_e);
    clang::SourceLocation e(clang::Lexer::getLocForEndOfToken(_e, 0, *sm, *lopt));
    return std::string(sm->getCharacterData(b),
        sm->getCharacterData(e)-sm->getCharacterData(b));
}

// Function used to create the semantic analyser frontend action.
FrontendAction* SemanticFrontendActionFactory::create() {

    // We create a new instance of the frontend action.
    return new SemanticFrontendAction(this->semanticData, this->rewriter, this->version, this->outputPrefix, this->baseDirectory, this->transType, this->phaseType);
}

// Semantic frontend action: action that will start the consumer.
ASTConsumer* SemanticFrontendAction::CreateASTConsumer(CompilerInstance &CI, StringRef file) {

    // We create a new AST consumer based on the type of transformation.
    if (this->transType == Transformation::StructReordering) {
        return new StructReorderingASTConsumer(&CI, this->semanticData, this->rewriter, this->baseDirectory, this->phaseType);
    } else if (this->transType == Transformation::FPReordering) {
        return new FPReorderingASTConsumer(&CI, this->semanticData, this->rewriter, this->baseDirectory, this->phaseType);
    } else if (this->transType == Transformation::SWCReordering) {
        return new SWCReorderingASTConsumer(&CI, this->semanticData, this->rewriter, this->baseDirectory, this->phaseType);
    }
}


// Semantic analyser frontend action: action that will start the consumer.
void SemanticFrontendAction::EndSourceFileAction () {

    // We obtain the filename.
    std::string fileName = std::string(this->getCurrentFile().data());

    // Whenever we are NOT doing analysis we should write out the changes.
    if (this->phaseType == Phase::Rewrite && rewriter->buffer_begin() != rewriter->buffer_end()) {
        writeChangesToOutput();

        // We need to clear the rewriter's modifications.
        rewriter->undoChanges();
    }
}

// Method which is used to write the changes
void SemanticFrontendAction::writeChangesToOutput() {

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
        std::string fileName = fileNameStr.substr(fileNameStr.find(baseDirectory) + baseDirectory.length()); /* until the end automatically... */

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
