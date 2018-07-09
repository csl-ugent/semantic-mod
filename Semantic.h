#ifndef _SEMANTIC
#define _SEMANTIC

#include "SemanticData.h"
#include "SemanticFrontendAction.h"
#include "SemanticUtil.h"

#include "clang/Tooling/Tooling.h"

#include "json.h"

#include <algorithm>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Method used to generate new versions
template <typename RewriterType>
void generateVersions(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfVersions) {
    typedef typename RewriterType::Analyser AnalyserType;
    typedef typename AnalyserType::Target TargetType;

    const MetaData metadata(baseDirectory, outputDirectory);

    // We run the analysis phase and get the valid candidates
    Candidates<TargetType> analysis_candidates;
    Tool->run(new AnalysisFrontendActionFactory<AnalyserType>(metadata, analysis_candidates));
    auto candidates = analysis_candidates.select_valid();

    // We need to determine the maximum number of versions that is actually
    // possible with the given input source files (based on the analysis phase).
    std::map<unsigned, unsigned> histogram;
    unsigned long totalItems = 0;
    unsigned long totalVersions = 0;
    for (const auto& candidate : candidates) {
        unsigned nrOfItems = candidate.second.nrOfItems();

        // Keep count of the total possible versions and the total number of items
        totalVersions += factorial(nrOfItems) -1;// All permutations are possible versions, except for the original one.
        totalItems += nrOfItems;

        // Look if this amount has already occured or not.
        if (histogram.find(nrOfItems) != histogram.end()) {
            histogram[nrOfItems]++;
        } else {
            histogram[nrOfItems] = 1;
        }
    }

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
    std::vector<Transformation> transformations;
    llvm::outs() << "Total number of versions possible with " << candidates.size() << " candidates is: " << totalVersions << "\n";
    llvm::outs() << "The actual number of versions is set to: " << actualNumberOfVersions << "\n";
    for (unsigned long versionId = 0; versionId < actualNumberOfVersions; versionId++)
    {
        // We choose a candidate at random.
        const auto& chosen = candidates[random_0_to_n(candidates.size())];

        // Create original ordering
        std::vector<unsigned> ordering(chosen.second.nrOfItems());
        std::iota(ordering.begin(), ordering.end(), 0);

        // Things we should write to an output file.
        Json::Value output;
        output["target_name"] = chosen.first.getName();
        output["file_name"] = chosen.first.getFileName();
        output["original"]["items"] = chosen.second.getJSON(ordering);// We output the original order.

        // Make sure the modified ordering isn't the same as the original
        std::vector<unsigned> original_ordering = ordering;
        do {
            std::random_shuffle(ordering.begin(), ordering.end());
        } while (original_ordering == ordering);

        // Check if this transformation isn't duplicate. If it is, we try again
        bool found = false;
        for (const auto& t : transformations) {
            if (t.target == chosen.first && t.ordering == ordering) {
                found = true;
                break;
            }
        }
        if (found)
            continue;

        // Debug information.
        llvm::outs() << "Chosen target: " << chosen.first.getName() << "\n";
        llvm::outs() << "Chosen ordering: " << "\n";
        for (auto it : ordering) {
            llvm::outs() << it << " ";
        }
        llvm::outs() << "\n";

        output["modified"]["items"] = chosen.second.getJSON(ordering);// We output the modified order.

        // We write some information regarding the performed transformations to output.
        writeJSONToFile(metadata.outputPrefix, versionId + 1, "transformations.json", output);

        // Add the transformation
        transformations.emplace_back(chosen.first, ordering);
    }

    // We perform the rewrite operations.
    for (unsigned long iii = 0; iii < actualNumberOfVersions; iii++)
    {
        Tool->run(new RewritingFrontendActionFactory<RewriterType>(metadata, transformations[iii], iii + 1));
    }
}

#endif
