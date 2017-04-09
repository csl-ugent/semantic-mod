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
public:

    explicit StructData(std::string name, std::string fileName): name(name), fileName(fileName) {};

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
};

// Structure reordering semantic modification.
class StructReordering {
private:

    // Map containing all information regarding different structures.
    std::map<std::string, StructData*> structMap;

    // Map containing all information regarding reorderings.
    std::map<std::string, StructData*> structReorderings;

    // Set containing all structs that have been rewritten already.
    std::set<std::string> rewritten;

    // Set containing all struct names that should be rewritten.
    std::set<std::string> structNeedRewritten;

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

    // Check if a struct needs to be rewritten.
    bool needsToBeRewritten(std::string name);

    // Method used to add structure data to the struct data map.
    // This object will take ownership of the allocated memory and release
    // it when necessary.
    void addStructData(std::string name, StructData* data);

    // Method used to add structure reordering data to the structure reordering
    // information map.
    void addStructReorderingData(std::string name, StructData* data);
    
    // Method used to add a structure to the set of rewritten structs.
    void structRewritten(std::string name);

    // Method used to add a structure that needs to be rewritten.
    void addStructNeedRewritten(std::string name);

    // Method used to clear the structure reordering information map.
    void clearNeedsRewritten();

    // Clear the set of structures that have been rewritten already.
    void clearRewritten();
};


// Class containing all relevant semantic information.
class SemanticData {
private:

    // The path to the main file.
    std::string mainFilePath;

    // Structreordering information.
    StructReordering* structReordering;

public:

    // Get path to the main file.
    std::string getMainFilePath() { return this->mainFilePath; }

    // Method to set the main file path.
    void setMainFilePath(std::string mainFilePath) {
        this->mainFilePath = mainFilePath;
    }

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
