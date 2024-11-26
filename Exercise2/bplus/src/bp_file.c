#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "bp_file.h"
#include "record.h"
#include <bp_datanode.h>
#include <bp_indexnode.h>
#include <stdbool.h>

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return bplus_ERROR;     \
    }                         \
  }

int BP_CreateFile(char *fileName)
{

  int error = BF_CreateFile(fileName);

  // Εαν πχ το αρχείο υπάρχει ήδη
  if (error != BF_OK)
  {
    BF_PrintError(error);
    return -1;
  }

  else
  {
    int file_desc;
    int error = BF_OpenFile(fileName, &file_desc);

    if (error != BF_OK)
    {
      BF_PrintError(error);
      return -1;
    }

    // Δημιουργία δεικτη σε block
    // το block δεν ειναι τιποτα αλλο παρα ενας δεικτης στα δεδομενα ενος μπλοκ
    // που τον χρησιμοποιουμε για να εχουμε προσβαση σε αυτα
    // οταν δεν τον χρειαζομαστε αλλο πρεπει να τον καταστρεψουμε
    BF_Block *block;
    BF_Block_Init(&block);
    error = BF_AllocateBlock(file_desc, block);
    if (error != BF_OK)
    {
      BF_PrintError(error);
      BF_Block_Destroy(&block);
      return -1;
    }

    // Αρχικοποιούμε τα στοιχεία του Β+ Δέντρου
    BPLUS_INFO bplus_info = {
        .file_desc = file_desc,
        .root_block = 1, // Το πρώτο block δεδομένων είναι η ρίζα
        .height = 1,
        .order = TREE_ORDER,
        .record_size = sizeof(Record),
        .key_size = sizeof(int),
        .first_data_block = 1,
        .is_open = true,
        .magic_number = MAGIC_NUMBER,
        .max_records_per_block = MAX_RECORDS_PER_BLOCK};

    // Αντιγραφή των μεταδεδομένων στο block 0
    char *data = BF_Block_GetData(block);
    memcpy(data, &bplus_info, sizeof(BPLUS_INFO));

    BF_Block_SetDirty(block); // αφου τροποποιηθηκε το block, το κανουμε dirty
    BF_UnpinBlock(block);     // δεν το χρειαζομαστε πλεον
    BF_Block_Destroy(&block); // καταστροφη του δεικτη
    BF_CloseFile(file_desc);  // κλεισιμο του αρχειου
    return 0;
  }
}

BPLUS_INFO *BP_OpenFile(char *fileName, int *file_desc)
{

  int error = BF_OpenFile(fileName, file_desc);
  if (error != BF_OK)
  {
    BF_PrintError(error);
    return NULL;
  }

  // Παίρνουμε το 1ο block με τα μεταδεδομένα
  BF_Block *block;
  BF_Block_Init(&block);

  error = BF_GetBlock(*file_desc, 0, block);
  if (error != BF_OK)
  {
    BF_PrintError(error);
    BF_UnpinBlock(block);
    return NULL;
  }

  char *data = BF_Block_GetData(block);

  // αποθηκευση των μεταδεδομενων στη δομη BPLUS_INFO
  BPLUS_INFO *bplus_info = (BPLUS_INFO *)data;

  BF_UnpinBlock(block); // μονο unpin χωρις set_dirty αφου δεν αλλαξαμε κατι
  BF_Block_Destroy(&block);
  return bplus_info;
}

int BP_CloseFile(int file_desc, BPLUS_INFO *info)
{
  int error;
  BF_Block *block;
  // δεν χρειαζεται να κανουμε unpin τσ blocks γιατί η κάθε λειτουργία πχ insert θα κάνει unpin οταν τελειώσει
  //(στις υλοποιήσεις των δικών μας συναρτήσεων κάνουμε εμείς το unpin)
  BF_Block_Init(&block);
  BF_Block_Destroy(&block);

  error = BF_CloseFile(file_desc);
  if (error != BF_OK)
  {
    BF_PrintError(error);
    return -1;
  }

  return 0;
}

int BP_InsertEntry(int file_desc, BPLUS_INFO *bplus_info, Record record)
{
  BF_Block *block;
  BF_Block_Init(&block);

  // Get the first data block (root for now)
  int error = BF_GetBlock(file_desc, bplus_info->root_block, block);
  if (error != BF_OK)
  {
    BF_PrintError(error);
    BF_Block_Destroy(&block);
    return -1;
  }

  // Convert block to data node
  BPLUS_DATA_NODE *data_node = (BPLUS_DATA_NODE *)BF_Block_GetData(block);

  // If the node is full, handle splitting
  if (data_node->num_records >= MAX_RECORDS_PER_BLOCK)
  {
    // Implement node splitting logic
    // This is a simplified version and needs more complex implementation
    return -1;
  }

  // Insert record into the data node
  if (insert_record_to_data_node(data_node, record) != 0)
  {
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
    return -1;
  }

  BF_Block_SetDirty(block);
  BF_UnpinBlock(block);

  BF_Block_Destroy(&block);
  return bplus_info->root_block;
}

int BP_GetEntry(int file_desc, BPLUS_INFO *header_info, int id, Record **result)
{
  // Παίρνουμε το μπλοκ που βρίσκεται στη ρίζα.
  BF_Block *block;
  BF_Block_Init(&block);

  int error = BF_GetBlock(file_desc, header_info->root_block, block);
  if (error != BF_OK)
  {
    BF_PrintError(error);
    BF_Block_Destroy(&block);
    return -1;
  }

  // Convert block to data node
  BPLUS_DATA_NODE *data_node = (BPLUS_DATA_NODE *)BF_Block_GetData(block);

  // Find record in this node
  int record_index = find_record_in_data_node(data_node, id);
  if (record_index != -1)
  {
    *result = &(data_node->records[record_index]);
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
    return 0;
  }

  // If record not found
  *result = NULL;
  BF_UnpinBlock(block);
  BF_Block_Destroy(&block);
  return -1;
}