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

          printf(" %s: %s         (Block Number: %d, Attribute Number: %d)\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType, currentBlockNum, j);
        }
      }  
    }  
  }
}

int main(int argc, char *argv[]) {
    Disk disk_run;
    StaticBuffer buffer; 
    OpenRelTable cache;
    
    for (int i = 0; i < 3; i++)
    {
        RelCatEntry relCatBuf;
		RelCacheTable::getRelCatEntry(i, &relCatBuf);

		printf("Relation: %s\n", relCatBuf.relName);
        
		for (int attr = 0; attr < relCatBuf.numAttrs; attr++) {
			AttrCatEntry attribute;
			AttrCacheTable::getAttrCatEntry(i, attr, &attribute);
            printf("  %s: %s\n", attribute.attrName, attribute.attrType == NUMBER ? "NUM":"STR");
        }
    }

    // printRelations();
    return 0;
}