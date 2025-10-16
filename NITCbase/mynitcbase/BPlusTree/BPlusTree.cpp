#include "BPlusTree.h"

#include <cstring>
#include <cstdio>

int BPlusTree::numComparisons;

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
                BPlusTree::numComparisons++;

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
            BPlusTree::numComparisons++;

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