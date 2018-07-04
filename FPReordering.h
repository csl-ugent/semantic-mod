#ifndef _FPREORDERING
#define _FPREORDERING

#include "Semantic.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"

#include <string>
#include <vector>

class FunctionUnique : public TargetUnique {
    public:
        FunctionUnique(const clang::FunctionDecl* D, const clang::ASTContext& astContext)
            : TargetUnique(D->getNameAsString(), astContext.getSourceManager().getFilename(D->getLocation()).str(), D->isGlobal()) {}

        class Data : public TargetUnique::Data {
            struct FunctionParam {
                std::string name;
                std::string type;

                FunctionParam(std::string name, std::string type) : name(name), type(type) {}
            };

            public:
            std::vector<FunctionParam> params;

            Data(bool valid = true) : TargetUnique::Data(valid) {}
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
};

class FPVersion : public Version<FunctionUnique> {
public:
    explicit FPVersion(const std::string& bd, const std::string& od) : Version(bd, od) {}
};

void fpreordering(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfVersions);

#endif
