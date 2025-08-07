#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"
#include <cstring>
#include <iostream>

void printRelations () {
  RecBuffer relCatBuffer(RELCAT_BLOCK);
  HeadInfo relCatHeader;

  relCatBuffer.getHeader(&relCatHeader);
  int relCount = relCatHeader.numEntries;

  for (int i = 0; i < relCount; i++)
  {
    Attribute relCatRecord[RELCAT_NO_ATTRS];    // Array of type Attribute to store a record
    relCatBuffer.getRecord(relCatRecord, i);
    printf("\nRelation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);

    int attrCatBlockNum = ATTRCAT_BLOCK;  // 5 initially

    while (attrCatBlockNum != -1)
    {
      RecBuffer attrCatBuffer(attrCatBlockNum);
      HeadInfo attrCatHeader;

      attrCatBuffer.getHeader(&attrCatHeader);
      int attrCount = attrCatHeader.numEntries;

      int currentBlockNum = attrCatBlockNum;
      attrCatBlockNum = attrCatHeader.rblock;   // If it is not -1, indicates more blocks in a linked list for Attribute Catalog

      for (int j = 0; j < attrCount; j++)
      {
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, j);

        if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0)
        {
          const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER 
            ? "NUM"
            : "STR";

          printf(" %s: %s         (Block Number: %d, Slot Number: %d)\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType, currentBlockNum, j);
        }
      }  
    }  
  }
}

void exercise2 () {
  char targetRelation[] = "Students";
  char targetAttribute[] = "Class";
  int attrCatBlockNum = ATTRCAT_BLOCK;   // 5 initially

  while (attrCatBlockNum != -1)
  {
    RecBuffer attrCatBuffer(attrCatBlockNum);
    HeadInfo attrCatHeader;
  
    attrCatBuffer.getHeader(&attrCatHeader);
    int attrCount = attrCatHeader.numEntries;

    int currentBlockNum = attrCatBlockNum;     // Needed in case the attribute to be changed is not in the first Attribute Catalog block
    attrCatBlockNum = attrCatHeader.rblock;    // If multiple blocks of attribute catalog, this won't be -1

    for (int i = 0; i < attrCount; i++ )
    {
      Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
      attrCatBuffer.getRecord(attrCatRecord, i);
      if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, targetRelation) == 0)
      {
        if (strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, targetAttribute) == 0)
        {
          unsigned char buffer[BLOCK_SIZE];
          Disk::readBlock(buffer, currentBlockNum);
          int offset = 32 + 20 + 96*i + 16;          // HEADER_SIZE + slotMapSize + (recordSize * slotNum) + 16 for AttributeName

          memcpy(buffer + offset, "Batch", 6);
          Disk::writeBlock(buffer, currentBlockNum);

          printRelations();
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  Disk disk_run;
  printRelations();
  // exercise2();
  return 0;
}