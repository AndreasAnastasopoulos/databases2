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

      //Δημιουργία δεικτη σε block
      //το block δεν ειναι τιποτα αλλο παρα ενας δεικτης στα δεδομενα ενος μπλοκ
      //που τον χρησιμοποιουμε για να εχουμε προσβαση σε αυτα 
      //οταν δεν τον χρειαζομαστε αλλο πρεπει να τον καταστρεψουμε
      BF_Block* block;
      
      BF_Block_Init(&block);

      //Δέσμευση block στο αρχείο
      BF_AllocateBlock(file_desc, block);
      
      void* data = BF_Block_GetData(block);

      HP_info hp_info;
      hp_info.last_block = 0; //ο αριθμος του τελευταιου block του αρχειου
      hp_info.number_of_blocks = 0; //πληθος των blocks
      hp_info.first_block_with_records = NULL; //το πρωτο block που περιεχει εγγραφες

      //υπολογισμος του πόσες εγγραφές χωράει ένα block, αφαιρώντας το μέγεθος της δομής HP_block_info
      //το οποίο βρίσκεται στο τέλος ΚΑΘΕ block
      hp_info.records_per_block = (BF_BLOCK_SIZE - sizeof(HP_block_info) )/ sizeof(Record);

      //αντιγραφή των δεδομένων της δομής HP_info στο block 0 με τα μεταδεδομενα
      memcpy(data, &hp_info , sizeof(hp_info));

      BF_Block_SetDirty(block); //αφου τροποποιηθηκε το block, το κανουμε dirty
      BF_UnpinBlock(block); //δεν το χρειαζομαστε πλεον
      BF_Block_Destroy(&block); //καταστροφη του δεικτη
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

      //αποθηκευση των μεταδεδομενων στη δομη hp_info
      HP_info* hp_info = (HP_info*) data;

      BF_UnpinBlock(block); //μονο unpin χωρις set_dirty αφου δεν αλλαξαμε κατι
      BF_Block_Destroy(&block);
      return hp_info;
    }

}


int HP_CloseFile(int file_desc,HP_info* hp_info ){
  int error;
  BF_Block* block;
  BF_Block_Init(&block);

  for(int i = 0; i <= hp_info->last_block; i++){
    
    error = BF_GetBlock(file_desc, i, block);
    if(error != BF_OK){
      BF_PrintError(error);
      return -1;
    }

    else{
      BF_UnpinBlock(block);
    }

  }

  BF_Block_Destroy(&block);
  
  error = BF_CloseFile(file_desc);
  if(error != BF_OK){
      BF_PrintError(error);
      return -1;
  }

  return 0;
}



