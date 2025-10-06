#include "StaticBuffer.h"
#include <cstring>

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

StaticBuffer::StaticBuffer() {
    for (int bmap = 0; bmap < 4; bmap++) 
    {
        unsigned char buffer[BLOCK_SIZE];
        Disk::readBlock(buffer, bmap);
        memcpy(blockAllocMap + bmap*BLOCK_SIZE, buffer, BLOCK_SIZE);
    }

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
    {
        metainfo[bufferIndex].free = true;
        metainfo[bufferIndex].dirty = false;
        metainfo[bufferIndex].timeStamp = -1;
        metainfo[bufferIndex].blockNum = -1;
    }
}

StaticBuffer::~StaticBuffer() {
    for (int bmap = 0; bmap < 4; bmap++) 
    {
        unsigned char buffer[BLOCK_SIZE];
        memcpy(buffer, blockAllocMap + bmap*BLOCK_SIZE, BLOCK_SIZE);
        Disk::writeBlock(buffer, bmap);
    }

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
    {
        if (metainfo[bufferIndex].free == false && metainfo[bufferIndex].dirty == true)
            Disk::writeBlock(StaticBuffer::blocks[bufferIndex], metainfo[bufferIndex].blockNum);
    }
}

int StaticBuffer::getFreeBuffer(int blockNum) {
    if (blockNum < 0 || blockNum > DISK_BLOCKS) 
        return E_OUTOFBOUND;

    int bufferNum = -1;

    for (int i = 0; i < BUFFER_CAPACITY; i++)
    {
        if (metainfo[i].free == false)
            metainfo[i].timeStamp++;
    }

    for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) 
    {
        if (metainfo[bufferIndex].free == 1) {
            bufferNum = bufferIndex;
            break;
        }
    }

    if (bufferNum == -1)
    {
        int maxTimeStamp = -1;
        int bufferIndexWithMaxTimeStamp = -1;

        for (int bufferIndex = 0;bufferIndex< BUFFER_CAPACITY;bufferIndex++) 
        {
            if (metainfo[bufferIndex].timeStamp > maxTimeStamp)
            {
                maxTimeStamp = metainfo[bufferIndex].timeStamp;
                bufferIndexWithMaxTimeStamp = bufferIndex;
            }
        }
        if(metainfo[bufferIndexWithMaxTimeStamp].dirty == true)
            Disk::writeBlock(blocks[bufferIndexWithMaxTimeStamp],metainfo[bufferIndexWithMaxTimeStamp].blockNum);
        
        bufferNum = bufferIndexWithMaxTimeStamp;
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