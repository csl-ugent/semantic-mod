#ifndef _SEMANTICDATA
#define _SEMANTICDATA

#include "clang/Basic/SourceManager.h"

#include <map>
#include <set>
#include <string>

// Switch AST data.
class SwitchData {
private:
    // The location of this switch.
    unsigned location;

    // The fileName in which this switch appears.
    std::string fileName;

    // Value that will be used in the case of reordering.
    int randomXORValue;

public:
    explicit SwitchData(unsigned location, std::string fileName): location(location), fileName(fileName) {};

    // Obtain the location of this function.
    unsigned getLocation() { return this->location; }

    // Obtain the fileName in which this function is defined.
    std::string getFileName() {return this->fileName; }

    // Method used to set the random XOR value.
    void setRandomXORValue(int randomXORValue) {
        this->randomXORValue = randomXORValue;
    }

    // Method used to get the random XOR value.
    int getRandomXORValue() {
        return this->randomXORValue;
    }
};

class Reordering {
  protected:
    virtual ~Reordering() {}
};

// Switch case reordering semantic modification.
class SWCReordering : public Reordering {
private:
    // Map containing all information regarding different switches.
    std::map<unsigned, SwitchData*> switchMap;

    // Map containing all information regarding reorderings.
    std::map<unsigned, SwitchData*> switchReorderings;

    // Set containing all switches that have been rewritten already.
    std::set<unsigned> rewritten;

public:
    explicit SWCReordering() {}
    ~SWCReordering();

    // Obtain the switch information map.
    std::map<unsigned, SwitchData*> getSwitchMap() { return switchMap; }

    // Obtain the switch reordering information map.
    std::map<unsigned, SwitchData*> getSwitchReorderings() { return switchReorderings; }

    // Check if a switch is already defined as a key in the function map.
    bool isInSwitchMap(unsigned);

    // Check if a struct is already defined as a key in the switch reordering map.
    bool isInSwitchReorderingMap(unsigned);

    // Method used to add switch data to the switch data map.
    // This object will take ownership of the switch parameter and data.
    void addSwitchData(unsigned, SwitchData* data);

    // Method used to add switch reordering data to the switch reordering
    // information map.
    void addSwitchReorderingData(unsigned, SwitchData* data);

    // Method used to add a switch to the set of rewritten switches.
    void switchRewritten(unsigned);

    // Check if a switch has been rewritten already.
    bool hasBeenRewritten(unsigned);

    // Method used to clear the switch reordering information map.
    void clearSwitchReorderings();

    // Clear the set of switches that have been rewritten already.
    void clearRewritten();
};

#endif
