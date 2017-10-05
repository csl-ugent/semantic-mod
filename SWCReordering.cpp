#include "SWCReordering.h"
#include "SemanticUtil.h"

#include "clang/Tooling/Tooling.h"

#include "json.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

// AST visitor, used for analysis.
bool SWCReorderingAnalyser::VisitSwitchStmt(clang::SwitchStmt* CS) {
    const SwitchUnique candidate(CS, astContext);
    const std::string& fileName = candidate.getFileName();

    // We make sure the file is contained in our base directory...
    if (fileName.find(reordering.baseDirectory) == std::string::npos)
        return true;

    SwitchData& data = reordering.candidates[candidate];
    if (data.valid && data.empty())
    {
        llvm::outs() << "Found valid candidate: " << candidate.getName() << "\n";
    }

    // DEBUG
    llvm::outs() << "Found valid switch case: " << "\n";

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
void swcreordering(ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings) {
    // We run the analysis phase.
    SWCReordering reordering(baseDirectory, outputDirectory);
    Tool->run(new AnalysisFrontendActionFactory<SWCReordering, SWCReorderingAnalyser>(reordering));

    // We determine the amount of reorderings we are going to make.
    //int amount = amountOfReorderings;
    //int amountChosen = 0;
    // In case we have at least one switch case we should be able
    // to infinite values to XOR with.

    Tool->run(new RewritingFrontendActionFactory<SWCReordering, SWCReorderingRewriter>(reordering, 0));
}
