#include "BPlusTree.h"

#include <cstring>
#include <cstdio>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {
    
    IndexId searchIndex;
    AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    
    int block, index;

    /* If this is the first search, start at the root block and index 0 */
    if (searchIndex.block == -1 && searchIndex.index == -1) 
    {
        // Search done for the first time
        block = attrCatEntry.rootBlock;
        index = 0;

        if (attrCatEntry.rootBlock == -1) 
            return RecId{-1, -1};

    } else {
        /* A valid searchIndex points to an entry in the leaf index of the attribute's
           B+ Tree which had previously satisfied the op for the given attrVal */

        /* If continuing a previous search, move to the next entry in the leaf block */
        block = searchIndex.block;
        index = searchIndex.index + 1;  
        IndLeaf leaf(block);

        HeadInfo leafHead;
        leaf.getHeader(&leafHead);

        /* If at the end of a leaf block, move to the next leaf block */
        if (index >= leafHead.numEntries) {
    
            block = leafHead.rblock;
            index = 0;

            // If end of linked list reached, search is done
            if (block == -1) 
                return RecId{-1, -1};
        }
    }

    /****** Traversing through all the internal nodes according to attrVal and op ******/

    /* (This section is only needed when
        - search restarts from the root block (when searchIndex is reset by caller)
        - root is not a leaf
        If there was a valid search index, then we are already at a leaf block
        and the test condition in the following loop will fail)
    */

    while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) 
    { 
        IndInternal internalBlk(block);
        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        InternalEntry intEntry;

        if (op == NE || op == LT || op == LE) {
            /*
            - NE: need to search the entire linked list of leaf indices of the B+ Tree,
            starting from the leftmost leaf index. Thus, always move to the left.

            - LT and LE: the attribute values are arranged in ascending order in the
            leaf indices of the B+ Tree. Values that satisfy these conditions, if
            any exist, will always be found in the left-most leaf index. Thus,
            always move to the left.
            */

            // Loading entry in the first slot of the block
            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;

        } else 
        {
            /*
            - EQ, GT and GE: move to the left child of the first entry that is
            greater than (or equal to) attrVal
            (we are trying to find the first entry that satisfies the condition.
            since the values are in ascending order we move to the left child which
            might contain more entries that satisfy the condition)
            */

            /*
             traverse through all entries of internalBlk and find an entry that
             satisfies the condition.
             if op == EQ or GE, then intEntry.attrVal >= attrVal
             if op == GT, then intEntry.attrVal > attrVal
            */

            int i = 0;
            while (i < intHead.numEntries) 
            {
                internalBlk.getEntry(&intEntry, i);
                int cval = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);

                if (((op == EQ || op == GE) && cval >= 0) ||(op == GT && cval > 0))
                    break;
                i++;
            }

            if (i < intHead.numEntries) {
                // Move to the left child of that entry
                block =  intEntry.lChild;

            } else {
                // Move to the right child of the last entry of the block
                block =  intEntry.rChild;
            }
        }
    }

    // NOTE: `block` now has the block number of a leaf index block.
    /****** Traversing leaf index block by moving right to find entry that matches our condition ******/
    while (block != -1) {
        IndLeaf leafBlk(block);
        HeadInfo leafHead;
        leafBlk.getHeader(&leafHead);

        Index leafEntry;

        while (index < leafHead.numEntries) {
            leafBlk.getEntry(&leafEntry, index);

            // Comparison between leafEntry's attribute value and input attrVal
            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);

            if (
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)
            ) {
                IndexId searchIndex{block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
                return RecId{leafEntry.block, leafEntry.slot};
            } 
            else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                // Future entries will also not satisfy since ascending order
                return RecId{-1, -1};
            }
            ++index;
        }

        if (op != NE) {
            break;
        }

        block = leafHead.rblock;
        index = 0;
    }
    
    return RecId{-1, -1};
}

