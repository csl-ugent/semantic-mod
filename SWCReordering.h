#ifndef _SWCREORDERING
#define _SWCREORDERING

#include "Semantic.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "llvm/ADT/MapVector.h"

#include <string>

class SwitchUnique {
        std::string name;
        std::string fileName;

    public:
        SwitchUnique(const clang::SwitchStmt* D, const clang::ASTContext& astContext) {}
        std::string getName() const { return name;}
        std::string getFileName() const { return fileName;}
        bool operator== (const SwitchUnique& other) const
        {
            // If the names differ, it can't be the same
            if (name != other.name)
                return false;

            // If both are local, yet from a different file, they are different
            if (fileName != other.fileName)
                return false;

            return true;
        }
        bool operator< (const SwitchUnique& other) const
        {
            // Create string representation so we can correctly compare
            std::string left = name + ":" + fileName;
            std::string right = other.name + ":" + other.fileName;

            return left < right;
        }
};

class SwitchData {
    public:
        bool valid;

        SwitchData(bool valid = true) : valid(valid) {}
        bool empty() {
          return true;
        }
};

// Struct which describes the transformation
struct SwitchTransformation {
    SwitchUnique target;
    std::vector<unsigned> ordering;
    SwitchTransformation(SwitchUnique target, std::vector<unsigned>& ordering)
        : target(target), ordering(ordering) {}
};

class SWCReordering : public Reordering {
public:
    explicit SWCReordering(const std::string& bd) : Reordering(bd) {}

    // Map containing all information regarding candidates.
    llvm::MapVector<SwitchUnique, SwitchData, std::map<SwitchUnique, unsigned>> candidates;
    void invalidateCandidate(const SwitchUnique& candidate, const std::string& reason) {
        llvm::outs() << "Invalidate candidate: " << candidate.getName() << ". Reason: " << reason << ".\n";
        SwitchData& data = candidates[candidate];
        data.valid = false;
    }

    // The transformation to apply
    SwitchTransformation* transformation;
};

// Declaration of used methods.
void swcreordering(clang::tooling::ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings);

// Semantic analyser, willl analyse different nodes within the AST.
class SWCReorderingAnalyser : public clang::RecursiveASTVisitor<SWCReorderingAnalyser> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    SWCReordering& reordering;
public:
    explicit SWCReorderingAnalyser(clang::ASTContext& Context, SWCReordering& reordering)
      : astContext(Context), reordering(reordering) { }

    // We want to investigate switch statements.
    bool VisitSwitchStmt(clang::SwitchStmt* CS);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class SWCReorderingRewriter : public clang::RecursiveASTVisitor<SWCReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    SWCReordering& reordering;
    clang::Rewriter& rewriter;
public:
    explicit SWCReorderingRewriter(clang::ASTContext& Context, SWCReordering& reordering, clang::Rewriter& rewriter)
      : astContext(Context), reordering(reordering), rewriter(rewriter)
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We want to rewrite switch statements.
    bool VisitSwitchStmt(clang::SwitchStmt* CS);
};

#endif
