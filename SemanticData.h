#ifndef _SEMANTIC_DATA
#define _SEMANTIC_DATA

#include "llvm/ADT/MapVector.h"
#include "llvm/Support/raw_ostream.h"

#include "json.h"

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
struct Transformation {
    const TargetUnique& target;
    const std::vector<unsigned> ordering;
    Transformation(const TargetUnique& target, const std::vector<unsigned>& ordering)
        : target(target), ordering(ordering) {}
};

// This class contains the data used during the generating of new versions
template <typename TargetType>
class Version {
    public:
        std::string baseDirectory;
        std::string outputPrefix;
        Transformation* transformation;// The transformation to apply
        llvm::MapVector<TargetType, typename TargetType::Data, std::map<TargetType, unsigned>> candidates;// Map containing all information regarding candidates.
        Version(const std::string& bd, const std::string& od) : baseDirectory(bd), outputPrefix(od + "version_") {}

        void invalidateCandidate(const TargetType& candidate, const std::string& reason) {
            // If the candidate is already invalid, just return
            TargetUnique::Data& data = candidates[candidate];
            if (!data.valid)
                return;

            llvm::outs() << "Invalidate candidate: " << candidate.getName() << ". Reason: " << reason << ".\n";
            data.valid = false;
        }
};

#endif
