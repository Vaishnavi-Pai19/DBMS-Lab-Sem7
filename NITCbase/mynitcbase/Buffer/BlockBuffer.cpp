#include "BlockBuffer.h"

#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

/* -------------------------Constructor definitions for classes that inherit from BlockBuffer--------------------------------- */
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}
RecBuffer::RecBuffer() : BlockBuffer('R'){}

IndBuffer::IndBuffer(int blockNum) : BlockBuffer(blockNum){}   // Create an IndBuffer object for an existing index block
IndBuffer::IndBuffer(char blockType) : BlockBuffer(blockType){}  // Create a new index block of a specific type

IndLeaf::IndLeaf() : IndBuffer('L'){}
IndLeaf::IndLeaf(int blockNum) : IndBuffer(blockNum){}

IndInternal::IndInternal() : IndBuffer('I'){}
IndInternal::IndInternal(int blockNum) : IndBuffer(blockNum){}

int BlockBuffer::getBlockNum(){
    return this->blockNum;
}

BlockBuffer::BlockBuffer(char blockType) {
   int blockTypeInt;
    if(blockType=='R'){
        blockTypeInt = REC;
    }
    else if(blockType=='I'){
        blockTypeInt = IND_INTERNAL;
    }
    else if(blockType=='L'){
        blockTypeInt = IND_LEAF;
    }
    else{
        blockTypeInt = UNUSED_BLK;
    }
    int blockNum = getFreeBlock(blockTypeInt);
    this->blockNum = blockNum;

    if (blockNum < 0 || blockNum >= DISK_BLOCKS)
        return;
}

int BlockBuffer::getFreeBlock(int blockType){
    int block = -1;
    for (int i = 0; i < DISK_BLOCKS; i++)
    {
        if (StaticBuffer::blockAllocMap[i] == UNUSED_BLK)
        {
            block = i;
            break;
        }
    }

    if (block == -1)
        return E_DISKFULL;
    
    this->blockNum = block;
    int bufferNum = StaticBuffer::getFreeBuffer(block);
    struct HeadInfo head;
    head.pblock = -1;
    head.lblock = -1;
    head.rblock = -1;
    head.numEntries = 0;
    head.numAttrs = 0;
    head.numSlots = 0;

    int ret = setHeader(&head);
    if (ret != SUCCESS)
        return ret;
    
    ret = setBlockType(blockType);
    if (ret != SUCCESS)
        return ret;

    return block;
}

int BlockBuffer::getHeader(struct HeadInfo *head) {
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
        return ret;

    memcpy(&head->reserved, bufferPtr + 28, 4);
    memcpy(&head->numSlots, bufferPtr + 24, 4);
    memcpy(&head->numAttrs, bufferPtr + 20, 4);
    memcpy(&head->numEntries, bufferPtr + 16, 4);
    memcpy(&head->rblock, bufferPtr + 12, 4);
    memcpy(&head->lblock, bufferPtr + 8, 4);
    memcpy(&head->pblock, bufferPtr + 4, 4);
    memcpy(&head->blockType, bufferPtr, 4);

    return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo *head){
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
        return ret;

    // Casting bufferPtr to type HeadInfo*
    struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

    bufferHeader->blockType = head->blockType;
    bufferHeader->pblock = head->pblock;
    bufferHeader->lblock = head->lblock;
    bufferHeader->rblock = head->rblock;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numSlots = head->numSlots;

    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}

int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    struct HeadInfo head;
    this->getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
        return ret;

    int recordSize = attrCount * ATTR_SIZE;
    unsigned char *slotPointer = bufferPtr + 32 + slotCount + recordSize*slotNum;

    memcpy(rec, slotPointer, recordSize);
    return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
        return ret;

    struct HeadInfo head;
    this->getHeader(&head);

    int numAttrs = head.numAttrs;
    int numSlots = head.numSlots;
    
    if (slotNum < 0 || slotNum >= numSlots)
    {
        return E_OUTOFBOUND;
    }

    int recordSize = numAttrs*ATTR_SIZE;
    unsigned char* recordPtr = bufferPtr + HEADER_SIZE + numSlots + slotNum*recordSize;

    memcpy(recordPtr, rec, recordSize);
    StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}

