#ifndef _STRUCTREORDERING
#define _STRUCTREORDERING

#include "Semantic.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"

#include <string>
#include <vector>

class StructUnique : public TargetUnique {
    public:
        StructUnique(const clang::RecordDecl* D, const clang::ASTContext& astContext)
            : TargetUnique(D->getNameAsString(), astContext.getSourceManager().getFilename(D->getLocation()).str())
        {
            // If the struct does not have a name, try to get one from when it was used in a typedef
            if (name.empty()) {
                if (const clang::TypedefNameDecl* T = D->getTypedefNameForAnonDecl()) {
                    name = T->getNameAsString();
                }
            }
        }

        class Data : public TargetUnique::Data {
            struct StructField {
                std::string name;
                std::string type;

                StructField(std::string name, std::string type) : name(name), type(type) {}
            };

            public:
            std::vector<StructField> fields;

            Data(bool valid = true) : TargetUnique::Data(valid) {}
            void addFields(clang::RecordDecl* D)
            {
                for(auto field : D->fields())
                {
                    fields.emplace_back(field->getNameAsString(), field->getType().getAsString());
                }
            }
            bool empty() const {
                return fields.empty();
            }
            Json::Value getJSON(const std::vector<unsigned>& ordering) const {
                Json::Value items(Json::arrayValue);
                for (unsigned iii = 0; iii < fields.size(); iii++) {
                    const StructField& param = fields[ordering[iii]];
                    Json::Value v;
                    v["position"] = iii;
                    v["name"] = param.name;
                    v["type"] = param.type;
                    items.append(v);
                }
                return items;
            }
            unsigned nrOfItems() const {
                return fields.size();
            }
        };
};

class StructVersion : public Version<StructUnique> {
public:
    explicit StructVersion(const std::string& bd, const std::string& od) : Version(bd, od) {}
};

void structReordering(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfVersions);

#endif
