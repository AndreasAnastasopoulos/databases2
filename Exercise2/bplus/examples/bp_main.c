#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bf.h"
#include "bp_file.h"
#include "bp_datanode.h"
#include "bp_indexnode.h"

#define RECORDS_NUM 200 // you can change it if you want
#define FILE_NAME "data.db"

#define TEST_FILE_1 "test1.db"
#define TEST_FILE_2 "test2.db"

#define CALL_OR_DIE(call)     \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      exit(code);             \
    }                         \
  }

void insertEntries();
void findEntries();
// Our function tests
void testSequentialInsert();
void testRandomInsert();
void testDuplicateInsert();
void testBoundaryConditions();
void testMultipleFiles();
void testSearchNonExistent();
void stressTest();

int main()
{
  printf("\n=== Starting B+ Tree Tests ===\n");

  printf("\n--- Test given by Mr. Μαϊλης ---\n");
  insertEntries();
  findEntries();

/*
  testSequentialInsert();
  testDuplicateInsert();
  testRandomInsert();
  testBoundaryConditions();
  testMultipleFiles();
  testSearchNonExistent();
  stressTest(); */

  printf("\n=== All Tests Completed ===\n");
  return 0;
}

void insertEntries()
{
  BF_Init(LRU);
  BP_CreateFile(FILE_NAME);
  int file_desc;
  BPLUS_INFO *info = BP_OpenFile(FILE_NAME, &file_desc);
  Record record;
  for (int i = 0; i < RECORDS_NUM; i++)
  {
    record = randomRecord();
    BP_InsertEntry(file_desc, info, record);
  }
  BP_CloseFile(file_desc, info);
  BF_Close();
}

void findEntries()
{
  int file_desc;
  BPLUS_INFO *info;

  BF_Init(LRU);
  info = BP_OpenFile(FILE_NAME, &file_desc);

  Record tmpRec; // Αντί για malloc
  Record *result = &tmpRec;

  int id = 159;
  printf("Searching for: %d\n", id);
  BP_GetEntry(file_desc, info, id, &result);
  if (result != NULL)
    printRecord(*result);

  BP_CloseFile(file_desc, info);
  BF_Close();
}

// Our tests:

// Test sequential insertion and retrieval
void testSequentialInsert() {
    printf("\n--- Testing Sequential Insertion ---\n");
    
    BF_Init(LRU);
    BP_CreateFile(TEST_FILE_1);
    
    int file_desc;
    BPLUS_INFO* info = BP_OpenFile(TEST_FILE_1, &file_desc);
    
    // Insert 100 sequential records
    Record record;
    for (int i = 1; i <= 100; i++) {
        record.id = i;
        sprintf(record.name, "name_%d", i);
        sprintf(record.surname, "surname_%d", i);
        sprintf(record.city, "city_%d", i);
        
        int result = BP_InsertEntry(file_desc, info, record);
        if (result == -1) {
            printf("Failed to insert record with id %d\n", i);
        }
    }
    
    // Verify insertions
    Record* result = NULL;
    for (int i = 1; i <= 100; i++) {
        if (BP_GetEntry(file_desc, info, i, &result) == 0) {
            printf("Found record %d: %s %s %s\n", 
                   result->id, result->name, result->surname, result->city);
        } else {
            printf("Failed to find record with id %d\n", i);
        }
    }
    
    BP_CloseFile(file_desc, info);
    BF_Close();
}

// Test random insertion and retrieval
void testRandomInsert() {
    printf("\n--- Testing Random Insertion ---\n");
    
    BF_Init(LRU);
    BP_CreateFile(TEST_FILE_2);
    
    int file_desc;
    BPLUS_INFO* info = BP_OpenFile(TEST_FILE_2, &file_desc);
    
    // Create array of random IDs
    int ids[50];
    for (int i = 0; i < 50; i++) {
        ids[i] = rand() % 1000 + 1;
    }
    
    // Insert records with random IDs
    Record record;
    for (int i = 0; i < 50; i++) {
        record.id = ids[i];
        sprintf(record.name, "random_%d", ids[i]);
        sprintf(record.surname, "surname_%d", ids[i]);
        sprintf(record.city, "city_%d", ids[i]);
        
        BP_InsertEntry(file_desc, info, record);
    }
    
    // Verify random insertions
    Record* result = NULL;
    for (int i = 0; i < 50; i++) {
        if (BP_GetEntry(file_desc, info, ids[i], &result) == 0) {
            printf("Found random record %d\n", ids[i]);
        } else {
            printf("Failed to find random record %d\n", ids[i]);
        }
    }
    
    BP_CloseFile(file_desc, info);
    BF_Close();
}

// Test duplicate key handling
void testDuplicateInsert() {
    printf("\n--- Testing Duplicate Insertion ---\n");
    
    BF_Init(LRU);
    BP_CreateFile("duplicate_test.db");
    
    int file_desc;
    BPLUS_INFO* info = BP_OpenFile("duplicate_test.db", &file_desc);
    
    Record record;
    record.id = 42;
    strcpy(record.name, "Test");
    strcpy(record.surname, "User");
    strcpy(record.city, "TestCity");
    
    // Try inserting same record twice
    printf("Inserting record with ID 42\n");
    int result1 = BP_InsertEntry(file_desc, info, record);
    printf("First insertion result: %d\n", result1);
    
    printf("Attempting duplicate insertion with ID 42\n");
    int result2 = BP_InsertEntry(file_desc, info, record);
    printf("Second insertion result: %d\n", result2);
    
    BP_CloseFile(file_desc, info);
    BF_Close();
}

