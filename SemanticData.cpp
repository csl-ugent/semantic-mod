#include "SemanticData.h"

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

void SWCReordering::clearSwitchReorderings() {

    // We empty (clear) the set itself...
    this->switchReorderings.clear();
}

void SWCReordering::clearRewritten() {

    // We empty (clear) the set itself...
    this->rewritten.clear();
}
