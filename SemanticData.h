#ifndef _SEMANTIC_DATA
#define _SEMANTIC_DATA

#include "SemanticUtil.h"

#include "llvm/ADT/MapVector.h"
#include "llvm/Support/raw_ostream.h"

#include "json.h"

#include <numeric>
#include <string>
#include <vector>

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

        // This class describes the data associated to a target
        class Data {
            protected:
                virtual ~Data() {}
            public:
                bool valid;

                Data(bool valid = true) : valid(valid) {}
                virtual bool empty() const = 0;
                virtual Json::Value getJSON(const std::vector<unsigned>& ordering) const = 0;
                virtual unsigned nrOfItems() const = 0;
        };
};

// This struct describes a transformation
class Transformation {
    protected:
        virtual ~Transformation() {}
        virtual void outputTransformationSpecificDebugInfo() const = 0;
    public:
        const TargetUnique& target;

        Transformation(const TargetUnique& target)
            : target(target) {}
        static void calculateStatistics(const std::vector<std::pair<const TargetUnique&, const TargetUnique::Data&>>& candidates, std::map<unsigned, unsigned>& histogram, unsigned long& totalItems, unsigned long& totalVersions) {}
        virtual Json::Value getJSON(const TargetUnique::Data& data) const = 0;

        bool operator== (const Transformation& other) const
        {
            return (target == other.target);
        }

        void outputDebugInfo() const
        {
            llvm::outs() << "Chosen target: " << target.getName() << "\n";
            outputTransformationSpecificDebugInfo();
            llvm::outs() << "\n";
        }
};

class InsertionTransformation : public Transformation {
    protected:
        virtual void outputTransformationSpecificDebugInfo() const
        {
            llvm::outs() << "Chosen insertion point: " << insertionPoint << "\n";
        }

    public:
        const unsigned insertionPoint;

        InsertionTransformation(const TargetUnique& target, const TargetUnique::Data& data)
            : Transformation(target), insertionPoint(random_0_to_n(data.nrOfItems() +1)) {}

        bool operator== (const InsertionTransformation& other) const
        {
            return (static_cast<const Transformation&>(*this) == static_cast<const Transformation&>(other)) && (insertionPoint == other.insertionPoint);
        }

        static void calculateStatistics(const std::vector<std::pair<const TargetUnique&, const TargetUnique::Data&>>& candidates, std::map<unsigned, unsigned>& histogram, unsigned long& totalItems, unsigned long& totalVersions)
        {
            for (const auto& candidate : candidates) {
                unsigned nrOfItems = candidate.second.nrOfItems();

                // Keep count of the total possible reorderings and the average number of items
                totalVersions += nrOfItems +1;
                totalItems += nrOfItems;

                // Look if this amount has already occured or not.
                if (histogram.find(nrOfItems) != histogram.end()) {
                    histogram[nrOfItems]++;
                } else {
                    histogram[nrOfItems] = 1;
                }
            }
        }

        virtual Json::Value getJSON(const TargetUnique::Data& data) const
        {
            Json::Value output;
            output["target_name"] = target.getName();
            output["file_name"] = target.getFileName();
            output["insertion_point"] = insertionPoint;

            return output;
        }
};

class ReorderingTransformation : public Transformation {
    protected:
        virtual void outputTransformationSpecificDebugInfo() const
        {
            llvm::outs() << "Chosen ordering: " << "\n";
            for (auto it : ordering) {
                llvm::outs() << it << " ";
            }
        }

        const std::vector<unsigned> generateOrdering(unsigned nrOfElements)
        {
            // Create original ordering
            std::vector<unsigned> ordering(nrOfElements);
            std::iota(ordering.begin(), ordering.end(), 0);

            // Make sure the modified ordering isn't the same as the original
            std::vector<unsigned> original_ordering = ordering;
            do {
                std::random_shuffle(ordering.begin(), ordering.end());
            } while (original_ordering == ordering);

            return ordering;
        }
    public:
        const std::vector<unsigned> ordering;

        ReorderingTransformation(const TargetUnique& target, const TargetUnique::Data& data)
            : Transformation(target), ordering(generateOrdering(data.nrOfItems())) {}

        bool operator== (const ReorderingTransformation& other) const
        {
            return (static_cast<const Transformation&>(*this) == static_cast<const Transformation&>(other)) && (ordering == other.ordering);
        }

        static void calculateStatistics(const std::vector<std::pair<const TargetUnique&, const TargetUnique::Data&>>& candidates, std::map<unsigned, unsigned>& histogram, unsigned long& totalItems, unsigned long& totalVersions)
        {
            for (const auto& candidate : candidates) {
                unsigned nrOfItems = candidate.second.nrOfItems();

                // Keep count of the total possible reorderings and the average number of items
                totalVersions += factorial(nrOfItems) -1;// All permutations are possible reorderings, except for the original one.
                totalItems += nrOfItems;

                // Look if this amount has already occured or not.
                if (histogram.find(nrOfItems) != histogram.end()) {
                    histogram[nrOfItems]++;
                } else {
                    histogram[nrOfItems] = 1;
                }
            }
        }

        virtual Json::Value getJSON(const TargetUnique::Data& data) const
        {
            // Create original ordering
            std::vector<unsigned> orig_ordering(data.nrOfItems());
            std::iota(orig_ordering.begin(), orig_ordering.end(), 0);

            Json::Value output;
            output["target_name"] = target.getName();
            output["file_name"] = target.getFileName();
            output["original"]["items"] = data.getJSON(orig_ordering);// We output the original order.
            output["modified"]["items"] = data.getJSON(ordering);// We output the modified order.

            return output;
        }
};

// This class contains the data used during the generating of new versions
template <typename TargetType>
class Candidates {
    private:
        llvm::MapVector<TargetType, typename TargetType::Data, std::map<TargetType, unsigned>> candidates;// Map containing all information regarding candidates.

    public:
        typename TargetType::Data& get(const TargetType& candidate) {
            return candidates[candidate];
        }

        void invalidate(const TargetType& candidate, const std::string& reason) {
            // If the candidate is already invalid, just return
            TargetUnique::Data& data = candidates[candidate];
            if (!data.valid)
                return;

            llvm::outs() << "Invalidate candidate: " << candidate.getName() << ". Reason: " << reason << ".\n";
            data.valid = false;
        }

        std::vector<std::pair<const TargetUnique&, const TargetUnique::Data&>> select_valid() const {
            std::vector<std::pair<const TargetUnique&, const TargetUnique::Data&>> ret;
            for (const auto& it : candidates) {
                if (it.second.valid)
                {
                    llvm::outs() << "Valid candidate: " << it.first.getName() << "\n";
                    ret.emplace_back(it);
                }
            }
            return ret;
        }
};

// This class contains some metadata used by FrontendActions
class MetaData {
    public:
        const std::string baseDirectory;
        const std::string outputPrefix;
        MetaData(const std::string& bd, const std::string& od) : baseDirectory(bd), outputPrefix(od + "version_") {}
};

#endif
