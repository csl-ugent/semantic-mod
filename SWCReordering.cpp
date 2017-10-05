#include "SWCReordering.h"
#include "SemanticUtil.h"

using namespace clang;
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

void swcreordering(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfReorderings) {
    reorder<SWCReordering, SWCReorderingAnalyser, SWCReorderingRewriter, SwitchUnique, SwitchTransformation>(Tool, baseDirectory, outputDirectory, numberOfReorderings);
}
