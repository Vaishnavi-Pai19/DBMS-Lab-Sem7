#include "OpenRelTable.h"
#include <cstring>
#include <cstdlib>
#include <stdlib.h>

OpenRelTable::OpenRelTable() {

    for (int i = 0; i < MAX_OPEN; ++i) 
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }
    /************** 1) RELATION CACHE TABLE ***************/
    /**** 1.1) Setting up Relation Catalog relation in the Relation Cache Table ****/

    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);       // Slot Number = 0

    struct RelCacheEntry relCacheEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);  // NOTE: Cache vs. Cat
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    // Allocating to heap because we want it to persist outside this function
    RelCacheTable::relCache[RELCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;

    /**** 1.2) Setting up Attribute Catalog relation in the Relation Cache Table ****/

    relCatBlock.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);    // Slot Number = 1
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_ATTRCAT;

    // Allocating to heap
    RelCacheTable::relCache[ATTRCAT_RELID] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;

    /**** 1.3) Setting up Students relation in the Relation Cache Table ****/

    relCatBlock.getRecord(relCatRecord, 2);   
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    relCacheEntry.recId.block = RELCAT_BLOCK;
    relCacheEntry.recId.slot = 2;

    // Allocating to heap
    RelCacheTable::relCache[ATTRCAT_RELID + 1] = (struct RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    *(RelCacheTable::relCache[ATTRCAT_RELID + 1]) = relCacheEntry;


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
    attrCacheHead = nullptr;
    prev = nullptr;
    for (int i = 12; i < 18; i++)               // Slots fafter ATTRCAT
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