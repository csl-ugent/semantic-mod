#ifndef _STRUCTREORDERING
#define _STRUCTREORDERING

#include "SemanticData.h"
#include "SemanticVisitors.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"

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
        class Analyser : public SemanticAnalyser<StructUnique>, public clang::RecursiveASTVisitor<Analyser> {
            public:
                explicit Analyser(clang::ASTContext& Context, const MetaData& metadata, Candidates<StructUnique>& candidates)
                    : SemanticAnalyser(Context, metadata, candidates) {}

                // We want to investigate all possible struct declarations and uses
                void detectStructsRecursively(const clang::Type* origType);
                bool VisitRecordDecl(clang::RecordDecl* D);
                bool VisitVarDecl(clang::VarDecl* D);
        };
};

// Semantic Rewriter, will rewrite source code based on the AST.
class StructReorderingRewriter : public SemanticRewriter, public clang::RecursiveASTVisitor<StructReorderingRewriter> {
    private:
        const ReorderingTransformation& transformation;
    public:
        explicit StructReorderingRewriter(clang::ASTContext& Context, const Transformation& transformation, clang::Rewriter& rewriter)
            : SemanticRewriter(Context, rewriter), transformation(static_cast<const ReorderingTransformation&>(transformation)) {}

        // We want to investigate top-level things.
        bool VisitRecordDecl(clang::RecordDecl* D);

        typedef StructUnique Target;
        typedef ReorderingTransformation TransformationType;
};

// Semantic Rewriter, will rewrite source code based on the AST.
class StructInsertionRewriter : public SemanticRewriter, public clang::RecursiveASTVisitor<StructInsertionRewriter> {
    private:
        const InsertionTransformation& transformation;
    public:
        explicit StructInsertionRewriter(clang::ASTContext& Context, const Transformation& transformation, clang::Rewriter& rewriter)
            : SemanticRewriter(Context, rewriter), transformation(static_cast<const InsertionTransformation&>(transformation)) {}

        // We want to investigate top-level things.
        bool VisitRecordDecl(clang::RecordDecl* D);

        typedef StructUnique Target;
        typedef InsertionTransformation TransformationType;
};
#endif