int HP_InsertEntry(int file_desc, HP_info* hp_info, Record record){

    //δεν χρειάζεται Open file γίνεται στο main

    BF_Block* new_block;
    BF_Block_Init(&new_block);

    int error;
    int block_id = hp_info->last_block;

    //αν εχουμε μονο ενα block, αυτο με τα μεταδεδομενα
    if(block_id == 0){

      //φτιάχνουμε νεο μπλοκ στο αρχείο
      error = BF_AllocateBlock(file_desc, new_block);
      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }

      //αντιγραφουμε την εγγραφη στην αρχη του καινουργιου μπλοκ
      memcpy(BF_Block_GetData(new_block), &record, sizeof(Record));

      //ενημερωνουμε το hp_info
      hp_info->number_of_blocks ++;
      hp_info->last_block = 1;
      hp_info->first_block_with_records = new_block;

      //ενημερωση στοιχειων του block
      HP_block_info block_info;
      block_info.number_of_records = 1;
      block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(Record) - sizeof(HP_block_info);
      block_info.next_block = NULL;

      //παιρνουμε τη διευθυνση των δεδομενων του μπλοκ, παμε στο τελος του και τοποθετουμε στα τελευταια 
      //sizeof(HP_block_info) bytes τη δομη HP_block_info
      memcpy(BF_Block_GetData(new_block) + BF_BLOCK_SIZE - sizeof(block_info), &block_info, sizeof(HP_block_info));

      //αφου τελειωσαμε με την εισαγωγη δεν χρειαζομαστε αλλο το μπλοκ
      BF_Block_SetDirty(new_block);
      BF_UnpinBlock(new_block);
      BF_Block_Destroy(&new_block);

      return hp_info->last_block;

    }

    //αν δεν είναι το πρώτο block που εισάγεται , τότε πρέπει να έχουμε πρ΄σοβαση και στο προηγούμενο block
    //ωστε να ξέρουμε αν υπάρχει χώρος για νέα εγγραφή
    else{
      BF_Block* last_block;
      BF_Block_Init(&last_block);


      error = BF_GetBlock(file_desc, block_id, last_block);
      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }

      //τα δεδομενα του τελευταιου μπλοκ
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

        //αντιγραφουμε την εγγραφη στην αρχη του νεου μπλοκ
        memcpy(BF_Block_GetData(new_block), &record, sizeof(Record));
        hp_info->number_of_blocks ++;

        int id;
        error = BF_GetBlockCounter(file_desc, &id);
        if(error != BF_OK){
          BF_PrintError(error);
          return -1;
        }

        //κραταμε τον αριθμο του τελευταιου μπλοκ(συνολικα μπλοκ - 1, αφου η αριθμηση ξεκιναει απο το 0)
        hp_info->last_block = id - 1;

        HP_block_info block_info;
        block_info.number_of_records = 1;
        block_info.current_block_capacity = BF_BLOCK_SIZE - sizeof(Record) - sizeof(HP_block_info);
        block_info.next_block = NULL;

        //ενημερωση next block του προηγουμενου
        last_block_info->next_block = new_block;

        //παιρνουμε τη διευθυνση των δεδομενων του μπλοκ, παμε στο τελος του και τοποθετουμε στα τελευταια 
        //sizeof(HP_block_info) bytes τη δομη HP_block_info
        memcpy(BF_Block_GetData(new_block) + BF_BLOCK_SIZE - sizeof(block_info), &block_info, sizeof(HP_block_info));

        //αφου τελειωσουμε με την εισαγωγη, δεν χρειαζομαστε πια το μπλοκ
        BF_Block_SetDirty(last_block);
        BF_Block_SetDirty(new_block);
        BF_UnpinBlock(last_block);
        BF_UnpinBlock(new_block);

      }

      //αν εχουμε χωρο στο τωρινο μπλοκ, προσθετουμε την εγγραφη
      else{
        //αντιγραφη εγγραφης μετα την τελευταια εγγραφη
        memcpy(data + ((last_block_info->number_of_records) * sizeof(Record) ), &record, sizeof(Record));
        last_block_info->number_of_records ++;
        last_block_info->current_block_capacity -= sizeof(Record);

        BF_Block_SetDirty(last_block); //αφου αλλαξαμε το last block
        BF_UnpinBlock(last_block); 
      }

      BF_Block_Destroy(&new_block);
      BF_Block_Destroy(&last_block);
    }

    return hp_info->last_block;
}

int HP_GetAllEntries(int file_desc,HP_info* hp_info, int value){    

    BF_Block* block;
    BF_Block_Init(&block);

    for(int i = 1; i < hp_info->number_of_blocks; i++){

      int error = BF_GetBlock(file_desc, i, block);
      if(error != BF_OK){
        BF_PrintError(error);
        return -1;
      }

      else{
        void* data = BF_Block_GetData(block);
        HP_block_info* block_info = (HP_block_info*) (data + BF_BLOCK_SIZE - sizeof(HP_block_info));
        int num = block_info->number_of_records;

        for(int j = 0; j < num; j++){
          Record* record = (Record*) (data + j * sizeof(Record));
          
          if(record->id == value){
            printRecord(*record);
          }
        }
      }

      BF_UnpinBlock(block);
    }
    BF_Block_Destroy(&block);
    
    return hp_info->number_of_blocks;
}


