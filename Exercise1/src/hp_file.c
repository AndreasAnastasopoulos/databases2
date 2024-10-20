#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    return HP_ERROR;        \
  }                         \
}

int HP_CreateFile(char *fileName){
    int error = BF_CreateFile(fileName);
    //εάν πχ το αρχείο υπάρχει ήδη
    if(error != BF_OK){
      BF_PrintError(error);
      return -1;
    }

    else{
      int file_desc;
      int error = BF_OpenFile(fileName, &file_desc);
      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }
      //Δημιουργία block
      BF_Block* block;
      BF_Block_Init(&block);
      BF_AllocateBlock(file_desc, block);

      //Δέσμευση block στο αρχείο
      
      void* data = BF_Block_GetData(block);
      HP_info hp_info;
      hp_info.last_block = 0;
      hp_info.number_of_blocks = 0;
      hp_info.first_block_with_records = NULL;

      //Υπολογισμός πόσες εγγραφές χωράει ένα block , αφαιρώντας το μέγεθος της δομής HP_block_info
      //το οποίο βρίσκεται στο τέλος ΚΑΘΕ block
      hp_info.records_per_block = (BF_BLOCK_SIZE - sizeof(HP_block_info) )/ sizeof(Record);

      memcpy(data, &hp_info , sizeof(hp_info));


      BF_Block_SetDirty(block);
      BF_UnpinBlock(block);
      BF_CloseFile(file_desc);

      //επιστρέφει BF_OK
      return error;
    }
}

HP_info* HP_OpenFile(char *fileName, int *file_desc){
    int error = BF_OpenFile(fileName, file_desc);
    if(error != BF_OK){
      BF_PrintError(error);
      return NULL;
    }

    BF_Block* block;
    BF_Block_Init(&block);

    error = BF_GetBlock(*file_desc, 0, block);  
    if(error != BF_OK){
      BF_PrintError(error);
      BF_UnpinBlock(block);
      return NULL;
    }
    else{
      char* data = BF_Block_GetData(block);
       HP_info* hp_info = (HP_info*) data;

      BF_UnpinBlock(block);
      return hp_info;
    }

}


int HP_CloseFile(int file_desc,HP_info* hp_info ){
  int error;

  for(int i = 0; i <= hp_info->last_block; i++){
    BF_Block* block;
    BF_Block_Init(&block);
    error = BF_GetBlock(file_desc, i, block);

    if(error != BF_OK){
      BF_PrintError(error);
      return -1;
    }

    else{
      BF_UnpinBlock(block);
      BF_Block_Destroy(&block);
    }

  }
  
  error = BF_CloseFile(file_desc);
  return 0;
}



int HP_InsertEntry(int file_desc, HP_info* hp_info, Record record){

    //δεν χρειάζεται Open file γίνεται στο main


    BF_Block* new_block;
    BF_Block_Init(&new_block);

    int error;
    int block_id = hp_info->last_block;

    //αν είναι το πρώτο block μετα το block με τα μεταδεδομένα
    if(block_id == 0 ){

      //φτιάχνουμε νεο μπλοκ στο αρχείο
      error = BF_AllocateBlock(file_desc, new_block);
      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }

      memcpy(BF_Block_GetData(new_block), &record, sizeof(Record));
      hp_info->number_of_blocks ++;
      hp_info->last_block = 1;
      hp_info->first_block_with_records = new_block;

      HP_block_info block_info;
      block_info.number_of_records = 1;
      block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(Record) - sizeof(HP_block_info);
      block_info.next_block = NULL;

      //παιρνουμε τη διευθυνση των δεδομενων του μπλοκ, παμε στο τελος του και τοποθετουμε στα τελευταια 
      //sizeof(HP_block_info) bytes τη δομη HP_block_info
      memcpy(BF_Block_GetData(new_block) + BF_BLOCK_SIZE - sizeof(block_info), &block_info, sizeof(HP_block_info));

      
      BF_Block_SetDirty(new_block);
      BF_UnpinBlock(new_block);
      return hp_info->last_block;

    }

    else{
      BF_Block* last_block;
      BF_Block_Init(&last_block);


      BF_GetBlock(file_desc, block_id, last_block);
      void* data = BF_Block_GetData(last_block);

      //βρίσκουμε το struct με τις πληροφορίες για το block για να δούμε εάν χωράει νέα εγγραφή
      HP_block_info* last_block_info;
      last_block_info = (HP_block_info*) (data + BF_BLOCK_SIZE - sizeof(HP_block_info));


      //αν δεν χωράει άλλη εγγραφή στο block
      if(last_block_info->current_block_capacity < sizeof(Record)){
        //φτιάχνουμε νεο μπλοκ στο αρχείο
        error = BF_AllocateBlock(file_desc, new_block);
        if(error != BF_OK){
          BF_PrintError(error);
          return -1;
        }

        memcpy(BF_Block_GetData(new_block), &record, sizeof(Record));
        hp_info->number_of_blocks ++;
        hp_info->last_block ++;

        HP_block_info block_info;
        block_info.number_of_records = 1;
        block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(Record) - sizeof(HP_block_info);
        block_info.next_block = NULL;

        //παιρνουμε τη διευθυνση των δεδομενων του μπλοκ, παμε στο τελος του και τοποθετουμε στα τελευταια 
        //sizeof(HP_block_info) bytes τη δομη HP_block_info
        memcpy(BF_Block_GetData(new_block) + BF_BLOCK_SIZE - sizeof(block_info), &block_info, sizeof(HP_block_info));

      }
      else{
        memcpy(data + ((last_block_info->number_of_records) * sizeof(Record) ) , &record, sizeof(Record));
        last_block_info->number_of_records ++;
        last_block_info->current_block_capacity -= sizeof(Record);
      }
    
      BF_UnpinBlock(last_block);
    }
    return hp_info->last_block;
    BF_Block_SetDirty(new_block);
    BF_UnpinBlock(new_block);
}

int HP_GetAllEntries(int file_desc,HP_info* hp_info, int value){    
    return -1;
}

