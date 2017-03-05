#include "SemanticData.h"

bool StructReordering::isInStructMap(std::string name) {

    // We check if the name is already defined in the map or not.
    return !(this->structMap.find(name) == this->structMap.end());
}


bool StructReordering::isInStructReorderingMap(std::string name) {

    // We check if the name is already defined in the map or not.
    return !(this->structReorderings.find(name) == this->structReorderings.end());
}

bool StructReordering::hasBeenRewritten(std::string name) {

    // We check if the name is already defined in the set or not.
    return !(this->rewritten.find(name) == this->rewritten.end());
}

void StructReordering::addStructData(std::string name, StructData* data) {

    // We add the structure and its corresponding data to the structure data map.
    this->structMap[name] = data;
}

void StructReordering::addStructReorderingData(std::string name, StructData* data) {

    // We add the structure and its corresponding data to the structure
    // reordering data map.
    this->structReorderings[name] = data;
}

void StructReordering::structRewritten(std::string name) {
    this->rewritten.insert(name);
}


StructReordering::~StructReordering() {

    // We deallocate all allocated memory.
    std::map<std::string, StructData*>::iterator it;

    for (it = this->structMap.begin(); it != this->structMap.end(); it++ )
    {
        // We delete the allocated StructData objects.
        delete it->second;
    }

    for (it = this->structReorderings.begin(); it != this->structReorderings.end(); it++ )
    {
        // We delete the allocated StructData objects.
        delete it->second;
    }
}

void StructData::addFieldData(int position, std::string fieldName, std::string fieldType, clang::SourceRange sourceRange) {

    // We add the data to the existing field data.
    this->fieldData.push_back(FieldData({position, fieldName, fieldType, sourceRange}));
}


void StructReordering::clearStructReorderings() {

    // We deallocate all allocated memory.
    std::map<std::string, StructData*>::iterator it;

    // We free the memory.
    for (it = this->structReorderings.begin(); it != this->structReorderings.end(); it++ )
    {
        // We delete the allocated StructData objects.
        delete it->second;
    }

    // We empty (clear) the map itself...
    this->structReorderings.clear();
}

void StructReordering::clearRewritten() {
    this->rewritten.clear();
}
