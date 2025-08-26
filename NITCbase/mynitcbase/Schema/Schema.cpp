#include "Schema.h"
#include <cmath>
#include <cstring>

int Schema::openRel(char relName[ATTR_SIZE]) {
    int ret = OpenRelTable::openRel(relName);

    if(ret >= 0){
        return SUCCESS;
    }

    return ret;
}

int Schema::closeRel(char relName[ATTR_SIZE]) {
    if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return E_NOTPERMITTED;
    }

    int relId = OpenRelTable::getRelId(relName); 
    if (relId == E_RELNOTOPEN) {                    // E_RELNOTOPEN = -89
        return E_RELNOTOPEN;
    }

    return OpenRelTable::closeRel(relId);
}

int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
    if (!strcmp(oldRelName, (char*)RELCAT_RELNAME) || !strcmp(oldRelName, (char*)ATTRCAT_RELNAME) ||
            !strcmp(newRelName, (char*)RELCAT_RELNAME) || !strcmp(newRelName, (char*)ATTRCAT_RELNAME))
            return E_NOTPERMITTED;
    
    int retVal = OpenRelTable::getRelId(oldRelName);
    if (retVal != E_RELNOTOPEN)
        return E_RELOPEN;

    retVal = BlockAccess::renameRelation(oldRelName, newRelName);
    return retVal;
}

int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) {
    if (!strcmp(relName, (char*)RELCAT_RELNAME) || !strcmp(relName, (char*)ATTRCAT_RELNAME))
            return E_NOTPERMITTED;

    int retVal = OpenRelTable::getRelId(relName);
    if (retVal != E_RELNOTOPEN)
        return E_RELOPEN;

    retVal = BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);
    return retVal;
}