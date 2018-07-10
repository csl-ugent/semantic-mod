#ifndef _STRUCTREORDERING
#define _STRUCTREORDERING

#include "SemanticData.h"
#include "SemanticFrontendAction.h"

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


        // Semantic analyser, willl analyse different nodes within the AST.
        class Analyser : public clang::RecursiveASTVisitor<Analyser> {
            private:
                clang::ASTContext& astContext; // Used for getting additional AST info.
                const MetaData& metadata;
                Candidates<StructUnique>& candidates;
            public:
                explicit Analyser(clang::ASTContext& Context, const MetaData& metadata, Candidates<StructUnique>& candidates)
                    : astContext(Context), metadata(metadata), candidates(candidates) { }

                // We want to investigate all possible struct declarations and uses
                void detectStructsRecursively(const clang::Type* origType);
                bool VisitRecordDecl(clang::RecordDecl* D);
                bool VisitVarDecl(clang::VarDecl* D);
        };
};

// Semantic Rewriter, will rewrite source code based on the AST.
class StructReorderingRewriter : public clang::RecursiveASTVisitor<StructReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    const Transformation& transformation;
    clang::Rewriter& rewriter;
public:
    explicit StructReorderingRewriter(clang::ASTContext& Context, const Transformation& transformation, clang::Rewriter& rewriter)
      : astContext(Context), transformation(transformation), rewriter(rewriter)
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We want to investigate top-level things.
    bool VisitRecordDecl(clang::RecordDecl* D);

    typedef StructUnique Target;
};

#endif
