#ifndef _FPREORDERING
#define _FPREORDERING

#include "Semantic.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "llvm/ADT/MapVector.h"

#include <string>

class FunctionUnique : public TargetUnique {
    public:
        FunctionUnique(const clang::FunctionDecl* D, const clang::ASTContext& astContext)
            : TargetUnique(D->getNameAsString(), astContext.getSourceManager().getFilename(D->getLocation()).str(), D->isGlobal()) {}
};

class FunctionData : public TargetData {
    struct FunctionParam {
        std::string name;
        std::string type;

        FunctionParam(std::string name, std::string type) : name(name), type(type) {}
    };

    public:
        std::vector<FunctionParam> params;

        FunctionData(bool valid = true) : TargetData(valid) {}
        void addParams(clang::FunctionDecl* D)
        {
            for (unsigned iii = 0; iii < D->getNumParams(); iii++) {
                const clang::ParmVarDecl* param = D->getParamDecl(iii);
                params.emplace_back(param->getNameAsString(), param->getType().getAsString());
            }
        }
        bool empty() const {
          return params.empty();
        }
        Json::Value getJSON(const std::vector<unsigned>& ordering) const {
            Json::Value items(Json::arrayValue);
            for (unsigned iii = 0; iii < params.size(); iii++) {
                const FunctionParam& param = params[ordering[iii]];
                Json::Value v;
                v["position"] = iii;
                v["name"] = param.name;
                v["type"] = param.type;
                items.append(v);
            }
            return items;
        }
        unsigned nrOfItems() const {
            return params.size();
        }
};

// Struct which describes the transformation
struct FPTransformation {
    FunctionUnique target;
    std::vector<unsigned> ordering;
    FPTransformation(FunctionUnique target, std::vector<unsigned>& ordering)
        : target(target), ordering(ordering) {}
};

// Function parameter reordering semantic modification.
class FPReordering : public Reordering {
public:
    explicit FPReordering(const std::string& bd, const std::string& od) : Reordering(bd, od) {}

    // Map containing all information regarding different functions.
    llvm::MapVector<FunctionUnique, FunctionData, std::map<FunctionUnique, unsigned>> candidates;
    void invalidateCandidate(const FunctionUnique& candidate, const std::string& reason) {
        llvm::outs() << "Invalidate candidate: " << candidate.getName() << ". Reason: " << reason << ".\n";
        FunctionData& data = candidates[candidate];
        data.valid = false;
    }

    // The transformation to apply
    FPTransformation* transformation;
};

void fpreordering(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfReorderings);

#endif
