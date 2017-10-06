#ifndef _SWCREORDERING
#define _SWCREORDERING

#include "Semantic.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "llvm/ADT/MapVector.h"

#include <string>

class SwitchUnique : public TargetUnique {
    public:
        SwitchUnique(const clang::SwitchStmt* D, const clang::ASTContext& astContext)
            : TargetUnique("", "") {}
};

class SwitchData : public TargetData {
    public:
        SwitchData(bool valid = true) : TargetData(valid) {}
        bool empty() const {
          return true;
        }
        Json::Value getJSON(const std::vector<unsigned>& ordering) const {
            Json::Value items(Json::arrayValue);
            return items;
        }
        unsigned nrOfItems() const {
            return 0;
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
    explicit SWCReordering(const std::string& bd, const std::string& od) : Reordering(bd, od) {}

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

void swcreordering(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfReorderings);

#endif
