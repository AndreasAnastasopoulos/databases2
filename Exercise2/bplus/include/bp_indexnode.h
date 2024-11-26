#ifndef BP_INDEX_NODE_H
#define BP_INDEX_NODE_H
#include <record.h>
#include <bf.h>
#include <bp_file.h>

#define MAX_CHILDREN_PER_NODE TREE_ORDER

typedef struct
{
    int num_keys;                        // Number of keys in this index node
    int block_id;                        // Block ID of this index node
    int parent_block_id;                 // Block ID of the parent node
    int keys[MAX_CHILDREN_PER_NODE - 1]; // Array of keys for navigation
    int children[MAX_CHILDREN_PER_NODE]; // Array of child node block IDs
} BPLUS_INDEX_NODE;

// Function prototypes for index node operations
int create_index_node(int file_desc, BPLUS_INDEX_NODE* index_node);
int insert_key_to_index_node(BPLUS_INDEX_NODE* index_node, int key, int child_block_id);
int find_child_index(BPLUS_INDEX_NODE* index_node, int key);
int split_index_node(BPLUS_INDEX_NODE* src_node, BPLUS_INDEX_NODE* dest_node);

#endif