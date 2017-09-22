#ifndef _SEMANTICDATA
#define _SEMANTICDATA

#include <map>
#include <set>
#include <string>
#include "clang/Basic/SourceManager.h"

// Data of fields appearing within a structure.
typedef struct fieldData_ {

    // Position.
    int position;

    // The name of the field.
    std::string fieldName;

    // The type of the field.
    std::string fieldType;

    // The range (location begin-end of where this field appears in
    // the source code).
    clang::SourceRange sourceRange;

} FieldData;


// Structure AST data.
class StructData {
private:
    // The name of this structure.
    std::string name;

    // The fileName in which this struct appears.
    std::string fileName;

    // Information regarding the fields within the structure.
    std::vector<FieldData> fieldData;

    // Information regarding possible definitions of fields.
    std::set<std::string> fieldDefinitions;

public:

    explicit StructData(std::string name, std::string fileName): name(name), fileName(fileName) {};

    // Get the field definitions.
    std::set<std::string> getFieldDefinitions() { return this->fieldDefinitions; }

    // Method used to set the fielddefinitions.
    void setFieldDefinitions(std::set<std::string> fieldDefinitions) {
        this->fieldDefinitions = fieldDefinitions;
    }

    // Obtain the name of this structure.
    std::string getName() { return this->name; }

    // Obtain the fileName in which this structure is defined.
    std::string getFileName() {return this->fileName; }

    // Obtain the field data.
    std::vector<FieldData> getFieldData() { return this->fieldData; }

    // Obtain the amount of field data fields.
    int getFieldDataSize() { return this->fieldData.size(); }

    // Adding field data.
    void addFieldData(int position, std::string fieldName, std::string fieldType, clang::SourceRange sourceRange);

    // Method used to add a field definition.
    void addFieldDefinition(std::string definition);

    // Method used to check if this struct has field definitions.
    bool hasFieldDefinitions();
};

// Function AST data.
class FunctionData {
private:
    // The name of this function.
    std::string name;

    // The fileName in which this function appears.
    std::string fileName;

    // Information regarding the fields within the structure.
    std::vector<FieldData> fieldData;

public:

    explicit FunctionData(std::string name, std::string fileName): name(name), fileName(fileName) {};

    // Obtain the name of this function.
    std::string getName() { return this->name; }

    // Obtain the fileName in which this function is defined.
    std::string getFileName() {return this->fileName; }

    // Obtain the field data.
    std::vector<FieldData> getFieldData() { return this->fieldData; }

    // Obtain the amount of field data fields.
    int getFieldDataSize() { return this->fieldData.size(); }

    // Adding field data.
    void addFieldData(int position, std::string fieldName, std::string fieldType, clang::SourceRange sourceRange);
};

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

// Structure reordering semantic modification.
class StructReordering : public Reordering {
private:

    // Map containing all information regarding different structures.
    std::map<std::string, StructData*> structMap;

    // Map containing all information regarding reorderings.
    std::map<std::string, StructData*> structReorderings;

    // Set containing all structs that have been rewritten already.
    std::set<std::string> rewritten;

public:

    explicit StructReordering() {}
    ~StructReordering();

    // Obtain the structure information map.
    std::map<std::string, StructData*> getStructMap() { return structMap; }

    // Obtain the structure reordering information map.
    std::map<std::string, StructData*> getStructReorderings() { return structReorderings; }

    // Check if a struct is already defined as a key in the struct map.
    bool isInStructMap(std::string name);

    // Check if a struct is already defined as a key in the struct reordering map.
    bool isInStructReorderingMap(std::string name);

    // Check if a struct has been rewritten already.
    bool hasBeenRewritten(std::string name);

    // Method used to add structure data to the struct data map.
    // This object will take ownership of the allocated memory and release
    // it when necessary.
    void addStructData(std::string name, StructData* data);

    // Method used to remove a struct from the struct data.
    void removeStructData(std::string name);

    // Method used to add structure reordering data to the structure reordering
    // information map.
    void addStructReorderingData(std::string name, StructData* data);

    // Method used to add a structure to the set of rewritten structs.
    void structRewritten(std::string name);

    // Method used to clear the structure reordering information map.
    void clearStructReorderings();

    // Clear the set of structures that have been rewritten already.
    void clearRewritten();
};

// Function parameter reordering semantic modification.
class FPReordering : public Reordering {
private:

    // Map containing all information regarding different functions.
    std::map<std::string, FunctionData*> functionMap;

    // Map containing all information regarding reorderings.
    std::map<std::string, FunctionData*> functionReorderings;

    // Set containing all functions that have been rewritten already.
    std::set<std::string> rewritten;

public:
    explicit FPReordering() {}
    ~FPReordering();

    // Obtain the function information map.
    std::map<std::string, FunctionData*> getFunctionMap() { return functionMap; }

    // Obtain the function reordering information map.
    std::map<std::string, FunctionData*> getFunctionReorderings() { return functionReorderings; }

    // Check if a function is already defined as a key in the function map.
    bool isInFunctionMap(std::string name);

    // Check if a struct is already defined as a key in the function reordering map.
    bool isInFunctionReorderingMap(std::string name);

    // Method used to add function reordering data to the function reordering
    // information map.
    void addFunctionReorderingData(std::string name, FunctionData* data);

    // Method used to add function data to the function data map.
    // This object will take ownership of the Function parameter and data.
    void addFunctionData(std::string name, FunctionData* data);

    // Method used to add a function to the set of rewritten functions.
    void functionRewritten(std::string name);

    // Check if a function has been rewritten already.
    bool hasBeenRewritten(std::string name);

    // Method used to clear the function reordering information map.
    void clearFunctionReorderings();

    // Clear the set of functions that have been rewritten already.
    void clearRewritten();
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
