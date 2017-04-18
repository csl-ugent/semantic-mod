#include "SWCReordering.h"
#include "json.h"
#include "SemanticUtil.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;
using namespace llvm;
using namespace clang::tooling;

// Override this to call our SemanticAnalyser on the entire source file.
void SWCReorderingASTConsumer::HandleTranslationUnit(ASTContext &Context) {

     // Visitor depends on the phase we are in.
     if (phaseType == Phase::Analysis) {
         this->visitorAnalysis->TraverseDecl(Context.getTranslationUnitDecl());
     } else if (phaseType == Phase::Rewrite) {
         this->visitorRewriter->TraverseDecl(Context.getTranslationUnitDecl());
     }
}

// AST visitor, used for analysis.
bool SWCReorderingAnalyser::VisitSwitchStmt(clang::SwitchStmt* CS) {

    // We keep track of the amount of switch statements.
    SWCReordering* swcReordering = semanticData->getSWCReordering();

    // We obtain the location encoding of this switch statement.
    unsigned location = CS->getSwitchLoc().getRawEncoding();

    // We check if we already visited this switch statement or not.
    if (swcReordering->isInSwitchMap(location)) {
        return true; // Switch case already visited.
    }

    // We create new switchData information.
    StringRef fileNameRef = astContext->getSourceManager().getFilename(
        CS->getSwitchLoc());

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

        // Invalid switch to analyse... (header name cannot be found?)
        return true;
    }

    // DEBUG
    llvm::outs() << "Found valid switch case: " << location << "\n";

    // New switch data object.
    SwitchData* switchData = new SwitchData(location, fileNameStr);

    // We add it to the SwitchData map.
    swcReordering->addSwitchData(location, switchData);

    // Continue AST traversal.
    return true;
}


// AST rewriter, used for rewriting.
bool SWCReorderingRewriter::VisitSwitchStmt(clang::SwitchStmt* CS) {
    llvm::outs() << "Rewriting switch statement!\n";
    llvm::outs() << "Location: " << CS->getSwitchLoc().getRawEncoding() << "\n";

    // We keep track of the amount of switch statements.
    // (increase amount of seen switch cases)
    return true;
}

// Method used for the switch case reordering semantic transformation.
void swcreordering(SemanticData* semanticData, Rewriter* rewriter, ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings) {

    // Switch case reordering information.
    SWCReordering* swcReordering = semanticData->getSWCReordering();

    // Debug information.
    llvm::outs() << "Phase 1: Switch Case Analysis\n";

    // We run the analysis phase.
    Tool->run(new SemanticFrontendActionFactory(semanticData, rewriter, baseDirectory, Transformation::SWCReordering, Phase::Analysis));

    // We determine the amount of reorderings we are going to make.
    int amount = amountOfReorderings;
    int amountChosen = 0;
    if (swcReordering->getSwitchMap().size() == 0) { // No switch cases...
        amount = 0;
    } // In case we have at least one switch case we should be able
    // to infinite values to XOR with.

    Tool->run(new SemanticFrontendActionFactory(semanticData, rewriter, baseDirectory, Transformation::SWCReordering, Phase::Rewrite));
}
