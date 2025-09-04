#include "OpenRelTable.h"
#include <cstring>
#include <cstdlib>
#include <stdlib.h>
#include <cstdio>


OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

AttrCacheEntry* createList(int length) {
    AttrCacheEntry* head = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
    AttrCacheEntry* tail = head;
    for (int i = 1; i < length; i++) {
        tail->next = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
        tail = tail->next;
    }
    tail->next = nullptr;
    return head;
}

OpenRelTable::OpenRelTable() {

    for (int i = 0; i < MAX_OPEN; ++i) 
    {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
        OpenRelTable::tableMetaInfo[i].free = true;
    }

    /************** 1) RELATION CACHE TABLE ***************/
    RecBuffer relCatBlock(RELCAT_BLOCK);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    struct RelCacheEntry relCacheEntry;

    for (int slot = 0; slot <= 1; slot ++)          // Only RELCAT and ATTRCAT
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

    
    /******************* 3) TABLEMETAINFO TABLE *********************/
    for (int i = RELCAT_RELID; i <= ATTRCAT_RELID; i++)
    {
        relCatBlock.getRecord(relCatRecord, i);
        RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
        
        tableMetaInfo[i].free = false;        // False ie. it is occupied
        memcpy(tableMetaInfo[i].relName, relCacheEntry.relCatEntry.relName, ATTR_SIZE);
    }
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
    for (int i = 0; i < MAX_OPEN; i++) 
    {
        if ((tableMetaInfo[i].free == false) && strcmp(relName, tableMetaInfo[i].relName) == 0)
            return i;
    }

    return E_RELNOTOPEN;
}

int OpenRelTable::getFreeOpenRelTableEntry() {
    for (int i = 2; i < MAX_OPEN; i++) 
    {
        if (tableMetaInfo[i].free)
            return i;
    }

    return E_CACHEFULL;
}

int OpenRelTable::openRel(char relName[ATTR_SIZE]) {

    // Checking if the relation already has an entry in the Open Relation Table
    int entry = OpenRelTable::getRelId(relName);
    if (entry >= 0 && entry <= MAX_OPEN)
    {
        return entry;
    }
    
    // Finding a free slot in the Open Relation Table
    int freeSlot = OpenRelTable::getFreeOpenRelTableEntry();
    if (freeSlot == E_CACHEFULL)
    {
        return E_CACHEFULL;
    }
    int relId = freeSlot;

    /****** Setting up Relation Cache entry for the relation ******/

    Attribute relNameAttribute;                            // Holds relation name in the form of Union Attribute for string comparison
    memcpy(relNameAttribute.sVal, relName, ATTR_SIZE);
    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttribute, EQ);

    if (relcatRecId.block == -1 && relcatRecId.slot == -1) 
    {
        return E_RELNOTEXIST;
    }

    RecBuffer relCatBuffer(relcatRecId.block);
    Attribute relCatRecord[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(relCatRecord, relcatRecId.slot);

    RelCatEntry relCatEntry;
    RelCacheTable::recordToRelCatEntry(relCatRecord, &relCatEntry);

    RelCacheTable::relCache[freeSlot] = (RelCacheEntry*) malloc(sizeof(RelCacheEntry));
    RelCacheTable::relCache[freeSlot]->recId = relcatRecId;
    RelCacheTable::relCache[freeSlot]->relCatEntry = relCatEntry;

    /****** Setting up Attribute Cache entry for the relation ******/

    int numAttrs = relCatEntry.numAttrs;
    AttrCacheEntry* listHead = createList(numAttrs);
    AttrCacheEntry* node = listHead;

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    // Iterating over all the entries in ATTRCAT for all attributes of relation relName
    for (int i = 0; i < numAttrs; i++)
    {
        RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttribute, EQ);

        if (attrcatRecId.block == -1 && attrcatRecId.slot == -1)
            break;

        RecBuffer attrRecBuffer(attrcatRecId.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrRecBuffer.getRecord(attrCatRecord, attrcatRecId.slot);

        AttrCatEntry attrCatEntry;
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &attrCatEntry);

        node->recId = attrcatRecId;
        node->attrCatEntry = attrCatEntry;
        node = node->next;
    }
    AttrCacheTable::attrCache[freeSlot] = listHead;

    /****** Setting up metadata in the Open Relation Table for the relation******/

    OpenRelTable::tableMetaInfo[relId].free = false;
    memcpy(OpenRelTable::tableMetaInfo[relId].relName, relName, ATTR_SIZE);

    return relId;
}