int RecBuffer::getSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;

    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    struct HeadInfo head;
    this->getHeader(&head);

    int slotCount = head.numSlots;
    unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

    memcpy(slotMap, slotMapInBuffer, slotCount);
    return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char *slotMap) {
    unsigned char *bufferPtr;

    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;
    }

    struct HeadInfo head;
    this->getHeader(&head);

    int slotCount = head.numSlots;
    unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

    memcpy(slotMapInBuffer, slotMap, slotCount);
    return StaticBuffer::setDirtyBit(this->blockNum);
}

int BlockBuffer::setBlockType(int blockType){
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
        return ret;

    *((int32_t *)bufferPtr) = blockType;
    StaticBuffer::blockAllocMap[this->blockNum] = blockType;

    ret = StaticBuffer::setDirtyBit(this->blockNum);
    if (ret != SUCCESS)
        return ret;

    return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if (bufferNum != E_BLOCKNOTINBUFFER)            // If block present in buffer, updating timestamp for LRU
    {
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;

        for (int i = 0; i < BUFFER_CAPACITY && i != bufferNum; i++)
        {
            if (StaticBuffer::metainfo[i].free == 0)
                StaticBuffer::metainfo[i].timeStamp += 1;
        }  
    } 
    else                                          // If block not present in buffer
    {                         
        bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

        if (bufferNum == E_OUTOFBOUND)
            return E_OUTOFBOUND;

        Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
    }

    *buffPtr = StaticBuffer::blocks[bufferNum];
    return SUCCESS;
}

void BlockBuffer::releaseBlock() {
    if (blockNum < 0 || blockNum >= DISK_BLOCKS || StaticBuffer::blockAllocMap[blockNum] == UNUSED_BLK)
        return;

    int bufferNum = StaticBuffer::getBufferNum(blockNum);
    if (bufferNum == E_BLOCKNOTINBUFFER)
        return;
    
    StaticBuffer::metainfo[bufferNum].free = true;

    StaticBuffer::blockAllocMap[this->blockNum] = UNUSED_BLK;

    this->blockNum = INVALID_BLOCKNUM; 
}

int IndInternal::getEntry(void *ptr, int indexNum) {
    if (indexNum < 0 || indexNum >= MAX_KEYS_INTERNAL)
        return E_OUTOFBOUND;

    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    // Typecasting the void pointer to an internal entry pointer
    struct InternalEntry *internalEntry = (struct InternalEntry *)ptr;

    // Copying the entries from the indexNum`th entry to *internalEntry
    // int32_t = type of int that is guaranteed to be 4 bytes across every C++ implementation
    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * 20);

    // 3-tuple <left pointer, attrVal, right pointer> = 24 bytes (40 on one row)
    memcpy(&(internalEntry->lChild), entryPtr, sizeof(int32_t));
    memcpy(&(internalEntry->attrVal), entryPtr + 4, sizeof(Attribute));
    memcpy(&(internalEntry->rChild), entryPtr + 20, 4);

    return SUCCESS;
}

int IndLeaf::getEntry(void *ptr, int indexNum) {
if (indexNum < 0 || indexNum >= MAX_KEYS_LEAF)
        return E_OUTOFBOUND;

    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);

    if (ret != SUCCESS)
        return ret;

    unsigned char *entryPtr = bufferPtr + HEADER_SIZE + (indexNum * LEAF_ENTRY_SIZE);
    struct Index *index = (struct Index *)ptr;

    // 3-tuple <attrVal, block, slot> = 24 bytes + 8 unused
    memcpy(&(index->attrVal), entryPtr, sizeof(Attribute));
    memcpy(&(index->block), entryPtr + 16, 4);
    memcpy(&(index->slot), entryPtr + 20, 4);

    return SUCCESS;
}

int IndInternal::setEntry(void *ptr, int indexNum) {
  return 0;
}

int IndLeaf::setEntry(void *ptr, int indexNum) {
  return 0;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {

    double diff;
    if (attrType == STRING)
        diff = strcmp(attr1.sVal, attr2.sVal);
    else
        diff = attr1.nVal - attr2.nVal;

    if (diff > 0)
        return 1;
    else if (diff < 0)
        return -1;
    else
        return 0;
}
