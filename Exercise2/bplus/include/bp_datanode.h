#ifndef BP_DATANODE_H
#define BP_DATANODE_H
#include <record.h>
#include <record.h>
#include <bf.h>
#include <bp_file.h>
#include <bp_indexnode.h>

#define MAX_RECORDS_PER_BLOCK (TREE_ORDER - 1)

typedef struct
{
    int num_records;                       // Number of records in this data node
    int block_id;                          // Block ID of this data node
    int next_data_block;                   // ID of the next data block (for leaf node chaining)
    Record records[MAX_RECORDS_PER_BLOCK]; // Array of records
} BPLUS_DATA_NODE;

// Function prototypes for data node operations
int create_data_node(int file_desc, BPLUS_DATA_NODE* data_node);
int insert_record_to_data_node(BPLUS_DATA_NODE* data_node, Record record);
int find_record_in_data_node(BPLUS_DATA_NODE* data_node, int key);
int split_data_node(BPLUS_DATA_NODE* src_node, BPLUS_DATA_NODE* dest_node);

#endif