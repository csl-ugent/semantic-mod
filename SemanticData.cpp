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

void StructReordering::removeStructData(std::string name) {

    // Safely clear the associated memory and remove the struct from the map.
    StructData* data = structMap[name];
    delete data;
    structMap.erase(name);
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

bool FPReordering::isInFunctionMap(std::string name) {

    // We check if the name is already defined in the map or not.
    return !(this->functionMap.find(name) == this->functionMap.end());
}

bool FPReordering::isInFunctionReorderingMap(std::string name) {

    // We check if the name is already defined in the map or not.
    return !(this->functionReorderings.find(name) == this->functionReorderings.end());
}

void FPReordering::addFunctionData(std::string name, FunctionData* data) {

    // We add the function and its corresponding data to the function map.
    this->functionMap[name] = data;
}

void FPReordering::addFunctionReorderingData(std::string name, FunctionData* data) {

    // We add the function and its corresponding data to the function
    // reordering data map.
    this->functionReorderings[name] = data;
}

bool FPReordering::hasBeenRewritten(std::string name) {

    // We check if the name is already defined in the set or not.
    return !(this->rewritten.find(name) == this->rewritten.end());
}

void FPReordering::functionRewritten(std::string name) {
    this->rewritten.insert(name);
}

bool SWCReordering::isInSwitchMap(unsigned key) {

    // We check if the name is already defined in the map or not.
    return !(this->switchMap.find(key) == this->switchMap.end());
}

bool SWCReordering::isInSwitchReorderingMap(unsigned key) {

    // We check if the name is already defined in the map or not.
    return !(this->switchReorderings.find(key) == this->switchReorderings.end());
}

void SWCReordering::addSwitchData(unsigned key, SwitchData* data) {

    // We add the switch and its corresponding data to the switch map.
    this->switchMap[key] = data;
}

void SWCReordering::addSwitchReorderingData(unsigned key, SwitchData* data) {

    // We add the function and its corresponding data to the switch
    // reordering data map.
    this->switchReorderings[key] = data;
}

void SWCReordering::switchRewritten(unsigned key) {
    this->rewritten.insert(key);
}

bool SWCReordering::hasBeenRewritten(unsigned key) {

    // We check if the name is already defined in the set or not.
    return !(this->rewritten.find(key) == this->rewritten.end());
}

FPReordering::~FPReordering() {

    // We deallocate all allocated memory.
    std::map<std::string, FunctionData*>::iterator it;

    for (it = this->functionMap.begin(); it != this->functionMap.end(); it++ )
    {
        // We delete the allocated FunctionData objects.
        delete it->second;
    }

    for (it = this->functionReorderings.begin(); it != this->functionReorderings.end(); it++ )
    {
        // We delete the allocated FunctionData objects.
        delete it->second;
    }
}

SWCReordering::~SWCReordering() {

    // We deallocate all allocated memory.
    std::map<unsigned, SwitchData*>::iterator it;

    for (it = this->switchMap.begin(); it != this->switchMap.end(); it++ )
    {
        // We delete the allocated SwitchData objects.
        delete it->second;
    }

    for (it = this->switchReorderings.begin(); it != this->switchReorderings.end(); it++ )
    {
        // We delete the allocated SwitchData objects.
        delete it->second;
    }
}

void StructData::addFieldData(int position, std::string fieldName, std::string fieldType, clang::SourceRange sourceRange) {

    // We add the data to the existing field data.
    this->fieldData.push_back(FieldData({position, fieldName, fieldType, sourceRange}));
}

void FunctionData::addFieldData(int position, std::string fieldName, std::string fieldType, clang::SourceRange sourceRange) {

    // We add the data to the existing field data.
    this->fieldData.push_back(FieldData({position, fieldName, fieldType, sourceRange}));
}

void StructData::addFieldDefinition(std::string definition) {

    // We add the definition to the collection of definitions.
    this->fieldDefinitions.insert(definition);
}

bool StructData::hasFieldDefinitions() {

    // We check the size of the field definitions.
    return this->fieldDefinitions.size() > 0;
}


void StructReordering::clearStructReorderings() {

    // We empty (clear) the set itself...
    this->structReorderings.clear();
}

void StructReordering::clearRewritten() {

    // We empty (clear) the set itself...
    this->rewritten.clear();
}


void FPReordering::clearFunctionReorderings() {

    // We empty (clear) the set itself...
    this->functionReorderings.clear();
}

void FPReordering::clearRewritten() {

    // We empty (clear) the set itself...
    this->rewritten.clear();
}


void SWCReordering::clearSwitchReorderings() {

    // We empty (clear) the set itself...
    this->switchReorderings.clear();
}

void SWCReordering::clearRewritten() {

    // We empty (clear) the set itself...
    this->rewritten.clear();
}
