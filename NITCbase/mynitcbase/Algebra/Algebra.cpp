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
    int srcRelId = OpenRelTable::getRelId(srcRel);      // Implemented later
    if (srcRelId == E_RELNOTOPEN) {
        return E_RELNOTOPEN;
    }

    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
    if (ret == E_ATTRNOTEXIST)
        return ret;

    // Converting strVal (string) to an attribute of data type NUMBER or STRING ***/
    int type = attrCatEntry.attrType;
    Attribute attrVal;
    if (type == NUMBER) {
        if (isNumber(strVal)) {       
            attrVal.nVal = atof(strVal);
        } else {
            return E_ATTRTYPEMISMATCH;
        }
    } else if (type == STRING) {
        strcpy(attrVal.sVal, strVal);
    }

    /*** Selecting records from the source relation ***/
    // Resetting search index before calling search
    RelCacheTable::resetSearchIndex(srcRelId);
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);

    /************************
     The following code prints the contents of a relation directly to the output
    console. Direct console output is not permitted by the actual the NITCbase
    specification and the output can only be inserted into a new relation. We will
    be modifying it in the later stages to match the specification.
    ************************/
    // Printing the attribute column names first
    printf("|");
    for (int i = 0; i < relCatEntry.numAttrs; ++i) {
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
    
        printf(" %s |", attrCatEntry.attrName);
    }
    printf("\n");

    // Printing the values from actual records
    while (true) {
        RecId searchRes = BlockAccess::linearSearch(srcRelId, attr, attrVal, op);

        if (searchRes.block != -1 && searchRes.slot != -1) {
            RecBuffer recBuffer(searchRes.block);
            int recNumAttrs = relCatEntry.numAttrs;
            Attribute record[recNumAttrs];

            recBuffer.getRecord(record, searchRes.slot);

            printf("|");
            for (int i = 0; i < recNumAttrs; ++i) {
                AttrCatEntry attrCatEntry;
                AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);

                if (attrCatEntry.attrType == NUMBER)
                    printf (" %d |", (int)record[i].nVal);
                else
                    printf (" %s |", record[i].sVal);
            }
            printf("\n");
        } else 
            break;
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