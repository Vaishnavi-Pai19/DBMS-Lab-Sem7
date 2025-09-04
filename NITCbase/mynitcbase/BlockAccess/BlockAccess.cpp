#include "BlockAccess.h"
#include <cstring>
#include <cstdio>

// SELECT * FROM RelName INTO TargetName WHERE Attribute op value;
RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {
    // Getting the previous search index of the relation relId from the relation cache
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);
    int block, slot;

    // If current search index is invalid, search should start from first record
    if (prevRecId.block == -1 && prevRecId.slot == -1)
    {
        RelCatEntry relCatBuf;
        RelCacheTable::getRelCatEntry(relId, &relCatBuf);

        block = relCatBuf.firstBlk;
        slot = 0;
    }
    else
    {
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    // NOTE! SearchIndex has record ID type
    while (block != -1)
    {
        RecBuffer recBuffer(block);

        HeadInfo head;
        recBuffer.getHeader(&head);

        Attribute record[head.numAttrs];
        recBuffer.getRecord(record, slot);

        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);

        // If slot >= the number of slots per block, then no more slots in this block
        if (slot >= head.numSlots)
        {
            block = head.rblock;
            slot = 0;
            continue;
        }

        if (slotMap[slot] == SLOT_UNOCCUPIED) {
            slot++;
            continue;
        }
        
        // Finding the offset of the attribute using the overloaded function by searching for the attribute
        AttrCatEntry attrCatBuffer;
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuffer);
        int attrOffset = attrCatBuffer.offset;

        int cmpVal = compareAttrs(record[attrOffset], attrVal, attrCatBuffer.attrType);

        /* Next task is to check whether this record satisfies the given condition.
           It is determined based on the output of previous comparison and
           the op value received.
           The following code sets the cond variable if the condition is satisfied.
        */
        if (
            (op == NE && cmpVal != 0) ||    // if op is "not equal to"
            (op == LT && cmpVal < 0) ||     // if op is "less than"
            (op == LE && cmpVal <= 0) ||    // if op is "less than or equal to"
            (op == EQ && cmpVal == 0) ||    // if op is "equal to"
            (op == GT && cmpVal > 0) ||     // if op is "greater than"
            (op == GE && cmpVal >= 0)       // if op is "greater than or equal to"
        ) {
            // Setting searchIndex for the record that satisfied it
            RecId searchIndex = {block, slot};
            RelCacheTable::setSearchIndex(relId, &searchIndex);
            return searchIndex;
        }

        slot++;
    }

    return RecId{-1, -1};
}

