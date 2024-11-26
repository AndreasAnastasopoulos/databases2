#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "bp_file.h"
#include "record.h"
#include <bp_datanode.h>

int create_data_node(int file_desc, BPLUS_DATA_NODE* data_node) {
    // Allocate a new block for the data node
    BF_Block *block;
    BF_Block_Init(&block);
    
    if (BF_AllocateBlock(file_desc, block) != BF_OK) {
        BF_PrintError(BF_ERROR);
        BF_Block_Destroy(&block);
        return -1;
    }

    // Initialize data node structure
    data_node->num_records = 0;
    data_node->block_id = BF_GetBlockCounter(file_desc, NULL) - 1;
    data_node->next_data_block = -1;  // No next block initially

    // Zero out records array
    memset(data_node->records, 0, sizeof(data_node->records));

    // Copy data node to block data
    char *data = BF_Block_GetData(block);
    memcpy(data, data_node, sizeof(BPLUS_DATA_NODE));
    BF_Block_SetDirty(block);

    // Unpin the block
    if (BF_UnpinBlock(block) != BF_OK) {
        BF_PrintError(BF_ERROR);
        BF_Block_Destroy(&block);
        return -1;
    }

    BF_Block_Destroy(&block);
    return data_node->block_id;
}

int insert_record_to_data_node(BPLUS_DATA_NODE* data_node, Record record) {
    // Check if there's space in the node
    if (data_node->num_records >= MAX_RECORDS_PER_BLOCK) {
        fprintf(stderr, "Data node is full\n");
        return -1;
    }

    // Find the correct position to insert the record (sorted by ID)
    int i = data_node->num_records;
    while (i > 0 && record.id < data_node->records[i-1].id) {
        data_node->records[i] = data_node->records[i-1];
        i--;
    }

    // Insert the new record
    data_node->records[i] = record;
    data_node->num_records++;

    return 0;
}

int find_record_in_data_node(BPLUS_DATA_NODE* data_node, int key) {
    // Binary search for the record with the given key
    int left = 0;
    int right = data_node->num_records - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (data_node->records[mid].id == key) {
            return mid;  // Record found
        }

        if (data_node->records[mid].id < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    // Record not found
    return -1;
}

int split_data_node(BPLUS_DATA_NODE* src_node, BPLUS_DATA_NODE* dest_node) {
    // Determine the split point (middle of the records)
    int split_point = (src_node->num_records + 1) / 2;

    // Initialize destination node
    dest_node->num_records = src_node->num_records - split_point;
    dest_node->next_data_block = src_node->next_data_block;

    // Copy records to the destination node
    memcpy(dest_node->records, 
           &src_node->records[split_point], 
           dest_node->num_records * sizeof(Record));
    
    // Reduce source node's record count
    src_node->num_records = split_point;

    // Link the nodes
    dest_node->next_data_block = src_node->next_data_block;
    src_node->next_data_block = dest_node->block_id;

    // Return the first key of the new node (for index node insertion)
    return dest_node->records[0].id;
}