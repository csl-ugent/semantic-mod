#ifndef _SEMANTICDATA
#define _SEMANTICDATA

#include <map>
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

    // Information regarding the fields within the structure.
    std::vector<FieldData> fieldData;
public:

    explicit StructData(std::string name): name(name) {};

    // Obtain the name of this structure.
    std::string getName() { return this->name; }

    // Obtain the field data.
    std::vector<FieldData> getFieldData() { return this->fieldData; }

    // Obtain the amount of field data fields.
    int getFieldDataSize() { return this->fieldData.size(); }

    // Adding field data.
    void addFieldData(int position, std::string fieldName, std::string fieldType, clang::SourceRange sourceRange);
};

// Structure reordering semantic modification.
class StructReordering {
private:

    // Map containing all information regarding different structures.
    std::map<std::string, StructData*> structMap;

    // Map containing all information regarding reorderings.
    std::map<std::string, StructData*> structReorderings;

public:

    explicit StructReordering() {}
    ~StructReordering();

    // Obtain the structure information map.
    std::map<std::string, StructData*> getStructMap() { return structMap; }

    // Obtain the structure reordering information map.
    std::map<std::string, StructData*> getStructReorderings() { return structReorderings; }

    // Check if a struct is already defined as a key in the struct map.
    bool isInStructMap(std::string name);

    // Method used to add structure data to the struct data map.
    // This object will take ownership of the allocated memory and release
    // it when necessary.
    void addStructData(std::string name, StructData* data);

    // Method used to add structure reordering data to the structure reordering
    // information map.
    void addStructReorderingData(std::string name, StructData* data);
};


// Class containing all relevant semantic information.
class SemanticData {
private:

    // Structreordering information.
    StructReordering* structReordering;

public:

    explicit SemanticData() {

        // Allocate the necessary object containing relevant information.
        structReordering = new StructReordering();
    }
    ~SemanticData() {

        // Delete the allocated objects.
        delete structReordering;
    }

    // Obtain structreordering information.
    StructReordering* getStructReordering() { return structReordering; }
};

#endif