// Test boundary conditions
void testBoundaryConditions() {
    printf("\n--- Testing Boundary Conditions ---\n");
    
    BF_Init(LRU);
    
    // Test 1: Try to open non-existent file
    int file_desc;
    BPLUS_INFO* info = BP_OpenFile("nonexistent.db", &file_desc);
    if (info == NULL) {
        printf("Correctly handled non-existent file\n");
    }
    
    // Test 2: Create file and test empty tree search
    BP_CreateFile("empty_test.db");
    info = BP_OpenFile("empty_test.db", &file_desc);
    
    Record* result = NULL;
    if (BP_GetEntry(file_desc, info, 1, &result) == -1) {
        printf("Correctly handled search in empty tree\n");
    }
    
    BP_CloseFile(file_desc, info);
    BF_Close();
}

// Stress test with large number of insertions
void stressTest() {
    printf("\n--- Running Stress Test ---\n");
    
    BF_Init(LRU);
    BP_CreateFile("stress_test.db");
    
    int file_desc;
    BPLUS_INFO* info = BP_OpenFile("stress_test.db", &file_desc);
    
    // Insert 1000 records
    Record record;
    int insertCount = 0;
    for (int i = 0; i < 1000; i++) {
        record.id = i;
        sprintf(record.name, "stress_%d", i);
        sprintf(record.surname, "test_%d", i);
        sprintf(record.city, "city_%d", i);
        
        if (BP_InsertEntry(file_desc, info, record) != -1) {
            insertCount++;
        }
    }
    
    printf("Successfully inserted %d records\n", insertCount);
    
    // Random access test
    int successCount = 0;
    Record* result = NULL;
    for (int i = 0; i < 100; i++) {
        int randomId = rand() % 1000;
        if (BP_GetEntry(file_desc, info, randomId, &result) == 0) {
            successCount++;
        }
    }
    
    printf("Successfully retrieved %d/100 random records\n", successCount);
    
    BP_CloseFile(file_desc, info);
    BF_Close();
}

void testMultipleFiles() {
    printf("\n--- Testing Multiple Files ---\n");
    
    BF_Init(LRU);
    
    // Create and open multiple files
    BP_CreateFile("file1.db");
    BP_CreateFile("file2.db");
    
    int fd1, fd2;
    BPLUS_INFO* info1 = BP_OpenFile("file1.db", &fd1);
    BPLUS_INFO* info2 = BP_OpenFile("file2.db", &fd2);
    
    // Insert different records in each file
    Record record;
    
    // File 1: Even numbers
    for (int i = 0; i < 50; i += 2) {
        record.id = i;
        sprintf(record.name, "even_%d", i);
        BP_InsertEntry(fd1, info1, record);
    }
    
    // File 2: Odd numbers
    for (int i = 1; i < 50; i += 2) {
        record.id = i;
        sprintf(record.name, "odd_%d", i);
        BP_InsertEntry(fd2, info2, record);
    }
    
    // Test retrieval from both files
    Record* result = NULL;
    printf("Testing even numbers in file1:\n");
    for (int i = 0; i < 10; i += 2) {
        if (BP_GetEntry(fd1, info1, i, &result) == 0) {
            printf("Found %d in file1\n", i);
        }
    }
    
    printf("Testing odd numbers in file2:\n");
    for (int i = 1; i < 10; i += 2) {
        if (BP_GetEntry(fd2, info2, i, &result) == 0) {
            printf("Found %d in file2\n", i);
        }
    }
    
    BP_CloseFile(fd1, info1);
    BP_CloseFile(fd2, info2);
    BF_Close();
}

void testSearchNonExistent() {
    printf("\n--- Testing Search for Non-existent Records ---\n");
    
    BF_Init(LRU);
    BP_CreateFile("search_test.db");
    
    int file_desc;
    BPLUS_INFO* info = BP_OpenFile("search_test.db", &file_desc);
    
    // Insert some records
    Record record;
    for (int i = 1; i <= 10; i++) {
        record.id = i * 10;  // IDs: 10, 20, 30, ..., 100
        sprintf(record.name, "name_%d", i * 10);
        BP_InsertEntry(file_desc, info, record);
    }
    
    // Search for non-existent records
    Record* result = NULL;
    int nonexistent_ids[] = {5, 15, 25, 35, 45};
    
    for (int i = 0; i < 5; i++) {
        printf("Searching for non-existent ID: %d\n", nonexistent_ids[i]);
        int search_result = BP_GetEntry(file_desc, info, nonexistent_ids[i], &result);
        if (search_result == -1) {
            printf("Correctly reported non-existent record %d\n", nonexistent_ids[i]);
        } else {
            printf("Unexpectedly found record %d\n", nonexistent_ids[i]);
        }
    }
    
    BP_CloseFile(file_desc, info);
    BF_Close();
}

