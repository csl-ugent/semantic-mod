#include "StructReordering.h"
#include "SemanticUtil.h"

#include "clang/Tooling/Tooling.h"

#include "json.h"

#include <iterator>
#include <string>
#include <vector>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

bool StructReorderingAnalyser::VisitRecordDecl(clang::RecordDecl* D) {
    // We make sure the record is a struct, and we have its definition
    if (D->isStruct() && D->isThisDeclarationADefinition()) {
        // Count the number of fields in the struct
        unsigned size = std::distance(D->field_begin(), D->field_end());

        // We want at least 2 fields
        if (size >= 2) {
            const StructUnique candidate(D, astContext);
            const std::string& fileName = candidate.getFileName();

            // We make sure the file is contained in our base directory...
            if (fileName.find(reordering.baseDirectory) == std::string::npos)
                return true;

            // To be a valid candidate none of the fields can be macro.
            for(auto field : D->fields())
                if (field->getLocStart().isMacroID())
                    return true;

            StructData& data = reordering.candidates[candidate];
            if (data.valid && data.empty())
            {
                llvm::outs() << "Found valid candidate: " << candidate.getName() << "\n";
                data.addFields(D);
            }
        }
    }
    return true;
}

void StructReorderingAnalyser::detectStructsRecursively(const Type* origType) {
    // We obtain the canonical qual type/type.
    QualType qualTypeCn = origType->getCanonicalTypeInternal();
    if (const Type* type = qualTypeCn.getTypePtrOrNull()) {
        if (type->isStructureType()) { // Handle structs
            RecordDecl* D = type->getAsStructureType()->getDecl();
            const StructUnique candidate(D, astContext);
            const std::string& fileName = candidate.getFileName();

            // We make sure the file is contained in our base directory...
            if (fileName.find(reordering.baseDirectory) == std::string::npos)
                return;

            // Invalidate the struct.
            reordering.invalidateCandidate(candidate, "an instance of the struct is stored globally");

            // We further investigate the fields of the struct.
            for(auto field : D->fields())
            {
                const Type* subType = (field->getType()).getTypePtrOrNull();

                // We handle the field recursively.
                detectStructsRecursively(subType);
            }
        } else if (type->isUnionType()) { // Handle unions.
            RecordDecl* D = type->getAsUnionType()->getDecl();

            // We further investigate the fields of the union.
            for(auto field : D->fields())
            {
                const Type* subType = (field->getType()).getTypePtrOrNull();

                // We handle the field recursively.
                detectStructsRecursively(subType);
            }
        } else if (type->isArrayType()) { // Handle arrays.
            const Type* subType = (type->getAsArrayTypeUnsafe()->getElementType()).getTypePtrOrNull();
            detectStructsRecursively(subType);
        }
    }
}

bool StructReorderingAnalyser::VisitVarDecl(clang::VarDecl *D) {
    const Type* type = (D->getType()).getTypePtrOrNull();

    // We detect structs recursively and remove them from the struct map.
    // We don't support reordering structs that have global storage
    // TODO: does this work?
    if (D->hasGlobalStorage())
        detectStructsRecursively(type);

    // We check if the variable has an initializer.
    // Initializer was actually an InitListExpr.
    // TODO: is this right?
    if (dyn_cast_or_null<InitListExpr>(D->getInit()))
        detectStructsRecursively(type);

    return true;
}

bool StructReorderingRewriter::VisitRecordDecl(clang::RecordDecl* D) {
    // We make sure the record is a struct and a definition.
    if (D->isStruct() && D->isThisDeclarationADefinition()) {
        // Check if this is a declaration for the struct that is to be reordered
        const StructTransformation* transformation = reordering.transformation;
        const StructUnique target(D, astContext);
        if (transformation->target == target) {
            llvm::outs() << "Declaration of: " << target.getName() << " has to be rewritten!\n";

            auto ordering = transformation->ordering;
            std::vector<clang::FieldDecl*> fields(D->field_begin(), D->field_end());
            for (size_t iii = 0; iii < fields.size(); iii++) {
                const SourceRange& oldRange = fields[iii]->getSourceRange();
                const SourceRange& newRange = fields[ordering[iii]]->getSourceRange();
                const SourceRange oldRangeExpanded(astContext.getSourceManager().getExpansionRange(oldRange.getBegin()).first, astContext.getSourceManager().getExpansionRange(oldRange.getEnd()).second);
                const SourceRange newRangeExpanded(astContext.getSourceManager().getExpansionRange(newRange.getBegin()).first, astContext.getSourceManager().getExpansionRange(newRange.getEnd()).second);

                // We replace the field with another one based on the ordering.
                const std::string substitute = location2str(newRangeExpanded, astContext);
                rewriter.ReplaceText(oldRangeExpanded, substitute);
            }
        }
    }
    return true;
}

// Method used for the structreordering semantic transformation.
void structReordering(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfReorderings) {
    reorder<StructReordering, StructReorderingAnalyser, StructReorderingRewriter, StructUnique, StructTransformation>(Tool, baseDirectory, outputDirectory, numberOfReorderings);
}
