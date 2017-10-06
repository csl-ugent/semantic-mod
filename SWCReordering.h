#ifndef _SWCREORDERING
#define _SWCREORDERING

#include "Semantic.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"

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

class SWCReordering : public Reordering<SwitchUnique, SwitchData> {
public:
    explicit SWCReordering(const std::string& bd, const std::string& od) : Reordering(bd, od) {}
};

void swcreordering(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfReorderings);

#endif
