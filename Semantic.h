#ifndef _SEMANTIC
#define _SEMANTIC

#include "SemanticFrontendAction.h"
#include "SemanticUtil.h"

#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/MapVector.h"

#include "json.h"

#include <algorithm>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>

// This class uniquely describes a possible target to transform
class TargetUnique {
    protected:
        virtual ~TargetUnique() {}
        std::string name;
        std::string fileName;
        bool global;

    public:
        TargetUnique(const std::string& name, const std::string& fileName, bool global = false)
            : name(name), fileName(fileName), global(global) {}
        std::string getName() const { return name;}
        std::string getFileName() const { return fileName;}
        bool operator== (const TargetUnique& other) const
        {
            // If the names differ, it can't be the same
            if (name != other.name)
                return false;

            // If one of the two is global and the other isn't, it can't be the same
            if (global ^ other.global)
                return false;

            // If both are local, yet from a different file, they are different
            if ((!global && !other.global) && (fileName != other.fileName))
                return false;

            return true;
        }
        bool operator< (const TargetUnique& other) const
        {
            // Create string representations so we can correctly compare
            std::string left = global ? name : name + ":" + fileName;
            std::string right = other.global ? other.name : other.name + ":" + other.fileName;

            return left < right;
        }
};

// This class describes the data associated to a target
class TargetData {
    protected:
        virtual ~TargetData() {}
    public:
        bool valid;

        TargetData(bool valid = true) : valid(valid) {}
        virtual bool empty() const = 0;
        virtual Json::Value getJSON(const std::vector<unsigned>& ordering) const = 0;
        virtual unsigned nrOfItems() const = 0;
};

// This struct describes a transformation
struct Transformation {
    const TargetUnique& target;
    const std::vector<unsigned> ordering;
    Transformation(const TargetUnique& target, const std::vector<unsigned>& ordering)
        : target(target), ordering(ordering) {}
};

// This class contains the data used during the generating of new versions
template <typename TargetUnique, typename TargetData>
class Version {
    protected:
        virtual ~Version() {}
    public:
        std::string baseDirectory;
        std::string outputPrefix;
        Transformation* transformation;// The transformation to apply
        llvm::MapVector<TargetUnique, TargetData, std::map<TargetUnique, unsigned>> candidates;// Map containing all information regarding candidates.
        Version(const std::string& bd, const std::string& od) : baseDirectory(bd), outputPrefix(od + "version_") {}

        void invalidateCandidate(const TargetUnique& candidate, const std::string& reason) {
            // If the candidate is already invalid, just return
            TargetData& data = candidates[candidate];
            if (!data.valid)
                return;

            llvm::outs() << "Invalidate candidate: " << candidate.getName() << ". Reason: " << reason << ".\n";
            data.valid = false;
        }
};

// Method used to generate new versions
template <typename VersionType, typename AnalyserType, typename RewriterType>
void generateVersions(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfVersions) {
    // We run the analysis phase and get the valid candidates
    VersionType version(baseDirectory, outputDirectory);
    Tool->run(new AnalysisFrontendActionFactory<VersionType, AnalyserType>(version));
    std::vector<std::pair<const TargetUnique&, const TargetData&>> candidates;
    for (const auto& it : version.candidates) {
        if (it.second.valid)
        {
            llvm::outs() << "Valid candidate: " << it.first.getName() << "\n";
            candidates.emplace_back(it);
        }
    }

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
        writeJSONToFile(version.outputPrefix, versionId + 1, "transformations.json", output);

        // Add the transformation
        transformations.emplace_back(chosen.first, ordering);
    }

    // We perform the rewrite operations.
    for (unsigned long iii = 0; iii < actualNumberOfVersions; iii++)
    {
        // We select the current transformation
        version.transformation = &transformations[iii];

        Tool->run(new RewritingFrontendActionFactory<VersionType, RewriterType>(version, iii + 1));
    }
}

#endif
