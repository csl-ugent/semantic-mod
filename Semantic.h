#ifndef _SEMANTIC
#define _SEMANTIC

#include "SemanticFrontendAction.h"
#include "SemanticUtil.h"

#include "clang/Tooling/Tooling.h"

#include "json.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

class Reordering {
    protected:
        virtual ~Reordering() {}
    public:
        std::string baseDirectory;
        std::string outputPrefix;
        Reordering(const std::string& bd, const std::string& od) : baseDirectory(bd), outputPrefix(od + "version_") {}
};

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

// Method used for the function parameter semantic transformation.
template <typename ReorderingType, typename AnalyserType, typename RewriterType, typename TargetUnique, typename Transformation>
void reorder(clang::tooling::ClangTool* Tool, const std::string& baseDirectory, const std::string& outputDirectory, const unsigned long numberOfReorderings) {
    // We run the analysis phase and get the valid candidates
    ReorderingType reordering(baseDirectory, outputDirectory);
    Tool->run(new AnalysisFrontendActionFactory<ReorderingType, AnalyserType>(reordering));
    std::vector<TargetUnique> candidates;
    for (const auto& it : reordering.candidates) {
        if (it.second.valid)
        {
            llvm::outs() << "Valid candidate: " << it.first.getName() << "\n";
            candidates.push_back(it.first);
        }
    }

    // We need to determine the maximum amount of reorderings that is actually
    // possible with the given input source files (based on the analysis phase).
    std::map<unsigned, unsigned> histogram;
    unsigned long totalReorderings = 0;
    double avgItems = 0;
    for (const auto& candidate : candidates) {
        unsigned nrOfItems = reordering.candidates[candidate].nrOfItems();

        // Keep count of the total possible reorderings and the average number of items
        totalReorderings += factorial(nrOfItems);
        avgItems += nrOfItems;

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
    analytics["avg_items"] = avgItems / candidates.size();
    analytics["entropy"] = entropyEquiprobable(totalReorderings);

    // Add histogram.
    for (const auto& it : histogram) {
        std::stringstream ss;
        ss << it.first;
        analytics["histogram"][ss.str()] = it.second;
    }

    // Output analytics
    llvm::outs() << "Total number of reorderings possible with " << candidates.size() << " candidates is: " << totalReorderings << "\n";
    llvm::outs() << "Writing analytics output...\n";
    writeJSONToFile(outputDirectory, -1, "analytics.json", analytics);

    unsigned long amountChosen = 0;
    unsigned long amount = std::min(numberOfReorderings, totalReorderings);
    std::vector<Transformation> transformations;
    llvm::outs() << "Amount of reorderings is set to: " << amount << "\n";
    while (amountChosen < amount)
    {
        // We choose a candidate at random.
        const auto& chosen = candidates[random_0_to_n(candidates.size())];

        // Create original ordering
        std::vector<unsigned> ordering(reordering.candidates[chosen].nrOfItems());
        std::iota(ordering.begin(), ordering.end(), 0);

        // Things we should write to an output file.
        Json::Value output;
        output["target_name"] = chosen.getName();
        output["file_name"] = chosen.getFileName();
        output["original"]["items"] = reordering.candidates[chosen].getJSON(ordering);// We output the original order.

        // Make sure the modified ordering isn't the same as the original
        std::vector<unsigned> original_ordering = ordering;
        do {
            std::random_shuffle(ordering.begin(), ordering.end());
        } while (original_ordering == ordering);

        // Check if this transformation isn't duplicate. If it is, we try again
        bool found = false;
        for (const auto& t : transformations) {
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

        output["modified"]["items"] = reordering.candidates[chosen].getJSON(ordering);// We output the modified order.

        // We write some information regarding the performed transformations to output.
        writeJSONToFile(reordering.outputPrefix, amountChosen+1, "transformations.json", output);

        // Add the transformation
        transformations.emplace_back(chosen, ordering);
        amountChosen++;
    }

    // We perform the rewrite operations.
    for (unsigned long iii = 0; iii < amount; iii++)
    {
        // We select the current transformation
        reordering.transformation = &transformations[iii];

        Tool->run(new RewritingFrontendActionFactory<ReorderingType, RewriterType>(reordering, iii + 1));
    }
}

#endif
