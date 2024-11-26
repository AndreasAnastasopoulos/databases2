#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "bp_file.h"
#include "record.h"
#include <bp_indexnode.h>

int create_index_node(int file_desc, BPLUS_INDEX_NODE *index_node)
{
    // Allocate a new block for the index node
    BF_Block *block;
    BF_Block_Init(&block);

    if (BF_AllocateBlock(file_desc, block) != BF_OK)
    {
        BF_PrintError(BF_ERROR);
        BF_Block_Destroy(&block);
        return -1;
    }

    // Initialize index node structure
    index_node->num_keys = 0;
    index_node->block_id = BF_GetBlockCounter(file_desc, NULL) - 1;
    index_node->parent_block_id = -1; // No parent initially

    // Zero out keys and children arrays
    memset(index_node->keys, 0, sizeof(index_node->keys));
    memset(index_node->children, 0, sizeof(index_node->children));

    // Copy index node to block data
    char *data = BF_Block_GetData(block);
    memcpy(data, index_node, sizeof(BPLUS_INDEX_NODE));
    BF_Block_SetDirty(block);

    // Unpin the block
    if (BF_UnpinBlock(block) != BF_OK)
    {
        BF_PrintError(BF_ERROR);
        BF_Block_Destroy(&block);
        return -1;
    }

    BF_Block_Destroy(&block);
    return index_node->block_id;
}

int insert_key_to_index_node(BPLUS_INDEX_NODE *index_node, int key, int child_block_id)
{
    // Check if there's space in the node
    if (index_node->num_keys >= MAX_CHILDREN_PER_NODE - 1)
    {
        fprintf(stderr, "Index node is full\n");
        return -1;
    }

    // Find the correct position to insert the key
    int i = index_node->num_keys;
    while (i > 0 && key < index_node->keys[i - 1])
    {
        index_node->keys[i] = index_node->keys[i - 1];
        index_node->children[i + 1] = index_node->children[i];
        i--;
    }

    // Insert the new key and child
    index_node->keys[i] = key;
    index_node->children[i + 1] = child_block_id;
    index_node->num_keys++;

    return 0;
}

int find_child_index(BPLUS_INDEX_NODE *index_node, int key)
{
    // Find the appropriate child index for a given key
    for (int i = 0; i < index_node->num_keys; i++)
    {
        if (key < index_node->keys[i])
        {
            return i;
        }
    }
    return index_node->num_keys;
}

int split_index_node(BPLUS_INDEX_NODE *src_node, BPLUS_INDEX_NODE *dest_node)
{
    // Determine the middle point for splitting
    int split_point = (src_node->num_keys + 1) / 2;

    // Initialize destination node
    dest_node->num_keys = src_node->num_keys - split_point;
    dest_node->parent_block_id = src_node->parent_block_id;

    // Copy keys to the destination node
    memcpy(dest_node->keys, &src_node->keys[split_point],
           (dest_node->num_keys) * sizeof(int));

    // Copy children to the destination node
    memcpy(dest_node->children, &src_node->children[split_point],
           (dest_node->num_keys + 1) * sizeof(int));

    // Reduce source node's key count
    src_node->num_keys = split_point - 1;

    return src_node->keys[split_point - 1]; // Return the promotion key
}