int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
        return E_NOTPERMITTED;

    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    if (ret != SUCCESS)
        return ret;

    if (attrCatEntry.rootBlock != -1)       // Index already exists
        return SUCCESS;

    /****** Creating a new B+ Tree ******/
    IndLeaf rootBlockBuf;

    // If the block could not be allocated, the appropriate error code will be stored in the blockNum member field of the object
    int rootBlock = rootBlockBuf.getBlockNum();
    if (rootBlock == E_DISKFULL) {
        return E_DISKFULL;
    }

    attrCatEntry.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    RelCatEntry relCatEntry;
    ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);
    int block = relCatEntry.firstBlk;

    /***** Traversing all the blocks in the relation and inserting them one by one into the B+ Tree *****/
    while (block != -1) {
        RecBuffer blockBuffer (block);
        unsigned char slotMap[relCatEntry.numSlotsPerBlk];
        blockBuffer.getSlotMap(slotMap);

        // Traversing through all occupied slots of the block
        for (int i = 0; i < relCatEntry.numSlotsPerBlk; i++)
        {
            if (slotMap[i] == OCCUPIED)
            {
                Attribute record[relCatEntry.numAttrs];
                blockBuffer.getRecord(record, i);

                RecId recId = RecId{block, i};
                ret = bPlusInsert(relId, attrName, record[attrCatEntry.offset], recId); // IMPORTANT!
                if (ret == E_DISKFULL)  // Not enough blocks to build bPlus Tree
                    return E_DISKFULL;

            }
        }
        HeadInfo head;
        blockBuffer.getHeader(&head);
        block = head.rblock;
    }

    return SUCCESS;
}

int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    if (ret != SUCCESS)
        return ret;

    int rootBlock = attrCatEntry.rootBlock;
    if (rootBlock == -1) 
        return E_NOINDEX;
    
    int leafBlkNum = findLeafToInsert(rootBlock, attrVal, attrCatEntry.attrType);

    Index leafEntry;
    leafEntry.attrVal = attrVal;
    leafEntry.block = recId.block;
    leafEntry.slot = recId.slot;

    ret = insertIntoLeaf(relId, attrName, leafBlkNum, leafEntry);
    if (ret == E_DISKFULL) {
        bPlusDestroy(rootBlock);
        attrCatEntry.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);
        
        return E_DISKFULL;
    }

    return SUCCESS;
}

int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {
    int blockNum = rootBlock;

    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF) {  
        IndInternal internalBlock(blockNum);
        HeadInfo header;
        internalBlock.getHeader(&header);
        int i = 0;

        // Finding the first entry whose attribute value >= value to be inserted
        while (i < header.numEntries)
        {
            InternalEntry temp;
            internalBlock.getEntry(&temp, i);

            if (compareAttrs(attrVal, temp.attrVal, attrType) <= 0){
                break;
            }
            i++;
        }
        
        if (i == header.numEntries)
        {
            InternalEntry temp;
            internalBlock.getEntry(&temp, header.numEntries-1);
            blockNum = temp.rChild;
        } 
        else 
        {
            InternalEntry temp;
            internalBlock.getEntry(&temp, i);
            blockNum = temp.lChild;
        }
    }

    return blockNum;
}

int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int blockNum, Index indexEntry) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    IndLeaf leafBlock(blockNum);
    HeadInfo header;
    leafBlock.getHeader(&header);

    // Array to hold list of index entries with existing indices + the new index to insert
    Index indices[header.numEntries + 1];
    Index leafEntry;
    int index = 0;
    int i = 0;

    for (i = 0; i < header.numEntries; i++)
    {
        leafBlock.getEntry(&leafEntry, i);
        if (compareAttrs(leafEntry.attrVal, indexEntry.attrVal, attrCatEntry.attrType) >= 0)
        {
            break;
        }
        // Inserting into the next position where next value should go
        indices[index] = leafEntry;
        index++;
    }
    // IMPORTANT! indexEntry = new entry to insert, leafEntry = temporary copy of existing entries
    indices[index] = indexEntry;
    index++;

    // Copying the remaining existing entries
    for (; i < header.numEntries; i++) 
    {
        leafBlock.getEntry(&leafEntry, i);
        indices[index] = leafEntry;
        index++;
    }

    // If leaf block has not reached its maximum limit
    if (header.numEntries != MAX_KEYS_LEAF) {
        header.numEntries++;
        leafBlock.setHeader(&header);
        
        for (int i = 0; i < header.numEntries; i++)
        {
            leafBlock.setEntry(&indices[i], i);
        }

        return SUCCESS;
    }

    // If we reached here, the `indices` array has more than entries than can fit in  leaf index block
    // Therefore, we will need to split the entries in `indices` between two leaf blocks
    int newRightBlk = splitLeaf(blockNum, indices);

    if (newRightBlk == E_DISKFULL)
        return E_DISKFULL;

    int retVal;
    // Case 1) Current leaf block is not the root block
    if (header.pblock != -1)
    {
        // Inserting middle entry into parent block
        InternalEntry internalEntry;
        internalEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal, 
        internalEntry.lChild = blockNum;
        internalEntry.rChild = newRightBlk;

        retVal = insertIntoInternal(relId, attrName, header.pblock, internalEntry);
    } 
    // Case 2) Current leaf block is the root block
    else       
        retVal = createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, blockNum, newRightBlk);

    if (retVal == E_DISKFULL)
        return E_DISKFULL;
    
    return SUCCESS;
}

