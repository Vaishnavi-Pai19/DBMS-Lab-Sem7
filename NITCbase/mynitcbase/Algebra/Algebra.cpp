#include "Algebra.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// Will return if a string can be parsed as a floating point number
bool isNumber(char *str) {
    int len;
    float ignore;
    /*
        sscanf returns the number of elements read, so if there is no float matching
        the first %f, ret will be 0, else it'll be 1

        %n gets the number of characters read. this scanf sequence will read the
        first float ignoring all the whitespace before and after. and the number of
        characters read that far will be stored in len. if len == strlen(str), then
        the string only contains a float with/without whitespace. else, there's other
        characters.
    */
    int ret = sscanf(str, "%f %n", &ignore, &len);
    return ret == 1 && len == strlen(str);
}

/*
- srcRel - the source relation we want to select from
- targetRel - the relation we want to select into (ignored for now)
- attr - the attribute that the condition is checking
- op - the operator of the condition
- strVal - the value that we want to compare against (represented as a string)
*/
// SELECT * FROM srcRel INTO targetVal WHERE attr op strVal;
int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]) {
    int srcRelId = OpenRelTable::getRelId(srcRel);      
    if (srcRelId == E_RELNOTOPEN) 
        return E_RELNOTOPEN;

    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
    if (ret != SUCCESS)
    {
        return E_ATTRNOTEXIST;
    }

    // Converting strVal (string) to an attribute of data type NUMBER or STRING ***/
    int type = attrCatEntry.attrType;
    Attribute attrVal;

    if (type == NUMBER) 
    {
        if (isNumber(strVal)) 
        {       
            attrVal.nVal = atof(strVal);
        } 
        else 
        {
            return E_ATTRTYPEMISMATCH;
        }
    } 
    else if (type == STRING) 
    {
        strcpy(attrVal.sVal, strVal);
    }

    /************* Creating and opening the target relation *************/
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
    int numAttrs = relCatEntry.numAttrs;
    
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    for (int i = 0; i<numAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);

        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
    if (ret != SUCCESS)
        return ret;

    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }
    
    /************ Selecting and inserting records into the target relation ************/
    RelCacheTable::resetSearchIndex(targetRelId);
    Attribute record[numAttrs];

    // Resetting for both search indexes, since BA::search can do either
    RelCacheTable::resetSearchIndex(srcRelId);
    AttrCacheTable::resetSearchIndex(srcRelId, attr);

    ret = BlockAccess::search(srcRelId, record, attr, attrVal, op);
    while (ret==SUCCESS) 
    {
        ret = BlockAccess::insert(targetRelId, record);
        if (ret != SUCCESS)
        {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
        ret = BlockAccess::search(srcRelId, record, attr, attrVal, op);

    }
    
    Schema::closeRel(targetRel);
    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]) {
    int srcRelId = OpenRelTable::getRelId(srcRel);      
    if (srcRelId == E_RELNOTOPEN) 
    {
        return E_RELNOTOPEN;
    }

    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
    
    int numAttrs = srcRelCatEntry.numAttrs;
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];

    for (int i = 0; i<numAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }

    /********* Creating and opening the target relation *********/
    int ret = Schema::createRel(targetRel, numAttrs, attrNames, attrTypes);
    if (ret != SUCCESS)
        return ret;

    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    RelCacheTable::resetSearchIndex(srcRelId);

    /*** Inserting projected records into the target relation ***/
    Attribute record[numAttrs];

    ret = BlockAccess::project(srcRelId, record);
    while (ret == SUCCESS)
    {
        ret = BlockAccess::insert(targetRelId, record);
        if (ret != SUCCESS) {
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
        ret = BlockAccess::project(srcRelId, record);
    }

    Schema::closeRel(targetRel);
    return SUCCESS;
}

int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]) {
    int srcRelId = OpenRelTable::getRelId(srcRel);      
    if (srcRelId == E_RELNOTOPEN) 
    {
        return E_RELNOTOPEN;
    }

    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
    
    int numAttrs = srcRelCatEntry.numAttrs;

    int attrOffset[tar_nAttrs];
    int attrTypes[tar_nAttrs];

    /*** Checking if attributes of target are present in the source relation
         and storing its offsets and types ***/
    for (int i = 0; i < tar_nAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        int ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
        if (ret != SUCCESS)
        {
            return E_ATTRNOTEXIST;
        }

        attrOffset[i] = attrCatEntry.offset;
        attrTypes[i] = attrCatEntry.attrType;
    }


    /******* Creating and opening the target relation *******/
    int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attrTypes);
    if (ret != SUCCESS)
        return SUCCESS;

    int targetRelId = OpenRelTable::openRel(targetRel);
    if (targetRelId < 0)
    {
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/
    RelCacheTable::resetSearchIndex(srcRelId);

    Attribute record[numAttrs];
    ret = BlockAccess::project(srcRelId, record);

    while (ret == SUCCESS) 
    {
        Attribute proj_record[tar_nAttrs];
        for (int i = 0; i < tar_nAttrs; i++)
        {
            proj_record[i] = record[attrOffset[i]];
        }
        ret = BlockAccess::insert(targetRelId, proj_record);

        if (ret != SUCCESS) 
        {
            Schema::closeRel(targetRel);
            ret = Schema::deleteRel(targetRel);
            return ret;
        }
        ret = BlockAccess::project(srcRelId, record);
    }

    ret = Schema::closeRel(targetRel);
    if (ret != SUCCESS)
    {
        printf("Invalid Relation ID.\n");
        exit(1);
    }

    return SUCCESS;
}

int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]) {
    if (strcmp(relName, (char*)RELCAT_RELNAME) == 0 || strcmp(relName, (char*)ATTRCAT_RELNAME) == 0)
        return E_NOTPERMITTED;

    int relId = OpenRelTable::getRelId(relName);
    if (relId == E_RELNOTOPEN)
        return relId;
    
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    if (relCatEntry.numAttrs != nAttrs)
        return E_NATTRMISMATCH;

    Attribute recordValues[nAttrs];
    
    // Converting 2D char array of record values to Attribute array recordValues
    for (int i = 0; i < nAttrs; i++)
    {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
        int type = attrCatEntry.attrType;

        if (type == NUMBER)
        {
            if (isNumber(record[i]))
                recordValues[i].nVal = atof(record[i]);
            else
                return E_ATTRTYPEMISMATCH;
        }
        else if (type == STRING)
        {
            strcpy(recordValues[i].sVal, record[i]);
        }

    }

    int retVal = BlockAccess::insert(relId, recordValues);
    return retVal;
}