int OpenRelTable::closeRel(int relId) {
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
        return E_NOTPERMITTED;

    if (relId < 0 || relId >= MAX_OPEN)
        return E_OUTOFBOUND;

    if (OpenRelTable::tableMetaInfo[relId].free)
        return E_RELNOTOPEN;

    if (RelCacheTable::relCache[relId]->dirty == true)
    {
        RelCatEntry relCatEntry = RelCacheTable::relCache[relId]->relCatEntry;
        Attribute record[RELCAT_NO_ATTRS];
        RelCacheTable::relCatEntryToRecord(&relCatEntry, record);

        RecId recId = RelCacheTable::relCache[relId]->recId;
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(record, recId.slot);
    }

    free(RelCacheTable::relCache[relId]);
    for (AttrCacheEntry* entry = AttrCacheTable::attrCache[relId]; entry != nullptr; ) {
            AttrCacheEntry* nextEntry = entry->next;
            free(entry);
            entry = nextEntry;
        }

    OpenRelTable::tableMetaInfo[relId].free = true;

    RelCacheTable::relCache[relId] = nullptr;
    AttrCacheTable::attrCache[relId] = nullptr;

    return SUCCESS;
}

OpenRelTable::~OpenRelTable() {
    for (int i = 2; i < MAX_OPEN; ++i) {          // Closing all the open relations for relId >= 2
        if (tableMetaInfo[i].free == false) {
            OpenRelTable::closeRel(i);            
        }
    }

    /**** Closing the catalog relations in the Relation Cache ****/
    // 1) Releasing the Relation Cache Entry of Atttribute Catalog

    if (RelCacheTable::relCache[ATTRCAT_RELID]->dirty) 
    {
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        RelCatEntry relCatEntry = RelCacheTable::relCache[ATTRCAT_RELID]->relCatEntry;
        RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

        RecId recId = RelCacheTable::relCache[ATTRCAT_RELID]->recId;
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(relCatRecord, recId.slot);
    }
    free(RelCacheTable::relCache[ATTRCAT_RELID]);


    // 2) Releasing the Relation Cache Entry of the Relation Catalog
    if(RelCacheTable::relCache[RELCAT_RELID]->dirty) {
        Attribute relCatRecord[RELCAT_NO_ATTRS];
        RelCatEntry relCatEntry = RelCacheTable::relCache[ATTRCAT_RELID]->relCatEntry;
        RelCacheTable::relCatEntryToRecord(&relCatEntry, relCatRecord);

        RecId recId = RelCacheTable::relCache[RELCAT_RELID]->recId;
        RecBuffer relCatBlock(recId.block);
        relCatBlock.setRecord(relCatRecord, recId.slot);
    }
    free(RelCacheTable::relCache[RELCAT_RELID]);

    // 3) Freeing the memory allocated for Attribute Cache entries of RELCAT and ATTRCAT
    for (AttrCacheEntry* temp = AttrCacheTable::attrCache[RELCAT_RELID], *next; temp != nullptr; temp = next) {
        next = temp->next;
        free(temp);
    }
    for (AttrCacheEntry* temp = AttrCacheTable::attrCache[ATTRCAT_RELID], *next; temp != nullptr; temp = next) {
        next = temp->next;
        free(temp);
    }
}