int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    IndLeaf rightBlk;              // New index leaf block
    IndLeaf leftBlk(leafBlockNum); // Existing overflowing leaf index block

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = leafBlockNum;

    if (rightBlkNum == E_DISKFULL) 
        return E_DISKFULL;


    HeadInfo leftHeader, rightHeader;
    rightBlk.getHeader(&rightHeader);
    leftBlk.getHeader(&leftHeader);

    // Updating header of right block
    rightHeader.numEntries = 32;   // (MAX_KEYS_LEAF+1)/2
    rightHeader.pblock = leftHeader.pblock;
    rightHeader.lblock = leftBlkNum;
    rightHeader.rblock = leftHeader.rblock;
    rightBlk.setHeader(&rightHeader);

    // Updating header of left block
    leftHeader.numEntries = 32;   
    leftHeader.rblock = rightBlkNum;
    leftBlk.setHeader(&leftHeader);

    for (int i = 0; i <= MIDDLE_INDEX_LEAF; i++)       // Each leaf block will have 32 entries
    {
        leftBlk.setEntry(&indices[i], i);
        rightBlk.setEntry(&indices[i+32], i);
    }

    return rightBlkNum;
}

int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    IndInternal intBlock(intBlockNum);
    HeadInfo header;
    intBlock.getHeader(&header);

    // Array to hold existing entries + the new entry
    InternalEntry intEntries[header.numEntries + 1];

    /*
    Update the lChild of the internalEntry immediately following the newly added
    entry to the rChild of the newly added entry.
    */

    InternalEntry tempEntry;
    int index = 0;
    int i = 0;

    for (i = 0; i < header.numEntries; i++)
    {
        intBlock.getEntry(&tempEntry, i);
        if (compareAttrs(tempEntry.attrVal, intEntry.attrVal, attrCatEntry.attrType) >= 0)
        {
            if (index > 0)
            {
                intEntries[index-1].rChild = intEntry.lChild;
            }
            break;
        }
        // Inserting into the next position where next value should go
        intEntries[index++] = tempEntry;
    }
    intEntries[index++] = intEntry;

    if (i < header.numEntries)
    {
        intBlock.getEntry(&tempEntry, i);
        tempEntry.lChild = intEntry.rChild;
        intEntries[index] = tempEntry;
        index++;
        i++;
    }

    for (;i < header.numEntries; i++)
    {
        intBlock.getEntry(&tempEntry, i);
        intEntries[index] = tempEntry;
        index++;
    }

    if (header.numEntries != MAX_KEYS_INTERNAL) {
        header.numEntries++;
        intBlock.setHeader(&header);
        
        for (int j = 0; j < header.numEntries; j++)
        {
            intBlock.setEntry(&intEntries[j], j);
        }
        return SUCCESS;
    }

    // If we reached here, the `internalEntries` array has more than entries than can fit in a single internal index block
    //  Therefore, we will need to split the entries in `internalEntries` between two internal index blocks
    int newRightBlk = splitInternal(intBlockNum, intEntries);

    if (newRightBlk == E_DISKFULL) {
        bPlusDestroy(intEntry.rChild);
        return E_DISKFULL;
    }

    int retVal;
    // Case 1) Current block not root
    if (header.pblock != -1) 
    {  
        // Inserting middle value from 'intEntries' into parent block
        InternalEntry internalEntry;
        internalEntry.attrVal = intEntries[MIDDLE_INDEX_INTERNAL].attrVal;
        internalEntry.lChild = intBlockNum;
        internalEntry.rChild = newRightBlk;

        retVal = insertIntoInternal(relId, attrName, header.pblock, internalEntry);
    } else 
    // Case 2) Current block is root and now split
    {
        retVal = createNewRoot(relId, attrName, intEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
    }

    if (retVal == E_DISKFULL)
        return E_DISKFULL;

    return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {
    IndInternal rightBlk;              // New internal index block
    IndInternal leftBlk(intBlockNum);  // Existing internal index block

    int rightBlkNum = rightBlk.getBlockNum();
    int leftBlkNum = intBlockNum;

    if (rightBlkNum == E_DISKFULL) 
        return E_DISKFULL;
    

    HeadInfo leftHeader, rightHeader;
    rightBlk.getHeader(&rightHeader);
    leftBlk.getHeader(&leftHeader);

    rightHeader.numEntries = (MAX_KEYS_INTERNAL)/2;    // 50
    rightHeader.pblock = leftHeader.pblock;
    rightBlk.setHeader(&rightHeader);

    leftHeader.numEntries = (MAX_KEYS_INTERNAL)/2;
    leftBlk.setHeader(&leftHeader);

    // First 50 entries of leftBlk = index 0 to 49 of internalEntries
    // First 50 entries of newRightBlk = index 51 to 100 of internalEntries
    // Index 50 will be moving to the parent internal index block
    for (int i = 0; i < 50; i++)       
    {
        leftBlk.setEntry(&internalEntries[i], i);
        rightBlk.setEntry(&internalEntries[i+51], i);
    }
    int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild);
    BlockBuffer blockbuffer (internalEntries[MIDDLE_INDEX_INTERNAL+1].lChild);

    HeadInfo blockHeader;
    blockbuffer.getHeader(&blockHeader);
    blockHeader.pblock = rightBlkNum;
    blockbuffer.setHeader(&blockHeader);

    // Updating the pblock of all entries in new right block
    for (int i = 0; i < MIDDLE_INDEX_INTERNAL; i++)
    {
        BlockBuffer blockbuffer (internalEntries[i + 51].rChild);
        blockbuffer.getHeader(&blockHeader);
        blockHeader.pblock = rightBlkNum;
        blockbuffer.setHeader(&blockHeader);
    }

    return rightBlkNum;
}