int BlockAccess::insert(int relId, Attribute *record) {
    RelCatEntry relCatBuf;
    RelCacheTable::getRelCatEntry(relId, &relCatBuf);

    
    int blockNum = relCatBuf.firstBlk;
    RecId rec_id = {-1, -1};

    int numSlots = relCatBuf.numSlotsPerBlk;
    int numAttributes = relCatBuf.numAttrs;

    int prevBlockNum = -1;

    /*
        Traversing the linked list of existing record blocks of the relation
        until a free slot is found OR
        until the end of the list is reached
    */
    while (blockNum != -1) 
    {
        RecBuffer currentBlock(blockNum);
        HeadInfo head;
        currentBlock.getHeader(&head);

        unsigned char slotMap[numSlots];
        currentBlock.getSlotMap(slotMap);
        int freeSlot = -1;
        for (int i = 0; i<numSlots; i++)
        {
            if (slotMap[i] == SLOT_UNOCCUPIED)
            {
                freeSlot = i;
                break;
            }
        }

        if (freeSlot != -1)
        {
            rec_id.block = blockNum;
            rec_id.slot = freeSlot;
            break;
        }
        else
        {
            prevBlockNum = blockNum;
            blockNum = head.rblock;
        }
    }

    // No free slot in the existing record blocks
    if (rec_id.block == -1 && rec_id.slot == -1)
    {
        if (relId == RELCAT_RELID)
            return E_MAXRELATIONS;
        else
        {
            RecBuffer newBlock;
            int ret = newBlock.getBlockNum();
            if (ret == E_DISKFULL)
                return E_DISKFULL;

            rec_id.block = ret;
            rec_id.slot = 0;

            HeadInfo newHeader;
            newBlock.getHeader(&newHeader);

            newHeader.blockType = REC;
            newHeader.pblock = -1;
            newHeader.lblock = prevBlockNum;
            newHeader.rblock = -1;
            newHeader.numEntries = 0;
            newHeader.numAttrs = numAttributes;
            newHeader.numSlots = numSlots;
            newBlock.setHeader(&newHeader);

            unsigned char newSlotMap[numSlots];
            for (int i = 0; i < numSlots; i++)
                newSlotMap[i] = SLOT_UNOCCUPIED;
            newBlock.setSlotMap(newSlotMap);

            if (prevBlockNum != -1)
            {
                RecBuffer prevBlock(prevBlockNum);
                HeadInfo prevHeader;
                prevBlock.getHeader(&prevHeader);
                prevHeader.rblock = rec_id.block;
                prevBlock.setHeader(&prevHeader);
            }
            else
            {
                relCatBuf.firstBlk = rec_id.block;
                RelCacheTable::setRelCatEntry(relId, &relCatBuf);
            }
            relCatBuf.lastBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatBuf);
        }
    }
    RecBuffer insertBlock(rec_id.block);
    insertBlock.setRecord(record, rec_id.slot);

    unsigned char insertSlotMap[numSlots];
    insertBlock.getSlotMap(insertSlotMap);
    insertSlotMap[rec_id.slot] = SLOT_OCCUPIED;
    insertBlock.setSlotMap(insertSlotMap);

    HeadInfo insertHeader;
    insertBlock.getHeader(&insertHeader);
    insertHeader.numEntries++;
    insertBlock.setHeader(&insertHeader);

    relCatBuf.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatBuf);

    return SUCCESS;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    RecId recId;
    recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);

    if (recId.block == -1 && recId.slot == -1)
        return E_NOTFOUND;

    RecBuffer recBuffer(recId.block);
    recBuffer.getRecord(record, recId.slot);

    return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {
    if (!strcmp(relName, (char*)RELCAT_RELNAME) || !strcmp(relName, (char*)ATTRCAT_RELNAME))
        return E_NOTPERMITTED;

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr; 
    strcpy(relNameAttr.sVal, relName);

    RecId searchIndex;
    searchIndex = linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
    if (searchIndex.block == -1 && searchIndex.slot == -1)
        return E_RELNOTEXIST;

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    RecBuffer relCatBuffer(searchIndex.block);
    relCatBuffer.getRecord(relCatEntryRecord, searchIndex.slot);
    
    int firstBlock = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    int numAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
    int recBlockNum = firstBlock;

    /************* 1) Deleting all the record blocks of the relation ***************/
    while (recBlockNum != -1)
    {
        RecBuffer recBuffer(recBlockNum);
        HeadInfo head;
        recBuffer.getHeader(&head);

        recBlockNum = head.rblock;
        recBuffer.releaseBlock(); 
    }
   
    /******** 2) Deleting Attribute Catalog Entries from ATTRCAT corresponding to the relation *********/
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numberOfAttributesDeleted = 0;

    while(true) {
        RecId attrCatRecId;
        attrCatRecId = linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

        if (attrCatRecId.block == -1 && attrCatRecId.slot == -1)
            break;

        numberOfAttributesDeleted++;

        RecBuffer attrCatBuffer(attrCatRecId.block);
        HeadInfo header;
        attrCatBuffer.getHeader(&header);

        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, attrCatRecId.slot);

        // Storing the root block index to delete indexes later on
        int rootBlock = attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        // Updating slotMap to reflect the Unoccupied block
        unsigned char slotMap[header.numSlots];
        attrCatBuffer.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        attrCatBuffer.setSlotMap(slotMap);

        // Decreasing the number of entries for ATTRCAT in header
        header.numEntries--;
        attrCatBuffer.setHeader(&header);

        // If number of entries become 0, releaseBlock is called after fixing the linked list.
        if (header.numEntries == 0) {
            // Standard doubly linked list
            RecBuffer prevBlock(header.lblock);
            HeadInfo leftHead;
            prevBlock.getHeader(&leftHead);

            // Setting rblock of left block to this block's rblock
            leftHead.rblock = header.rblock;
            prevBlock.setHeader(&leftHead);

            if (header.rblock != -1) 
            {
                RecBuffer nextBlock(header.rblock);
                HeadInfo rightHead;
                nextBlock.getHeader(&rightHead);
                
                rightHead.lblock = header.lblock;
                nextBlock.setHeader(&rightHead);

            } else 
            {
                // The block being released is the last block in the relation
                RelCatEntry relCatEntryBuffer;
				RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);

				relCatEntryBuffer.lastBlk = header.lblock;
            }

            attrCatBuffer.releaseBlock();
        }

        // (the following part is only relevant once indexing has been implemented)
        // if index exists for the attribute (rootBlock != -1), call bplus destroy
        if (rootBlock != -1) {
            
        }
    }

    /****** 3) Deleting Relation Catalog entry from RELCAT corresponding to the relation ******/
    HeadInfo relCatHead;
    RecBuffer relCatEntryBuffer(RELCAT_BLOCK);
    relCatEntryBuffer.getHeader(&relCatHead);

    // Decrementing the number of entries
    relCatHead.numEntries--;
    relCatEntryBuffer.setHeader(&relCatHead);
    
    unsigned char relCatSlotMap[relCatHead.numSlots];
    relCatEntryBuffer.getSlotMap(relCatSlotMap);
    relCatSlotMap[searchIndex.slot] = SLOT_UNOCCUPIED;
    relCatEntryBuffer.setSlotMap(relCatSlotMap);

    /*************** 4) Updating the Relation Cache Table *********************/

    // 4.1) Updating the RELCAT record entry in relation cache to reflect one less relation
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntry);
    relCatEntry.numRecs--;
    RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntry);
    

    // 4.2) Updating the ATTRCAT entry in relation cache to reflect the decrease in number of attributes
    RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
    relCatEntry.numRecs -= numberOfAttributesDeleted;
    RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);

    return SUCCESS;
}
    

