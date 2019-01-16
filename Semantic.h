#ifndef _SEMANTIC
#define _SEMANTIC

#include "SemanticData.h"
#include "SemanticFrontendAction.h"
#include "SemanticUtil.h"

#include "clang/Tooling/Tooling.h"

#include "json.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Method used to generate new versions
template <typename RewriterType>
void generateVersions(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfVersions) {
    typedef typename RewriterType::Target TargetType;
    typedef typename RewriterType::TransformationType TransformationType;

    const MetaData metadata(baseDirectory, outputDirectory);

    // We run the analysis phase and get the valid candidates
    Candidates<TargetType> analysis_candidates;
    Tool->run(new AnalysisFrontendActionFactory<TargetType>(metadata, analysis_candidates));
    auto candidates = analysis_candidates.select_valid();

    // Calculate some statistics based on the candidates
    std::map<unsigned, unsigned> histogram;
    unsigned long totalItems = 0;
    unsigned long totalVersions = 0;
    TransformationType::calculateStatistics(candidates, histogram, totalItems, totalVersions);

    // Create analytics
    Json::Value analytics;
    analytics["number_of_candidates"] = candidates.size();
    analytics["avg_items"] = totalItems / candidates.size();
    analytics["entropy"] = entropyEquiprobable(totalVersions);

    // Add histogram.
    for (const auto& it : histogram) {
        std::stringstream ss;
        ss << it.first;
        analytics["histogram"][ss.str()] = it.second;
    }

    // Output analytics
    llvm::outs() << "Writing analytics output...\n";
    writeJSONToFile(outputDirectory, -1, "analytics.json", analytics);

    unsigned long actualNumberOfVersions = std::min(numberOfVersions, totalVersions);
    std::vector<TransformationType> transformations;
    llvm::outs() << "Total number of versions possible with " << candidates.size() << " candidates is: " << totalVersions << "\n";
    llvm::outs() << "The actual number of versions is set to: " << actualNumberOfVersions << "\n";
    for (unsigned long versionId = 1; versionId <= actualNumberOfVersions; versionId++)
    {
        // We choose a candidate at random.
        const auto& chosen = candidates[random_0_to_n(candidates.size() -1)];

        // Generate a transformation for this candidate
        TransformationType transformation(chosen.first, chosen.second);

        // Check if this transformation isn't duplicate. If it is, we try again
        bool found = false;
        for (const auto& t : transformations) {
            if (t == transformation) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        // We write some information regarding the performed transformations to output.
        transformation.outputDebugInfo();
        const Json::Value output = transformation.getJSON(chosen.second);
        writeJSONToFile(metadata.outputPrefix, versionId, "transformations.json", output);

        // Do the actual transformation and remember it
        Tool->run(new RewritingFrontendActionFactory<RewriterType>(metadata, transformation, versionId));
        transformations.push_back(transformation);
    }
}

#endif