int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);

    IndInternal newRootBlk;
    int newRootBlkNum = newRootBlk.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) {
        bPlusDestroy(rChild);
        return E_DISKFULL;
    }

    // Updating root block header to reflect 1 entry
    HeadInfo rootHeader;
    newRootBlk.getHeader(&rootHeader);
    rootHeader.numEntries = 1;
    newRootBlk.setHeader(&rootHeader);

    // Inserting the entry into root block
    InternalEntry intEntry;
    intEntry.attrVal = attrVal;
    intEntry.lChild = lChild;
    intEntry.rChild = rChild;
    newRootBlk.setEntry(&intEntry, 0);

    // Connecting left and right child blocks
    BlockBuffer leftBlock(lChild);
    BlockBuffer rightBlock(rChild);

    HeadInfo leftHeader;
    HeadInfo rightHeader;

    // Setting parent block in headers
    leftBlock.getHeader(&leftHeader);
    rightBlock.getHeader(&rightHeader);
    leftHeader.pblock = newRootBlkNum;
    rightHeader.pblock = newRootBlkNum;
    leftBlock.setHeader(&leftHeader);
    rightBlock.setHeader(&rightHeader);

    // Setting rootBlock for attrName
    attrCatEntry.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntry);

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS) {
        return E_OUTOFBOUND;
    }

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (type == IND_LEAF) 
    {
        IndLeaf indLeaf(rootBlockNum);
        indLeaf.releaseBlock();
        return SUCCESS;
    } 
    else if (type == IND_INTERNAL) 
    {
        IndInternal internalBlock(rootBlockNum);
        HeadInfo header;
        internalBlock.getHeader(&header);

        InternalEntry blockEntry;
        internalBlock.getEntry (&blockEntry, 0);
        BPlusTree::bPlusDestroy(blockEntry.lChild);

        for (int entry = 0; entry < header.numEntries; entry++) {
            internalBlock.getEntry (&blockEntry, entry);
            BPlusTree::bPlusDestroy(blockEntry.rChild);
        }
        internalBlock.releaseBlock();
        return SUCCESS;
    } 
    else
        return E_INVALIDBLOCK;

}