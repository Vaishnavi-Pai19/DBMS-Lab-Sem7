#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

StaticBuffer::StaticBuffer() {
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
    {
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }
}
        
// Writing back all modified blocks on system exit
StaticBuffer::~StaticBuffer() {
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
    {
        if (metainfo[bufferIndex].free == false && metainfo[bufferIndex].dirty == true)
            Disk::writeBlock(StaticBuffer::blocks[bufferIndex], metainfo[bufferIndex].blockNum);
    }
}

int StaticBuffer::getFreeBuffer(int blockNum) {
    if (blockNum < 0 || blockNum > DISK_BLOCKS) 
        return E_OUTOFBOUND;

    for (int i = 0; i < BUFFER_CAPACITY; i++)
    {
        if (metainfo[i].free == 0)
            metainfo[i].timeStamp++;
    }

    int bufferNum;
    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
    {
        if (metainfo[bufferIndex].free == 1) {
            bufferNum = bufferIndex;
            break;
        }
    }
    
    if (bufferNum == BUFFER_CAPACITY)
    {
        int maxTimeIndex = -1;
        for (int i = 0; i < BUFFER_CAPACITY; i++)
        {
            if (metainfo[i].timeStamp > maxTimeIndex)
                maxTimeIndex = i;
        }

        if (metainfo[maxTimeIndex].dirty == true)
            Disk::writeBlock(blocks[maxTimeIndex], metainfo[maxTimeIndex].blockNum);
        bufferNum = maxTimeIndex;
    }
    
    metainfo[bufferNum].free = false;
    metainfo[bufferNum].dirty = false;
    metainfo[bufferNum].blockNum = blockNum;
    metainfo[bufferNum].timeStamp = 0;

    return bufferNum;
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

int StaticBuffer::setDirtyBit(int blockNum){
    int bufferIndex = StaticBuffer::getBufferNum(blockNum);
    
    if (bufferIndex == E_BLOCKNOTINBUFFER)
        return E_BLOCKNOTINBUFFER;

    if (bufferIndex == E_OUTOFBOUND)
        return E_OUTOFBOUND;

    metainfo[bufferIndex].dirty = true;
    return SUCCESS;
}