#include "AttrCacheTable.h"
#include <cstring>

AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

// Returns the attrOffset-th attribute for the relation corresponding to relId
int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
        if (entry->attrCatEntry.offset == attrOffset) {
            *attrCatBuf = entry->attrCatEntry;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

/* Converts a attribute catalog record to AttrCatEntry struct
    We get the record as Attribute[] from the BlockBuffer.getRecord() function.
    This function will convert that to a struct AttrCatEntry type.
*/
void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry* attrCatEntry) {
    strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
    strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
    attrCatEntry->attrType = (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
    attrCatEntry->primaryFlag = (bool)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
    attrCatEntry->rootBlock = (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
    attrCatEntry->offset = (int)record[ATTRCAT_OFFSET_INDEX].nVal;
}

// Overloading the function to instead find an attribute of a relation with a particular name.
int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry* attrCatBuf) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0) {
            strcpy(attrCatBuf->relName, entry->attrCatEntry.relName);
            strcpy(attrCatBuf->attrName, entry->attrCatEntry.attrName);
            attrCatBuf->attrType = entry->attrCatEntry.attrType;
            attrCatBuf->offset = entry->attrCatEntry.offset;
            attrCatBuf->primaryFlag = entry->attrCatEntry.primaryFlag;
            attrCatBuf->rootBlock = entry->attrCatEntry.rootBlock;
            return SUCCESS;
        }
    }
    
    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0)
        {
            searchIndex->block = entry->searchIndex.block;
            searchIndex->index = entry->searchIndex.index;

            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

// Overloading with Attribute Offset
int AttrCacheTable::getSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if (entry->attrCatEntry.attrOffset == attrOffset)
        {
            searchIndex->block = entry->searchIndex.block;
            searchIndex->index = entry->searchIndex.index;

            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if (strcmp(entry->attrCatEntry.attrName, attrName) == 0)
        {
            entry->searchIndex.block = searchIndex->block;
            entry->searchIndex.index = searchIndex->index;

            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

// Overloading with Attribute Offset
int AttrCacheTable::setSearchIndex(int relId, int attrOffset, IndexId *searchIndex) {
    if (relId < 0 || relId >= MAX_OPEN) {
        return E_OUTOFBOUND;
    }

    if (attrCache[relId] == nullptr) {
        return E_RELNOTOPEN;
    }

    for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next)
    {
        if (entry->attrCatEntry.attrOffset == attrOffset)
        {
            entry->searchIndex.block = searchIndex->block;
            entry->searchIndex.index = searchIndex->index;

            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE]) {
    IndexId indexId = {-1, -1};
    int ret = AttrCacheTable::setSearchIndex(relId, attrName, &indexId);
    return ret;
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset) {
    IndexId indexId = {-1, -1};
    int ret = AttrCacheTable::setSearchIndex(relId, attrOffset, &indexId);
    return ret;
}