int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){
    // NOTE! Only Relation Cache table has searchIndex ie. only RelCacheEntry
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    // Searching for new relation name
    Attribute newRelationName;
    memcpy(newRelationName.sVal, newName, ATTR_SIZE);   
    RecId searchIndex = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, newRelationName, EQ);
    // printf ("%d, %d\n", searchIndex.block, searchIndex.slot);

    if (searchIndex.block != -1 && searchIndex.slot != -1)
        return E_RELEXIST;

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    
    // Searching for old relation name
    Attribute oldRelationName;    
    memcpy(oldRelationName.sVal, oldName, ATTR_SIZE);  
    searchIndex = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, oldRelationName, EQ);

    if (searchIndex.block == -1 && searchIndex.slot == -1)
        return E_RELNOTEXIST;

    RecBuffer relationBuffer(searchIndex.block);
    Attribute record[RELCAT_NO_ATTRS];
    
    relationBuffer.getRecord(record, searchIndex.slot);
    memcpy(record[RELCAT_REL_NAME_INDEX].sVal, newName, ATTR_SIZE);

    relationBuffer.setRecord(record, searchIndex.slot);

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numAttrs = record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    // Going through records of attribute catalog
    for (int i = 0; i < numAttrs; i++)
    {
        searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);

        if(searchIndex.block == -1 && searchIndex.slot == -1)
            return E_RELNOTEXIST;

        RecBuffer attrCatBlock(searchIndex.block);
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBlock.getRecord(attrCatRecord, searchIndex.slot);
        strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
        attrCatBlock.setRecord(attrCatRecord, searchIndex.slot);
    }

    return SUCCESS;
}

int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {
    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;    
    memcpy(relNameAttr.sVal, relName, ATTR_SIZE); 

    RecId searchIndex = BlockAccess::linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
    if (searchIndex.block == -1 && searchIndex.slot == -1)
        return E_RELNOTEXIST;

    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    // attrToRenameRecId used to store the attr-cat recId of the attribute to rename
    RecId attrToRenameId = {-1, -1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    while(true) {
        RecId attrRecId = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

        if (attrRecId.block == -1 && attrRecId.slot == -1)
            break;

        RecBuffer attrCatBlock(attrRecId.block);
        attrCatBlock.getRecord(attrCatEntryRecord, attrRecId.slot);

        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0) 
        {
            attrToRenameId = attrRecId;
            break;
        }

        if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0)
        {
            return E_ATTREXIST;
        }
    }

    if (attrToRenameId.block == -1 && attrToRenameId.slot == -1)
        return E_ATTRNOTEXIST;

    RecBuffer bufferToRename(attrToRenameId.block);
    Attribute recordToRename[ATTRCAT_NO_ATTRS];

    bufferToRename.getRecord(recordToRename, attrToRenameId.slot);
    memcpy(recordToRename[ATTRCAT_ATTR_NAME_INDEX].sVal, newName, ATTR_SIZE);
    bufferToRename.setRecord(recordToRename, attrToRenameId.slot);

    return SUCCESS;
}