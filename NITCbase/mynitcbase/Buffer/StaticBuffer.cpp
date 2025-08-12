#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
        metainfo[bufferIndex].free = true;
}

// No writing back from the buffer now since no modifications to buffer
// In later stages write-back will be implemented
StaticBuffer::~StaticBuffer() {}

int StaticBuffer::getFreeBuffer(int blockNum) {
    if (blockNum < 0 || blockNum > DISK_BLOCKS) 
        return E_OUTOFBOUND;

    int allocatedBuffer;
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
    {
        if (metainfo[bufferIndex].free == 1) {
            allocatedBuffer = bufferIndex;
            break;
        }
    }
    metainfo[allocatedBuffer].free = false;
    metainfo[allocatedBuffer].blockNum = blockNum;

    return allocatedBuffer;
}

int StaticBuffer::getBufferNum(int blockNum) {
    if (blockNum < 0 || blockNum > DISK_BLOCKS)
        return E_OUTOFBOUND;
    
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++)
    {
        if (metainfo[bufferIndex].blockNum == blockNum)
            return bufferIndex;
    }

    return E_BLOCKNOTINBUFFER;
}