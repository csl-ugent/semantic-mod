#ifndef _STRUCTREORDERING
#define _STRUCTREORDERING

#include "Semantic.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/MapVector.h"

#include <string>

class StructUnique {
        std::string name;
        std::string fileName;

    public:
        StructUnique(const clang::RecordDecl* D, const clang::ASTContext& astContext)
          : fileName(astContext.getSourceManager().getFilename(D->getLocation()).str())
        {
            name = D->getNameAsString();

            // If the struct does not have a name, try to get one from when it was used in a typedef
            if (name.empty()) {
                if (const clang::TypedefNameDecl* T = D->getTypedefNameForAnonDecl()) {
                    name = T->getNameAsString();
                }
            }
        }
        std::string getName() const { return name;}
        std::string getFileName() const { return fileName;}
        bool operator== (const StructUnique& other) const
        {
            // If the names differ, it can't be the same
            if (name != other.name)
                return false;

            // If both are local, yet from a different file, they are different
            if (fileName != other.fileName)
                return false;

            return true;
        }
        bool operator< (const StructUnique& other) const
        {
            // Create string representation so we can correctly compare
            std::string left = name + ":" + fileName;
            std::string right = other.name + ":" + other.fileName;

            return left < right;
        }
};

class StructData {
    struct StructField {
        std::string name;
        std::string type;

        StructField(std::string name, std::string type) : name(name), type(type) {}
    };

    public:
        bool valid;
        std::vector<StructField> fields;

        StructData(bool valid = true) : valid(valid) {}
        void addFields(clang::RecordDecl* D)
        {
            for(auto field : D->fields())
            {
                fields.emplace_back(field->getNameAsString(), field->getType().getAsString());
            }
        }
        bool empty() {
          return fields.empty();
        }
        Json::Value getJSON(const std::vector<unsigned>& ordering) {
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
        unsigned nrOfItems() {
            return fields.size();
        }
};

// Struct which describes the transformation
struct StructTransformation {
    StructUnique target;
    std::vector<unsigned> ordering;
    StructTransformation(StructUnique target, std::vector<unsigned>& ordering)
        : target(target), ordering(ordering) {}
};

class StructReordering : public Reordering {
public:
    explicit StructReordering(const std::string& bd, const std::string& od) : Reordering(bd, od) {}

    // Map containing all information regarding candidates.
    llvm::MapVector<StructUnique, StructData, std::map<StructUnique, unsigned>> candidates;
    void invalidateCandidate(const StructUnique& candidate, const std::string& reason) {
        llvm::outs() << "Invalidate candidate: " << candidate.getName() << ". Reason: " << reason << ".\n";
        StructData& data = candidates[candidate];
        data.valid = false;
    }

    // The transformation to apply
    StructTransformation* transformation;
};

// Semantic analyser, willl analyse different nodes within the AST.
class StructReorderingAnalyser : public clang::RecursiveASTVisitor<StructReorderingAnalyser> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    StructReordering& reordering;
public:
    explicit StructReorderingAnalyser(clang::ASTContext& Context, StructReordering& reordering)
      : astContext(Context), reordering(reordering) { }

    // We want to investigate all possible struct declarations and uses
    void detectStructsRecursively(const clang::Type* origType);
    bool VisitRecordDecl(clang::RecordDecl* D);
    bool VisitVarDecl(clang::VarDecl* D);
};

// Semantic Rewriter, will rewrite source code based on the AST.
class StructReorderingRewriter : public clang::RecursiveASTVisitor<StructReorderingRewriter> {
private:
    clang::ASTContext& astContext; // Used for getting additional AST info.
    StructReordering& reordering;
    clang::Rewriter& rewriter;
public:
    explicit StructReorderingRewriter(clang::ASTContext& Context, StructReordering& reordering, clang::Rewriter& rewriter)
      : astContext(Context), reordering(reordering), rewriter(rewriter)
    {
        rewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());
    }

    // We want to investigate top-level things.
    bool VisitRecordDecl(clang::RecordDecl* D);
};

void structReordering(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfReorderings);

#endif
