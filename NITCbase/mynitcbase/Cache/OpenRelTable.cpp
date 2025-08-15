#include "OpenRelTable.h"
#include <cstring>
#include <cstdlib>
#include <stdlib.h>
#include <cstdio>

OpenRelTable::OpenRelTable() {

    for (int i = 0; i < MAX_OPEN; ++i) 
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }
    /************** 1) RELATION CACHE TABLE ***************/
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    struct RelCacheEntry relCacheEntry;

    for (int slot = 0; slot <=2; slot ++)
    {
        relCatBlock.getRecord(relCatRecord, slot);
        
        RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);  // NOTE: Cache vs. Cat
        relCacheEntry.recId.block = RELCAT_BLOCK;
        relCacheEntry.recId.slot = slot;

        // Allocating to heap because we want it to persist outside this function
        RelCacheTable::relCache[slot] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
        *(RelCacheTable::relCache[slot]) = relCacheEntry;


    }


    /******************* 2) ATTRIBUTE CACHE TABLE *********************/
    /**** 2.1) Setting up RELCAT attributes in the Attribute Cache Table from ATTRCAT ****/
    RecBuffer attrCatBlock(ATTRCAT_BLOCK);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    AttrCacheEntry* attrCacheHead = nullptr;
    AttrCacheEntry* prev = nullptr;
    for (int i = 0; i < RELCAT_NO_ATTRS; i++)
    {
        AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        attrCatBlock.getRecord(attrCatRecord, i);
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = i;
        attrCacheEntry->next = nullptr;

        if (attrCacheHead == nullptr)
            attrCacheHead = attrCacheEntry;
        else
            prev->next = attrCacheEntry;
        prev = attrCacheEntry;

    }
    AttrCacheTable::attrCache[RELCAT_RELID] = attrCacheHead;

    /**** 2.2) Setting up ATTRCAT attributes in the Attribute Cache Table from ATTRCAT ****/
    attrCacheHead = nullptr;
    prev = nullptr;
    for (int i = 6; i < 12; i++)               // Slots for RELCAT = 0-5, Slots for ATTRCAT = 6-11
    {
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = i;
        attrCacheEntry->next = nullptr;

        if (attrCacheHead == nullptr)
            attrCacheHead = attrCacheEntry;
        else
            prev->next = attrCacheEntry;
        prev = attrCacheEntry;
    }
    AttrCacheTable::attrCache[ATTRCAT_RELID] = attrCacheHead;

    /**** 2.3) Setting up Students attributes in the Attribute Cache Table from ATTRCAT ****/
    struct RelCacheEntry studentsCacheEntry;
    relCatBlock.getRecord(relCatRecord, 2);   
    RelCacheTable::recordToRelCatEntry(relCatRecord, &studentsCacheEntry.relCatEntry);
    int studentsNumAttrs = studentsCacheEntry.relCatEntry.numAttrs;

    attrCacheHead = nullptr;
    prev = nullptr;
    for (int i = 12; i < 12 + studentsNumAttrs; i++)               // Slots after ATTRCAT
    {
        attrCatBlock.getRecord(attrCatRecord, i);

        AttrCacheEntry* attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCacheEntry->attrCatEntry);
        attrCacheEntry->recId.block = ATTRCAT_BLOCK;
        attrCacheEntry->recId.slot = i;
        attrCacheEntry->next = nullptr;

        if (attrCacheHead == nullptr)
            attrCacheHead = attrCacheEntry;
        else
            prev->next = attrCacheEntry;
        prev = attrCacheEntry;
    }
    AttrCacheTable::attrCache[ATTRCAT_RELID+1] = attrCacheHead;

    // for (int i = 0; i < 3; ++i) 
    // {
    //     printf ("%s\n", RelCacheTable::relCache[i]->relCatEntry.relName);
    // }
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {

    if (!strcmp(relName, RELCAT_RELNAME))
        return RELCAT_RELID;
    else if (!strcmp(relName, ATTRCAT_RELNAME))
        return ATTRCAT_RELID;
    else if (!strcmp(relName, "Students"))
        return 2;
    else
        return E_RELNOTOPEN;
}

OpenRelTable::~OpenRelTable() {
    for (int i = 0; i < MAX_OPEN; i++)
    {
        free(RelCacheTable::relCache[i]);
        for (AttrCacheEntry* entry = AttrCacheTable::attrCache[i]; entry != nullptr; ) {
            AttrCacheEntry* nextEntry = entry->next;
            free(entry);
            entry = nextEntry;
        }
    }
}