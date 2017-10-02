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
            if (fileName.find(this->baseDirectory) == std::string::npos)
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
            if (fileName.find(this->baseDirectory) == std::string::npos)
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
                this->rewriter->ReplaceText(oldRangeExpanded, substitute);
            }
        }
    }
    return true;
}

// Method used for the structreordering semantic transformation.
void structReordering(Rewriter* rewriter, ClangTool* Tool, std::string baseDirectory, std::string outputDirectory, int amountOfReorderings) {
    // Debug information.
    llvm::outs() << "Phase 1: Struct analysis\n";

    // We run the analysis phase.
    StructReordering reordering;
    Tool->run(new SemanticFrontendActionFactory(reordering, rewriter, baseDirectory, Transformation::StructReordering, Phase::Analysis));
    std::vector<StructUnique> candidates;
    for (const auto& it : reordering.candidates) {
        if (it.second.valid)
        {
            llvm::outs() << "Valid candidate: " << it.first.getName() << "\n";
            candidates.push_back(it.first);
        }
    }

    // We need to determine the maximum amount of reorderings that is actually
    // possible with the given input source files (based on the analysis phase).
    unsigned long totalReorderings = 0;
    double averageStructSize = 0;
    Json::Value vec(Json::arrayValue);
    for (const auto& candidate : candidates) {
        unsigned nrFields = reordering.candidates[candidate].fields.size();

        Json::Value v;
        v["size"] = nrFields;
        totalReorderings += factorial(nrFields);
        averageStructSize += nrFields;
        vec.append(v);
    }

    // Create analytics.
    Json::Value analytics;
    analytics["amount_of_structs"] = candidates.size();
    analytics["avg_struct_size"] = averageStructSize / candidates.size();
    analytics["entropy"] = entropyEquiprobable(totalReorderings);
    analytics["struct_fields"] = vec;
    llvm::outs() << "Total reorderings possible with " << candidates.size() << " structs.\n";
    llvm::outs() << "Writing analytics output...\n";
    writeJSONToFile(outputDirectory, -1, "analytics.json", analytics);

    // Get the output prefix
    std::string outputPrefix = outputDirectory + "struct_r_";

    // We choose the amount of configurations of this struct that is required.
    unsigned long amount = amountOfReorderings;
    unsigned long amountChosen = 0;
    std::vector<StructTransformation> transformations;
    llvm::outs() << "Amount of reorderings is set to: " << amount << "\n";
    while (amountChosen < amount)
    {
        // We choose a candidate at random.
        StructUnique& chosen = candidates[random_0_to_n(candidates.size())];
        std::vector<StructField>& fields = reordering.candidates[chosen].fields;

        // Things we should write to an output file.
        Json::Value output;
        output["type"] = "struct_reordering";
        output["file_struct_name"] = chosen.getName();
        output["file_name"] = chosen.getFileName();

        // We output the original order.
        Json::Value original(Json::arrayValue);
        std::vector<unsigned> ordering;
        for (unsigned iii = 0; iii < fields.size(); iii++) {
            const StructField& field = fields[iii];
            Json::Value v;
            v["position"] = iii;
            v["name"] = field.name;
            v["type"] = field.type;
            vec.append(v);
            ordering.push_back(iii);
        }
        output["original"]["fields"] = original;

        // Make sure the modified ordering isn't the same as the original
        std::vector<unsigned> original_ordering = ordering;
        do {
            std::random_shuffle(ordering.begin(), ordering.end());
        } while (original_ordering == ordering);

        // Check if this transformation isn't duplicate. If it is, we try again
        bool found = false;
        for (auto& t : transformations) {
            if (t.target == chosen && t.ordering == ordering) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        // Debug information.
        llvm::outs() << "Chosen target: " << chosen.getName() << "\n";
        llvm::outs() << "Chosen ordering: " << "\n";
        for (auto it : ordering) {
            llvm::outs() << it << " ";
        }
        llvm::outs() << "\n";

        // Create the output for the reordering as decided.
        Json::Value modified(Json::arrayValue);
        for (size_t iii = 0; iii < ordering.size(); iii++) {
            unsigned old_pos = ordering[iii];
            const StructField& field = fields[old_pos];

            Json::Value v;
            v["position"] = iii;
            v["name"] = field.name;
            v["type"] = field.type;
            modified.append(v);
            llvm::outs() << "Position:" << old_pos << " type: " << field.type << "\n";
        }
        output["modified"]["fields"] = modified;

        // We write some information regarding the performed transformations to output.
        writeJSONToFile(outputPrefix, amountChosen+1, "transformations.json", output);

        // Add the transformation
        transformations.emplace_back(chosen, ordering);
        amountChosen++;
    }

    // We perform the rewrite operations.
    for (unsigned long iii = 0; iii < amount; iii++)
    {
        // We select the current transformation
        StructTransformation* transformation = &transformations[iii];
        reordering.transformation = transformation;

        llvm::outs() << "Phase 2: performing rewrite for version: " << iii + 1 << " target name: " << transformation->target.getName() << "\n";
        Tool->run(new SemanticFrontendActionFactory(reordering, rewriter, baseDirectory, Transformation::StructReordering, Phase::Rewrite, iii + 1, outputPrefix));
    }
}
