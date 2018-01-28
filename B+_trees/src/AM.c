#include "AM.h"
#include "bf.h"
#include "defn.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

//================================================================================================
//                                        Defines start
//================================================================================================
#define BLOCKSIZE 512
#define MAXOPENEDFILES 20
#define MAXOPENEDSCANS 20
//================================================================================================
//                                        Defines end
//================================================================================================

//================================================================================================
//                                Global initializations start
//================================================================================================
struct OpenedFile OpenedFileArray[MAXOPENEDFILES]; // this is the array that keeps opened files
struct OpenedScan OpenedScanArray[MAXOPENEDSCANS]; // this is the array that keeps opened scans

int AM_errno = AME_OK;
//================================================================================================
//                                Global initializations ends
//================================================================================================

//================================================================================================
//                                        AM_Init starts
//================================================================================================
void AM_Init() // Initializing global arrays
{
  BF_Init(LRU);
  int i = 0;
  for (i = 0; i < MAXOPENEDFILES; i++)
  {
    OpenedFileArray[i].fileDesc = -1;
    OpenedFileArray[i].attrLength1 = -1;
    OpenedFileArray[i].attrLength2 = -1;
    OpenedFileArray[i].attrType1 = -1;
    OpenedFileArray[i].attrType2 = -1;
    strcpy(OpenedFileArray[i].fileName, "");
    OpenedFileArray[i].entriesDataBlockCanContain = -1;
    OpenedFileArray[i].entriesIndexBlockCanContain = -1;
    OpenedFileArray[i].rootBlock = -1;
  }
  for (i = 0; i < MAXOPENEDSCANS; i++)
  {
    OpenedScanArray[i].fileDesc = -1;
    OpenedScanArray[i].attrLength1 = -1;
    OpenedScanArray[i].attrLength2 = -1;
    OpenedScanArray[i].attrType1 = -1;
    OpenedScanArray[i].attrType2 = -1;
    OpenedScanArray[i].nextBlock = -1;
    OpenedScanArray[i].nextEntry = -1;
    OpenedScanArray[i].endBlock = -1;
    OpenedScanArray[i].endEntry = -1;
    OpenedScanArray[i].startBlock = -1;
    OpenedScanArray[i].startEntry = -1;
    OpenedScanArray[i].neq = -1;
  }
  return;
}
//================================================================================================
//                                       AM_Init ends
//================================================================================================

//================================================================================================
//                                   AM_CreateIndex starts
//================================================================================================
int AM_CreateIndex(char *fileName,
                   char attrType1,
                   int attrLength1,
                   char attrType2,
                   int attrLength2)
{
  //initializing metadata struct
  struct FileMetaData mdContent;
  mdContent.identifier = 777; // Convention to recognize our blocks
  mdContent.attrLength1 = attrLength1;
  mdContent.attrLength1 = attrLength2;
  mdContent.attrType1 = attrType1;
  mdContent.attrType2 = attrType2;
  mdContent.rootBlock = 1;
  //to be sure it is always EVEN
  int temp = (BLOCKSIZE - sizeof(struct BlockMetaData)) / ((2 * sizeof(int)) + attrLength1 + attrLength2);
  if (temp % 2 == 1)
  {
    temp--;
  }
  mdContent.entriesIndexBlockCanContain = temp;
  temp = (BLOCKSIZE - sizeof(struct BlockMetaData)) / ((2 * sizeof(int)) + attrLength1 + attrLength2);
  if (temp % 2 == 1)
  {
    temp--;
  }
  mdContent.entriesDataBlockCanContain = temp;

  int fileDesc;
  BF_Block *fileMetaDataBlock;
  BF_Block *rootBlock;
  char *fileMetaDataBlockContent; // Content of metadata block
  int errorCode;

  //creating file
  errorCode = BF_CreateFile(fileName);
  if (errorCode) // Trying to create file
  {
    AM_errno = AME_FILE_CREATE_FAILED_ERROR;
    AM_PrintError("Failed to create index while was creating file");
    return AM_errno;
  }
  //opening the file
  errorCode = BF_OpenFile(fileName, &fileDesc); // Trying to open file
  if (errorCode)
  {
    AM_errno = AME_FILE_OPEN_FAILED_ERROR;
    AM_PrintError("Failed to open file");
    return AM_errno;
  }

  //----------------Creating Metadata block---------------//
  BF_Block_Init(&fileMetaDataBlock);                         // Initializing block
  errorCode = BF_AllocateBlock(fileDesc, fileMetaDataBlock); // Trying to allocate initial metadata block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_ALOCATION_FAILED_ERROR;
    AM_PrintError("Failed to allocate block");
    return AM_errno;
  }
  fileMetaDataBlockContent = BF_Block_GetData(fileMetaDataBlock); // Getting block Content
  memcpy(fileMetaDataBlockContent, &mdContent, sizeof(struct FileMetaData));
  BF_Block_SetDirty(fileMetaDataBlock);         // Setting dirty the block (to be written on disk)
  errorCode = BF_UnpinBlock(fileMetaDataBlock); // We dont need this block anymore
  if (errorCode)
  {
    AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
    AM_PrintError("Failed to unpin block");
    return AM_errno;
  }
  //---------Creating Metadata block finished-------------//
  int rootBlockNum = CreateBlock(Index, fileDesc, -1, -1, -1); // Trying to create root block
  return AME_OK;
}
//================================================================================================
//                                  AM_CreateIndex ends
//================================================================================================

//================================================================================================
//                                  AM_DestroyIndex starts
//================================================================================================
int AM_DestroyIndex(char *fileName)
{
  //if the file exists in file array, getting its name to delete it from file system
  int exists = 0;
  int i;
  for (i = 0; i < MAXOPENEDFILES; i++)
  {
    exists = strcmp(OpenedFileArray[i].fileName, fileName);
    if (exists == 1)
    {
      break;
    }
  }
  if (exists)
  {
    AM_errno = AME_FILE_OPENED_ERROR;
    AM_PrintError("There is an opened file");
    return AM_errno;
  }
  else
  {
    char command[100];
    sprintf(command, "rm %s", fileName);
    system(command);
  }
  return AME_OK;
}
//================================================================================================
//                                  AM_DestroyIndex ends
//================================================================================================
//================================================================================================
//                                 AM_OpenIndex starts
//================================================================================================

int AM_OpenIndex(char *fileName)
{
  int i;
  int errorCode, fileDesc, rootBlockNumber;

  errorCode = BF_OpenFile(fileName, &fileDesc); // Trying to open file
  if (errorCode)
  {
    AM_errno = AME_FILE_OPEN_FAILED_ERROR;
    AM_PrintError("Failed to open file");
    return AM_errno;
  }
  for (i = 0; i < MAXOPENEDFILES; i++)
  { //check for empty position
    if (OpenedFileArray[i].fileDesc == -1)
    {
      OpenedFileArray[i].fileDesc = 0;
      strcpy(OpenedFileArray[i].fileName, fileName);
      BF_Block *currentBlock;
      char *currentBlockData; // Current block data
      struct FileMetaData *fileMetaData;
      struct BlockMetaData *blockMetaData;
      BF_Block_Init(&currentBlock);                       // Initializing block
      errorCode = BF_GetBlock(fileDesc, 0, currentBlock); //Trying to get first block
      if (errorCode)
      {
        AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
        AM_PrintError("Failed to get the 0 block");
        return AM_errno;
      }
      currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
      fileMetaData = (struct FileMetaData *)currentBlockData;
      OpenedFileArray[i].attrLength1 = fileMetaData->attrLength1;
      OpenedFileArray[i].attrLength2 = fileMetaData->attrLength2;
      OpenedFileArray[i].attrType1 = fileMetaData->attrType1;
      OpenedFileArray[i].attrType2 = fileMetaData->attrType2;
      OpenedFileArray[i].entriesDataBlockCanContain = fileMetaData->entriesDataBlockCanContain;
      OpenedFileArray[i].entriesIndexBlockCanContain = fileMetaData->entriesIndexBlockCanContain;
      BF_CloseFile(fileDesc);
      return i;
    }
    return AME_FILE_OPEN_FAILED_ERROR;
  }
  return AME_OK;
}

//================================================================================================
//                                  AM_OpenIndex ends
//================================================================================================

//================================================================================================
//                                  AM_CloseIndex starts
//================================================================================================
int AM_CloseIndex(int fileDesc)
{

  int i;
  //removing index from oppened file array
  for (i = 0; i < MAXOPENEDSCANS; i++)
  {
    if (OpenedScanArray[i].fileDesc == fileDesc)
    {
      AM_errno = AME_FILE_SCAN_FAILED_FIND_ERROR;
      return AM_errno;
    }
  }
  for (i = 0; i < MAXOPENEDFILES; i++)
  {
    if (OpenedFileArray[i].fileDesc == fileDesc)
    {
      OpenedFileArray[i].fileDesc = -1;
      OpenedFileArray[i].attrLength1 = -1;
      OpenedFileArray[i].attrLength2 = -1;
      OpenedFileArray[i].attrType1 = -1;
      OpenedFileArray[i].attrType2 = -1;
      OpenedFileArray[i].entriesDataBlockCanContain = -1;
      OpenedFileArray[i].entriesIndexBlockCanContain = -1;
      strcpy(OpenedFileArray[i].fileName, "");
      return AME_OK;
    }
  }
  AM_errno = AME_FILE_NOT_EXISTS_ERROR;
  printf("file does not exists");
  return AM_errno;
}
//================================================================================================
//                                  AM_CloseIndex ends
//================================================================================================

//================================================================================================
//                                  AM_InsertEntry starts
//================================================================================================
int AM_InsertEntry(int fileDesc, void *value1, void *value2)
{
  int errorCode, blockNumber, i;
  //----------------------Getting root block--------------------------
  BF_Block *currentBlock;
  char *currentBlockData; // Current block data
  struct FileMetaData *fileMetaData;
  struct BlockMetaData *currentBlockMetaData;
  struct OpenedFile *fileInfo;
  GetFileInfo(fileDesc, fileInfo);
  BF_Block_Init(&currentBlock);                       // Initializing block
  errorCode = BF_GetBlock(fileDesc, 0, currentBlock); //Trying to get first block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
    AM_PrintError("Failed to get the 0 block");
    return AM_errno;
  }
  currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
  fileMetaData = (struct FileMetaData *)currentBlockData;
  blockNumber = fileMetaData->rootBlock;
  errorCode = BF_GetBlock(fileDesc, blockNumber, currentBlock); //Trying to get first block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
    AM_PrintError("Failed to get the 0 block");
    return AM_errno;
  }
  currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
  currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
  char key1[255];
  memcpy(
      &key1,
      value1,
      fileInfo->attrLength1);
  char key2[255];
  char key3[255];
  int pointer;
  int found = 0;
  if (currentBlockMetaData->blockNumber == fileInfo->rootBlock) //in case its the first entry of the whole data structure
  {
    if (currentBlockMetaData->existingEntries == 0)
    {
      int newBlockNum = CreateBlock(Data, fileDesc, -1, -1, fileInfo->rootBlock);
      WriteToBlock(newBlockNum, fileDesc, -1, value1, value2);
      return AME_OK;
    }
  }
  while (!currentBlockMetaData->isDataBlock) //searching for the block we need to to add the entry
  {
    for (i = 0; i < currentBlockMetaData->existingEntries; i++)
    {
      switch (fileInfo->attrType1)
      {
        {
        case 'c':
          memcpy(
              &key2,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (2 * (sizeof(int)) + fileInfo->attrLength1)),
              fileInfo->attrLength1);
          if (i + 1 < currentBlockMetaData->existingEntries)
          {
            memcpy(
                &key3,
                currentBlockData + sizeof(struct BlockMetaData) + ((i + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                fileInfo->attrLength1);
          }
          memcpy(
              &pointer,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              sizeof(int));
          if (strcmp(key1, key2) == 0 || (strcmp(key1, key2) > 0 && strcmp(key1, key3) < 0))
          {
            found = 1;
          }
          break;
        case 'i':
          memcpy(
              &key2,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (2 * (sizeof(int)) + fileInfo->attrLength1)),
              fileInfo->attrLength1);
          if (i + 1 < currentBlockMetaData->existingEntries)
          {
            memcpy(
                &key3,
                currentBlockData + sizeof(struct BlockMetaData) + ((i + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                fileInfo->attrLength1);
          }
          memcpy(
              &pointer,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              sizeof(int));
          if (atoi(key1) == atoi(key2) || atoi(key1) == atoi(key2) > 0 && atoi(key1) == atoi(key2))
          {
            found = 1;
          }
          break;
        case 'f':
          memcpy(
              &key2,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (2 * (sizeof(int)) + fileInfo->attrLength1)),
              fileInfo->attrLength1);
          if (i + 1 < currentBlockMetaData->existingEntries)
          {
            memcpy(
                &key3,
                currentBlockData + sizeof(struct BlockMetaData) + ((i + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                fileInfo->attrLength1);
          }
          memcpy(
              &pointer,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              sizeof(int));
          if (atof(key1) == atof(key2) || atof(key1) == atof(key2) > 0 && atof(key1) == atof(key2))
          {
            found = 1;
          }
          break;
        }
      }
      if (found)
      {
        errorCode = BF_GetBlock(fileDesc, pointer, currentBlock); //Trying to get first block
        if (errorCode)
        {
          AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
          AM_PrintError("Failed to get the 0 block");
          return AM_errno;
        }
        currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
        currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
      }
      break;
    }
    errorCode = BF_GetBlock(fileDesc, pointer, currentBlock); //Trying to get first block
    if (errorCode)
    {
      AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
      AM_PrintError("Failed to get the 0 block");
      return AM_errno;
    }
    currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
    currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
  }
  // if we are here that means we found the needed data block
  int finalBlockNum = currentBlockMetaData->blockNumber;
  errorCode = BF_UnpinBlock(currentBlock); // Trying to unpin
  if (errorCode)
  {
    AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
    AM_PrintError("Failed to unpin block");
    return AM_errno;
  }
  //inide the below function is the logic of B+ tree
  WriteToBlock(finalBlockNum, fileDesc, -1, value1, value2);
  return AME_OK;
}
//================================================================================================
//                                  AM_InsertEntry ends
//================================================================================================

//================================================================================================
//                                  AM_OpenIndexScan starts
//================================================================================================
int AM_OpenIndexScan(int fileDesc, int op, void *value)
{
  int i;
  for (i = 0; i < MAXOPENEDFILES; i++)
  {
    if (OpenedScanArray[i].fileDesc == -1) //
    {
      int errorCode, blockNumber, i;
      //----------------------Getting root block--------------------------
      BF_Block *currentBlock;
      char *currentBlockData; // Current block data
      struct FileMetaData *fileMetaData;
      struct BlockMetaData *currentBlockMetaData;
      struct OpenedFile *fileInfo;
      GetFileInfo(fileDesc, fileInfo);
      BF_Block_Init(&currentBlock);                       // Initializing block
      errorCode = BF_GetBlock(fileDesc, 0, currentBlock); //Trying to get first block
      if (errorCode)
      {
        AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
        AM_PrintError("Failed to get the 0 block");
        return AM_errno;
      }
      currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
      fileMetaData = (struct FileMetaData *)currentBlockData;
      blockNumber = fileMetaData->rootBlock;
      errorCode = BF_GetBlock(fileDesc, blockNumber, currentBlock); //Trying to get first block
      if (errorCode)
      {
        AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
        AM_PrintError("Failed to get the 0 block");
        return AM_errno;
      }
      currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
      currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
      char key1[255];
      memcpy(
          &key1,
          value,
          fileInfo->attrLength1);
      int pointer;
      while (!currentBlockMetaData->isDataBlock) //searching for the first block
      {
        memcpy(
            &pointer,
            currentBlockData + sizeof(struct BlockMetaData),
            sizeof(int));
        errorCode = BF_GetBlock(fileDesc, pointer, currentBlock); //Trying to get first block
        if (errorCode)
        {
          AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
          AM_PrintError("Failed to get the 0 block");
          return AM_errno;
        }
        currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
        currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
      }
      OpenedScanArray[i].attrLength1 = fileInfo->attrLength1;
      OpenedScanArray[i].attrLength2 = fileInfo->attrLength2;
      OpenedScanArray[i].attrType1 = fileInfo->attrType1;
      OpenedScanArray[i].attrType2 = fileInfo->attrType2;
      OpenedScanArray[i].fileDesc = fileDesc;
      int targetBlock;
      int targetEntry;
      SearchEntry(value, fileDesc, &targetBlock, &targetEntry);
      switch (op)
      {
        { // in this cases -1 means the last existing
        case EQUAL:
          OpenedScanArray[i].startBlock = targetBlock;
          OpenedScanArray[i].startEntry = targetEntry;
          OpenedScanArray[i].nextBlock = targetBlock;
          OpenedScanArray[i].nextEntry = targetEntry;
          OpenedScanArray[i].endBlock = targetEntry;
          OpenedScanArray[i].endEntry = targetEntry;
          break;
        case NOT_EQUAL:
          OpenedScanArray[i].startBlock = currentBlockMetaData->blockNumber;
          OpenedScanArray[i].startEntry = 0;
          OpenedScanArray[i].nextBlock = currentBlockMetaData->blockNumber;
          OpenedScanArray[i].nextEntry = 0;
          OpenedScanArray[i].endBlock = -1;
          OpenedScanArray[i].endEntry = -1;
          OpenedScanArray[i].neq = 1;
          OpenedScanArray[i].target = targetEntry;
          break;
        case LESS_THAN:
          OpenedScanArray[i].startBlock = currentBlockMetaData->blockNumber;
          OpenedScanArray[i].startEntry = 0;
          OpenedScanArray[i].nextBlock = currentBlockMetaData->blockNumber;
          OpenedScanArray[i].nextEntry = 0;
          OpenedScanArray[i].endBlock = targetBlock;
          OpenedScanArray[i].endEntry = targetEntry;
          OpenedScanArray[i].neq = 1;
          OpenedScanArray[i].target = targetEntry;
          break;
        case GREATER_THAN:
          OpenedScanArray[i].startBlock = targetBlock;
          OpenedScanArray[i].startEntry = targetEntry;
          OpenedScanArray[i].nextBlock = targetBlock;
          OpenedScanArray[i].nextEntry = targetEntry;
          OpenedScanArray[i].endBlock = -1;
          OpenedScanArray[i].endEntry = -1;
          OpenedScanArray[i].neq = 1;
          OpenedScanArray[i].target = targetEntry;
          break;
        case LESS_THAN_OR_EQUAL:
          OpenedScanArray[i].startBlock = currentBlockMetaData->blockNumber;
          OpenedScanArray[i].startEntry = 0;
          OpenedScanArray[i].nextBlock = currentBlockMetaData->blockNumber;
          OpenedScanArray[i].nextEntry = 0;
          OpenedScanArray[i].endBlock = targetBlock;
          OpenedScanArray[i].endEntry = targetEntry;
          break;
        case GREATER_THAN_OR_EQUAL:
          OpenedScanArray[i].startBlock = targetBlock;
          OpenedScanArray[i].startEntry = targetEntry;
          OpenedScanArray[i].nextBlock = targetBlock;
          OpenedScanArray[i].nextEntry = targetEntry;
          OpenedScanArray[i].endBlock = -1;
          OpenedScanArray[i].endEntry = -1;
          break;
        }
      }
      return i;
    }
  }
}
//================================================================================================
//                                  AM_OpenIndexScan ends
//================================================================================================

//================================================================================================
//                                  AM_FindNextEntry starts
//================================================================================================
void *AM_FindNextEntry(int scanDesc)
{
  int errorCode, blockNumber, i;
  struct OpenedScan *scanInfo = &OpenedScanArray[scanDesc];
  BF_Block *currentBlock;
  char *currentBlockData; // Current block data
  struct FileMetaData *fileMetaData;
  struct BlockMetaData *currentBlockMetaData;
  struct OpenedFile *fileInfo;
  GetFileInfo(scanInfo->fileDesc, fileInfo);
  BF_Block_Init(&currentBlock);                                                   // Initializing block
  errorCode = BF_GetBlock(scanInfo->fileDesc, scanInfo->nextBlock, currentBlock); //Trying to get first block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
    AM_PrintError("Failed to get the 0 block");
    return NULL;
  }
  currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
  currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
  memcpy(
      scanInfo->currentValue,
      currentBlockData + sizeof(struct BlockMetaData) + (scanInfo->nextEntry * (scanInfo->attrType1 + scanInfo->attrLength2)),
      scanInfo->attrLength2);
  if (currentBlockMetaData->maxEntries > scanInfo->nextEntry && scanInfo->nextEntry < scanInfo->endEntry)
  {
    scanInfo->nextEntry++;
    if (scanInfo->nextEntry == scanInfo->target)
    {
      if (currentBlockMetaData->maxEntries > scanInfo->nextEntry)
      {
        scanInfo->nextEntry++;
      }
      else
      {
        errorCode = BF_GetBlock(scanInfo->fileDesc, currentBlockMetaData->nextBlock, currentBlock); //Trying to get first block
        if (errorCode)
        {
          AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
          AM_PrintError("Failed to get the 0 block");
          return NULL;
        }
        currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
        currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
        scanInfo->nextBlock = currentBlockMetaData->blockNumber;
        scanInfo->nextEntry = 1;
      }
    }
  }
  else if (scanInfo->nextEntry == scanInfo->endEntry)
  {
    AM_errno = AME_EOF;
    errorCode = BF_UnpinBlock(currentBlock); // We dont need this block anymore
    if (errorCode)
    {
      AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
      AM_PrintError("Failed to unpin block");
    }
    return NULL;
  }
  else if (currentBlockMetaData->maxEntries == scanInfo->nextEntry)
  {
    errorCode = BF_GetBlock(scanInfo->fileDesc, currentBlockMetaData->nextBlock, currentBlock); //Trying to get first block
    if (errorCode)
    {
      AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
      AM_PrintError("Failed to get the 0 block");
      return NULL;
    }
    currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
    currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
    scanInfo->nextBlock = currentBlockMetaData->blockNumber;
    scanInfo->nextEntry = 0;
  }
  AM_errno = AME_EOF;
  errorCode = BF_UnpinBlock(currentBlock); // We dont need this block anymore
  if (errorCode)
  {
    AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
    AM_PrintError("Failed to unpin block");
  }
  scanInfo->currentValue;
}
//================================================================================================
//                                  AM_FindNextEntry ends
//================================================================================================

//================================================================================================
//                                  AM_CloseIndexScan starts
//================================================================================================
int AM_CloseIndexScan(int scanDesc)
{
  //just removing index scan form the scan array
  OpenedScanArray[scanDesc].fileDesc = -1;
  OpenedScanArray[scanDesc].attrLength1 = -1;
  OpenedScanArray[scanDesc].attrLength2 = -1;
  OpenedScanArray[scanDesc].attrType1 = -1;
  OpenedScanArray[scanDesc].attrType2 = -1;
  OpenedScanArray[scanDesc].nextBlock = -1;
  OpenedScanArray[scanDesc].nextEntry = -1;
  OpenedScanArray[scanDesc].endBlock = -1;
  OpenedScanArray[scanDesc].endEntry = -1;
  OpenedScanArray[scanDesc].startBlock = -1;
  OpenedScanArray[scanDesc].startEntry = -1;
  OpenedScanArray[scanDesc].neq = -1;

  AM_errno = AME_OK;
  return AME_OK;
  AM_errno = AME_FILE_NOT_EXISTS_ERROR;
  printf("file does not exists");
  return AM_errno;
}

//================================================================================================
//                                  AM_CloseIndexScan ends
//================================================================================================

//================================================================================================
//                                  AM_PrintError starts
//================================================================================================
void AM_PrintError(char *errString)
{
  printf("%s", errString);
  printf("\n");

  switch (AM_errno)
  {
  case AME_FILE_OPENED_ERROR:
    printf("The file is already opened");
    break;

  case AME_FILE_CREATE_FAILED_ERROR:
    printf("The file creation failed");
    break;

  case AME_FILE_OPEN_FAILED_ERROR:
    printf("The file opening failed");
    break;

  case AME_FILE_CLOSE_FAILED_ERROR:
    printf("The file close failed");
    break;

  case AME_FILE_NOT_EXISTS_ERROR:
    printf("The file does not exist");
    break;

  case AME_FILE_SCAN_FAILED_FIND_ERROR:
    printf("Scan failed to find the file");
    break;

  case AME_FIND_NEXT_ENTRY_FAILED_ERRON:
    printf("Find next entry failed");
    break;

  case AME_BLOCK_ALOCATION_FAILED_ERROR:
    printf("The file opening failed");
    break;

  case AME_BLOCK_GETTING_FAILED_ERROR:
    printf("The file opening failed");
    break;

  case AME_BLOCK_UNPIN_FAILED_ERROR:
    printf("Unpin block failed");
    break;

  case AME_BLOCK_COUNT_GETTING_FAILED_ERROR:
    printf("unable to get block counter");
    break;

  case AME_BLOCK_CREATION_FAILED_ERROR:
    printf("unable to get block counter");
    break;

  case AME_AM_DISPOSE_ERROR:
    printf("Disposing of AM fails");
    break;

  case AME_AM_KEY_EXISTS_ERROR:
    printf("The Key Already Exists");
    break;
  }
  printf("\n");
}
//================================================================================================
//                                  AM_PrintError ends
//================================================================================================

//================================================================================================
//                                     AM_Close starts
//================================================================================================
void AM_Close()
{
  int errorCode = BF_Close();
  if (errorCode) // Trying to close file
  {
    AM_errno = AME_AM_DISPOSE_ERROR;
    AM_PrintError("Failed to close AM structure check if there is some unpinned blocks");
    return;
  }
  // The scan and file arrays will be disposed when programm finishes
}
//================================================================================================
//                                    AM_Close ends
//================================================================================================

//================================================================================================
//                               HELPER FUNCTIONS START
//================================================================================================

int CreateBlock(
    int blockType,
    int fileDesc,
    int previus,
    int next,
    int parrent)
{
  BF_Block *block;
  char *blockContent;
  int errorCode, blockCount, i;
  struct OpenedFile *fileInfo;
  GetFileInfo(fileDesc, fileInfo);
  struct BlockMetaData iContent;
  struct BlockMetaData dContent;
  BF_Block_Init(&block);                         // Initializing block
  errorCode = BF_AllocateBlock(fileDesc, block); // Trying to allocate initial metadata block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_ALOCATION_FAILED_ERROR;
    AM_PrintError("Failed to allocate block");
    return -1;
  }
  blockContent = BF_Block_GetData(block); // Getting block Content

  errorCode = BF_GetBlockCounter(fileDesc, &blockCount); // Getting Block count
  if (errorCode)
  {
    AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
    AM_PrintError("Failed to unpin block");
    return -1;
  }
  int pointer = -1;
  switch (blockType)
  {
  case Index:
    iContent.blockNumber = blockCount;
    iContent.isDataBlock = 0;
    iContent.existingEntries = 0;
    iContent.maxEntries = fileInfo->entriesIndexBlockCanContain;
    iContent.parrent = parrent;
    iContent.nextBlock = -1;
    iContent.previusBlock = -1;
    memcpy(blockContent, &iContent, sizeof(struct BlockMetaData));
    break;
  case Data:
    dContent.blockNumber = blockCount;
    dContent.isDataBlock = 1;
    dContent.existingEntries = 0;
    dContent.maxEntries = fileInfo->entriesIndexBlockCanContain;
    dContent.parrent = parrent;
    dContent.nextBlock = next;
    dContent.previusBlock = previus;
    memcpy(blockContent, &dContent, sizeof(struct BlockMetaData));
    break;
  }

  BF_Block_SetDirty(block);         // Setting dirty the block (to be written on disk)
  errorCode = BF_UnpinBlock(block); // We dont need this block anymore
  if (errorCode)
  {
    AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
    AM_PrintError("Failed to unpin block");
    return -1;
  }
  return blockCount;
}
//================================================================================================
int WriteToBlock(int blockNumber, int fileDesc, int pointer, void *key, void *entry)
{
  int errorCode;
  BF_Block *block;
  char *blockData; // Current block data
  struct BlockMetaData *blockMetaData;
  struct OpenedFile *fileInfo;
  GetFileInfo(fileDesc, fileInfo);
  BF_Block_Init(&block);                                 // Initializing block
  errorCode = BF_GetBlock(fileDesc, blockNumber, block); //Trying to get first block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
    AM_PrintError("Failed to get the block");
    return AM_errno;
  }
  blockData = BF_Block_GetData(block); // Reading the content
  blockMetaData = (struct BlockMetaData *)blockData;
  //in case its full
  if (blockMetaData->maxEntries == blockMetaData->existingEntries) // to add logic if its full
  {
    // case its data block+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    if (blockMetaData->isDataBlock)
    {
      int newBlockNum = CreateBlock(Data, fileDesc, blockMetaData->blockNumber, blockMetaData->nextBlock, blockMetaData->parrent);
      BF_Block *newBlock;
      char *newBlockData;
      BF_Block_Init(&newBlock);
      errorCode = BF_GetBlock(fileDesc, newBlockNum, newBlock); //Trying to get first block
      if (errorCode)
      {
        AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
        AM_PrintError("Failed to get the block");
        return AM_errno;
      }
      newBlockData = BF_Block_GetData(block); // Reading the content
      int halfEntries = blockMetaData->maxEntries / 2;
      memcpy(
          &newBlockData,
          blockData + sizeof(struct BlockMetaData) + (halfEntries * (fileInfo->attrLength1 + fileInfo->attrLength2)),
          halfEntries * (fileInfo->attrLength1 + fileInfo->attrLength2));
      blockMetaData->existingEntries = halfEntries;
      struct BlockMetaData *newBlockMetaData = (struct BlockMetaData *)newBlockData;
      newBlockMetaData->existingEntries = halfEntries;
      int toSecondBock = 0;
      char key1[255];
      char key2[255];
      switch (fileInfo->attrType1)
      {
        {
        case 'c':
          memcpy(
              &key1,
              newBlockData + sizeof(struct BlockMetaData),
              fileInfo->attrLength1);
          memcpy(
              &key2,
              key,
              fileInfo->attrLength1);
          toSecondBock = strcmp(key1, key2);
          if (toSecondBock == 0)
          {
            AM_errno = AME_AM_KEY_EXISTS_ERROR;
            AM_PrintError("Key Already Exists");
            return AM_errno;
          }
          else if (toSecondBock < 0)
          {
            toSecondBock = 1;
          }
          else
          {
            toSecondBock = 0;
          }
          break;
        case 'i':
          memcpy(
              &key1,
              newBlockData + sizeof(struct BlockMetaData),
              fileInfo->attrLength1);
          memcpy(
              &key2,
              key,
              fileInfo->attrLength1);
          if (atoi(key1) == atoi(key2))
          {
            AM_errno = AME_AM_KEY_EXISTS_ERROR;
            AM_PrintError("Key Already Exists");
            return AM_errno;
          }
          else if (atoi(key2) > atoi(key1))
          {
            toSecondBock = 1;
          }
          else
          {
            toSecondBock = 0;
          }
          break;
        case 'f':
          memcpy(
              &key1,
              newBlockData + sizeof(struct BlockMetaData),
              fileInfo->attrLength1);
          memcpy(
              &key2,
              key,
              fileInfo->attrLength1);
          if (atof(key1) == atof(key2))
          {
            AM_errno = AME_AM_KEY_EXISTS_ERROR;
            AM_PrintError("Key Already Exists");
            return AM_errno;
          }
          else if (atof(key2) > atof(key1))
          {
            toSecondBock = 1;
          }
          else
          {
            toSecondBock = 0;
          }
          break;
        }
      }
      if (toSecondBock)
      {
        memcpy(
            newBlockData + (halfEntries * (fileInfo->attrLength1 + fileInfo->attrLength2)),
            key,
            fileInfo->attrLength1);
        memcpy(
            newBlockData + (halfEntries * (fileInfo->attrLength1 + fileInfo->attrLength2)) + fileInfo->attrLength1,
            entry,
            fileInfo->attrLength2);
      }
      else
      {
        memcpy(
            blockData + (halfEntries * (fileInfo->attrLength1 + fileInfo->attrLength2)),
            key,
            fileInfo->attrLength1);
        memcpy(
            blockData + (halfEntries * (fileInfo->attrLength1 + fileInfo->attrLength2)) + fileInfo->attrLength1,
            entry,
            fileInfo->attrLength2);
      }
      char newKey[255];
      char oldKey[255];
      memcpy(
          &oldKey,
          blockData + sizeof(struct BlockMetaData) + sizeof(int),
          fileInfo->attrLength1);
      SortBlockContent(block, fileDesc);
      memcpy(
          &newKey,
          blockData + sizeof(struct BlockMetaData) + sizeof(int),
          fileInfo->attrLength1);
      ReplaceInParrent(fileDesc, blockMetaData->parrent, newKey, oldKey);

      SortBlockContent(newBlock, fileDesc);

      BF_Block_SetDirty(block);
      BF_Block_SetDirty(newBlock);
      errorCode = BF_UnpinBlock(block); // We dont need this block anymore
      if (errorCode)
      {
        AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
        AM_PrintError("Unpinning block failed");
        return AM_errno;
      }
      errorCode = BF_UnpinBlock(newBlock); // We dont need this block anymore
      if (errorCode)
      {
        AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
        AM_PrintError("Unpinning block failed");
        return AM_errno;
      }
      WriteToBlock(newBlockMetaData->parrent, fileDesc, newBlockMetaData->blockNumber, key1, NULL);
    }
    // case its index block+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    else
    {
      int newBlockNum = CreateBlock(Index, fileDesc, -1, -1, blockMetaData->parrent);
      BF_Block *newBlock;
      char *newBlockData;
      BF_Block_Init(&newBlock);
      errorCode = BF_GetBlock(fileDesc, newBlockNum, newBlock); //Trying to get first block
      if (errorCode)
      {
        AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
        AM_PrintError("Failed to get the block");
        return AM_errno;
      }
      newBlockData = BF_Block_GetData(block); // Reading the content
      int halfEntries = blockMetaData->maxEntries / 2;
      memcpy(
          &newBlockData,
          blockData + sizeof(struct BlockMetaData) + (halfEntries * (fileInfo->attrLength1 + sizeof(int))),
          halfEntries * (fileInfo->attrLength1 + sizeof(int)));
      blockMetaData->existingEntries = halfEntries;
      struct BlockMetaData *newBlockMetaData = (struct BlockMetaData *)newBlockData;
      newBlockMetaData->existingEntries = halfEntries;
      int toSecondBock = 0;
      char key1[255];
      char key2[255];
      switch (fileInfo->attrType1)
      {
        {
        case 'c':
          memcpy(
              &key1,
              newBlockData + sizeof(struct BlockMetaData),
              fileInfo->attrLength1);
          memcpy(
              &key2,
              key,
              fileInfo->attrLength1);
          toSecondBock = strcmp(key1, key2);
          if (toSecondBock == 0)
          {
            AM_errno = AME_AM_KEY_EXISTS_ERROR;
            AM_PrintError("Key Already Exists");
            return AM_errno;
          }
          else if (toSecondBock < 0)
          {
            toSecondBock = 1;
          }
          else
          {
            toSecondBock = 0;
          }
          break;
        case 'i':
          memcpy(
              &key1,
              newBlockData + sizeof(struct BlockMetaData),
              fileInfo->attrLength1);
          memcpy(
              &key2,
              key,
              fileInfo->attrLength1);
          if (atoi(key1) == atoi(key2))
          {
            AM_errno = AME_AM_KEY_EXISTS_ERROR;
            AM_PrintError("Key Already Exists");
            return AM_errno;
          }
          else if (atoi(key2) > atoi(key1))
          {
            toSecondBock = 1;
          }
          else
          {
            toSecondBock = 0;
          }
          break;
        case 'f':
          memcpy(
              &key1,
              newBlockData + sizeof(struct BlockMetaData),
              fileInfo->attrLength1);
          memcpy(
              &key2,
              key,
              fileInfo->attrLength1);
          if (atof(key1) == atof(key2))
          {
            AM_errno = AME_AM_KEY_EXISTS_ERROR;
            AM_PrintError("Key Already Exists");
            return AM_errno;
          }
          else if (atof(key2) > atof(key1))
          {
            toSecondBock = 1;
          }
          else
          {
            toSecondBock = 0;
          }
          break;
        }
      }
      if (toSecondBock)
      {
        memcpy(
            newBlockData + (halfEntries * (fileInfo->attrLength1 + sizeof(int))) + fileInfo->attrLength1,
            &pointer,
            sizeof(int));
        memcpy(
            newBlockData + (halfEntries * (fileInfo->attrLength1 + sizeof(int))),
            &key,
            fileInfo->attrLength1);
      }
      else
      {
        memcpy(
            blockData + (halfEntries * (fileInfo->attrLength1 + sizeof(int))) + fileInfo->attrLength1,
            &pointer,
            sizeof(int));
        memcpy(
            blockData + (halfEntries * (fileInfo->attrLength1 + sizeof(int))),
            &key,
            fileInfo->attrLength1);
      }
      //if its not root
      if (newBlockMetaData->parrent != -1)
      {
        char newKey[255];
        char oldKey[255];
        memcpy(
            &oldKey,
            blockData + sizeof(struct BlockMetaData) + sizeof(int),
            fileInfo->attrLength1);
        SortBlockContent(block, fileDesc);
        memcpy(
            &newKey,
            blockData + sizeof(struct BlockMetaData) + sizeof(int),
            fileInfo->attrLength1);
        ReplaceInParrent(fileDesc, blockMetaData->parrent, newKey, oldKey);

        SortBlockContent(newBlock, fileDesc);
        BF_Block_SetDirty(block);
        BF_Block_SetDirty(newBlock);
        errorCode = BF_UnpinBlock(block); // We dont need this block anymore
        if (errorCode)
        {
          AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
          AM_PrintError("Unpinning block failed");
          return AM_errno;
        }
        errorCode = BF_UnpinBlock(newBlock); // We dont need this block anymore
        if (errorCode)
        {
          AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
          AM_PrintError("Unpinning block failed");
          return AM_errno;
        }
        WriteToBlock(newBlockMetaData->parrent, fileDesc, newBlockMetaData->blockNumber, key1, NULL);
      }
      //in case its root
      else
      {
        int newRootBlockNum = CreateBlock(Index, fileDesc, -1, -1, -1);
        blockMetaData->parrent = newRootBlockNum;
        newBlockMetaData->parrent = newRootBlockNum;
        BF_Block *newRootBlock;
        char *newRootBlockData;
        BF_Block_Init(&newRootBlock);
        errorCode = BF_GetBlock(fileDesc, newRootBlockNum, newRootBlock); //Trying to get first block
        if (errorCode)
        {
          AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
          AM_PrintError("Failed to get the block");
          return AM_errno;
        }
        newRootBlockData = BF_Block_GetData(newRootBlock); // Reading the content
        struct BlockMetaData *newRootBlockMetaData = (struct BlockMetaData *)newRootBlockData;
        newRootBlockMetaData->existingEntries = 2;
        WriteToBlock(newRootBlockNum, fileDesc, blockMetaData->blockNumber, key1, NULL);
        WriteToBlock(newRootBlockNum, fileDesc, newBlockMetaData->blockNumber, key2, NULL);
        BF_Block_SetDirty(newRootBlock);
        errorCode = BF_UnpinBlock(newRootBlock); // We dont need this block anymore
        if (errorCode)
        {
          AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
          AM_PrintError("Unpinning block failed in Write to block new root case");
          return AM_errno;
        }
        UpdateRoot(fileDesc, newRootBlockNum);
      }

      BF_Block_SetDirty(block);
      BF_Block_SetDirty(newBlock);
      errorCode = BF_UnpinBlock(block); // We dont need this block anymore
      if (errorCode)
      {
        AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
        AM_PrintError("Unpinning block failed");
        return AM_errno;
      }
      errorCode = BF_UnpinBlock(newBlock); // We dont need this block anymore
      if (errorCode)
      {
        AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
        AM_PrintError("Unpinning block failed");
        return AM_errno;
      }
    }
  }
  //logic in case the block is not full
  else
  {
    if (blockMetaData->isDataBlock)
    {
      memcpy(
          blockData + sizeof(struct BlockMetaData) + blockMetaData->existingEntries * (fileInfo->attrLength1 + fileInfo->attrLength2),
          &key,
          fileInfo->attrLength1);
      memcpy(
          blockData + sizeof(struct BlockMetaData) + (blockMetaData->existingEntries * (fileInfo->attrLength1 + fileInfo->attrLength2)) + fileInfo->attrLength1,
          &entry,
          fileInfo->attrLength2);
    }
    else
    {
      memcpy(
          blockData + sizeof(struct BlockMetaData) + blockMetaData->existingEntries * (sizeof(int) + fileInfo->attrLength1),
          &pointer,
          sizeof(int));
      memcpy(
          blockData + sizeof(struct BlockMetaData) + (blockMetaData->existingEntries * (sizeof(int) + fileInfo->attrLength1)) + sizeof(int),
          &key,
          fileInfo->attrLength1);
    }
    SortBlockContent(block, fileDesc);
    BF_Block_SetDirty(block);
    errorCode = BF_UnpinBlock(block); // We dont need this block anymore
    if (errorCode)
    {
      AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
      AM_PrintError("Unpinning block failed");
      return AM_errno;
    }
  }
  return AME_OK;
}
//================================================================================================
void GetFileInfo(int fileDesc, struct OpenedFile *fileInfo)
{
  //serach and returns file Info
  int i;
  for (i = 0; i < MAXOPENEDFILES; i++)
  {
    if (OpenedFileArray[i].fileDesc == fileDesc)
    {
      fileInfo = &OpenedFileArray[i];
      return;
    }
    else
    {
      AM_errno = AME_FILE_NOT_EXISTS_ERROR;
      AM_PrintError("The file is not opened in GetFileInfo function");
      return;
    }
  }
}
//================================================================================================
void SortBlockContent(void *block, int fileDesc)
{
  //sorts the block entries with the bubble sort algorithm
  int errorCode;
  BF_Block *blockToSort = (BF_Block *)block;
  char *blockData = BF_Block_GetData(block);
  struct BlockMetaData *blockMetaData = (struct BlockMetaData *)blockData;
  struct OpenedFile *fileInfo;
  GetFileInfo(fileDesc, fileInfo);

  int x, y;
  char key1[255];
  char key2[255];
  char value1[255];
  char value2[255];

  if (blockMetaData->isDataBlock)
  {
    for (int x = 0; x < blockMetaData->existingEntries; x++)
    {
      for (int y = 0; y < blockMetaData->existingEntries - 1; y++)
      {
        if (blockMetaData->isDataBlock)
        {
          switch (fileInfo->attrType1)
          {
            {
            case 'c':
              memcpy(
                  &key1,
                  blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &key2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &value1,
                  blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                  fileInfo->attrLength2);
              memcpy(
                  &value2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                  fileInfo->attrLength2);
              if (strcmp(key1, key2) > 0)
              {
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                    &key2,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                    &key1,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                    &value2,
                    fileInfo->attrLength2);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                    &value1,
                    fileInfo->attrLength2);
              }
              break;
            case 'i':
              memcpy(
                  &key1,
                  blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &key2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &value1,
                  blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                  fileInfo->attrLength2);
              memcpy(
                  &value2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                  fileInfo->attrLength2);
              if (atoi(key1) > atoi(key2))
              {
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                    &key2,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                    &key1,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                    &value2,
                    fileInfo->attrLength2);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                    &value1,
                    fileInfo->attrLength2);
              }
              break;
            case 'f':
              memcpy(
                  &key1,
                  blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &key2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &value1,
                  blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                  fileInfo->attrLength2);
              memcpy(
                  &value2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                  fileInfo->attrLength2);
              if (atof(key1) > atof(key2))
              {
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                    &key2,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + fileInfo->attrLength1)),
                    &key1,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                    &value2,
                    fileInfo->attrLength2);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (fileInfo->attrLength2 + (2 * fileInfo->attrLength1))),
                    &value1,
                    fileInfo->attrLength2);
              }
              break;
            }
          }
        }
        else
        {
          switch (fileInfo->attrType1)
          {
            {
            case 'c':
              memcpy(
                  &key1,
                  blockData + sizeof(struct BlockMetaData) + (y * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &key2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &value1,
                  blockData + sizeof(struct BlockMetaData) + (y * (sizeof(int) + fileInfo->attrLength1)),
                  sizeof(int));
              memcpy(
                  &value2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (sizeof(int) + fileInfo->attrLength1)),
                  sizeof(int));
              if (strcmp(key1, key2) > 0)
              {
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                    &key2,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                    &key1,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (sizeof(int) + fileInfo->attrLength1)),
                    &value2,
                    sizeof(int));
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (sizeof(int) + fileInfo->attrLength1)),
                    &value2,
                    sizeof(int));
              }
              break;
            case 'i':
              memcpy(
                  &key1,
                  blockData + sizeof(struct BlockMetaData) + (y * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &key2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                  fileInfo->attrLength1);
              memcpy(
                  &value1,
                  blockData + sizeof(struct BlockMetaData) + (y * (sizeof(int) + fileInfo->attrLength1)),
                  sizeof(int));
              memcpy(
                  &value2,
                  blockData + sizeof(struct BlockMetaData) + ((y + 1) * (sizeof(int) + fileInfo->attrLength1)),
                  sizeof(int));
              if (atof(key1) > atof(key2))
              {
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                    &key2,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                    &key1,
                    fileInfo->attrLength1);
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + (y * (sizeof(int) + fileInfo->attrLength1)),
                    &value2,
                    sizeof(int));
                memcpy(
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (sizeof(int) + fileInfo->attrLength1)),
                    &value2,
                    sizeof(int));
                break;
              case 'f':
                memcpy(
                    &key1,
                    blockData + sizeof(struct BlockMetaData) + (y * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                    fileInfo->attrLength1);
                memcpy(
                    &key2,
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                    fileInfo->attrLength1);
                memcpy(
                    &value1,
                    blockData + sizeof(struct BlockMetaData) + (y * (sizeof(int) + fileInfo->attrLength1)),
                    sizeof(int));
                memcpy(
                    &value2,
                    blockData + sizeof(struct BlockMetaData) + ((y + 1) * (sizeof(int) + fileInfo->attrLength1)),
                    sizeof(int));
                if (atof(key1) > atof(key2))
                {
                  memcpy(
                      blockData + sizeof(struct BlockMetaData) + (y * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                      &key2,
                      fileInfo->attrLength1);
                  memcpy(
                      blockData + sizeof(struct BlockMetaData) + ((y + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                      &key1,
                      fileInfo->attrLength1);
                  memcpy(
                      blockData + sizeof(struct BlockMetaData) + (y * (sizeof(int) + fileInfo->attrLength1)),
                      &value2,
                      sizeof(int));
                  memcpy(
                      blockData + sizeof(struct BlockMetaData) + ((y + 1) * (sizeof(int) + fileInfo->attrLength1)),
                      &value2,
                      sizeof(int));
                  break;
                }
              }
            }
          }
        }
      }
    }
  }
}

void ReplaceInParrent(int fileDesc, int parrent, void *newKey, void *oldKey)
{
  //updates the changed key in child to parrent
  int errorCode, i;
  BF_Block *block;
  char *blockData; // Current block data
  struct BlockMetaData *blockMetaData;
  struct OpenedFile *fileInfo;
  GetFileInfo(fileDesc, fileInfo);
  BF_Block_Init(&block);                             // Initializing block
  errorCode = BF_GetBlock(fileDesc, parrent, block); //Trying to get first block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
    AM_PrintError("Failed to get the block in ReplaceInParrent function");
  }
  blockData = BF_Block_GetData(block); // Reading the content
  blockMetaData = (struct BlockMetaData *)blockData;
  char key1[255];
  char key2[255];
  int flag = 0;
  for (i = 0; i < blockMetaData->existingEntries; i++)
  {
    switch (fileInfo->attrType1)
    {
      {
      case 'c':
        memcpy(
            &key1,
            blockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
            fileInfo->attrLength1);
        memcpy(
            &key2,
            oldKey,
            fileInfo->attrLength1);
        if (strcmp(key1, key2))
        {
          memcpy(
              blockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              &newKey,
              fileInfo->attrLength1);
          flag = 1;
        }
        break;
      case 'i':
        memcpy(
            &key1,
            blockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
            fileInfo->attrLength1);
        memcpy(
            &key2,
            oldKey,
            fileInfo->attrLength1);
        if (atoi(key1) == atoi(key2))
        {
          memcpy(
              blockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              &newKey,
              fileInfo->attrLength1);
          flag = 1;
        }
        break;
      case 'f':
        memcpy(
            &key1,
            blockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
            fileInfo->attrLength1);
        memcpy(
            &key2,
            oldKey,
            fileInfo->attrLength1);
        if (atof(key1) == atof(key2))
        {
          memcpy(
              blockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              &newKey,
              fileInfo->attrLength1);
          flag = 1;
        }
        break;
      }
    }
    if (flag)
    {
      BF_Block_SetDirty(block);
      errorCode = BF_UnpinBlock(block); // We dont need this block anymore
      if (errorCode)
      {
        AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
        AM_PrintError("Unpinning block failed in ReplaceInParrent Function");
      }
      return;
    }
  }
}

void UpdateRoot(int fileDesc, int newRootNumber)
{
  //updates root info in File info block and opened file block
  struct OpenedFile *fileInfo;
  GetFileInfo(fileDesc, fileInfo);
  fileInfo->rootBlock = newRootNumber;

  int errorCode;
  //----------------------Getting root block--------------------------
  BF_Block *block;
  char *blockData; // Current block data
  struct FileMetaData *fileMetaData;
  struct BlockMetaData *blockMetaData;
  BF_Block_Init(&block);                       // Initializing block
  errorCode = BF_GetBlock(fileDesc, 0, block); //Trying to get first block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
    AM_PrintError("Failed to get the 0 block");
    return;
  }
  blockData = BF_Block_GetData(block); // Reading the content
  fileMetaData = (struct FileMetaData *)blockData;
  fileMetaData->rootBlock = newRootNumber;
  BF_Block_SetDirty(block);
  errorCode = BF_UnpinBlock(block); // We dont need this block anymore
  if (errorCode)
  {
    AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
    AM_PrintError("Unpinning block failed in ReplaceInParrent Function");
  }
}

int SearchEntry(void *keyToFind, int fileDesc, int *returnBlockNumber, int *returnEntryNumber)
{
  //updates root info in File info block and opened file block

  int errorCode, i, blockNumber;
  //----------------------Getting root block--------------------------
  BF_Block *currentBlock;
  char *currentBlockData; // Current block data
  struct FileMetaData *fileMetaData;
  struct BlockMetaData *currentBlockMetaData;
  struct OpenedFile *fileInfo;
  GetFileInfo(fileDesc, fileInfo);
  BF_Block_Init(&currentBlock);                       // Initializing block
  errorCode = BF_GetBlock(fileDesc, 0, currentBlock); //Trying to get first block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
    AM_PrintError("Failed to get the 0 block");
    return AM_errno;
  }
  currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
  fileMetaData = (struct FileMetaData *)currentBlockData;
  blockNumber = fileMetaData->rootBlock;
  errorCode = BF_GetBlock(fileDesc, blockNumber, currentBlock); //Trying to get first block
  if (errorCode)
  {
    AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
    AM_PrintError("Failed to get the 0 block");
    return AM_errno;
  }
  currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
  currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
  char key1[255];
  memcpy(
      &key1,
      keyToFind,
      fileInfo->attrLength1);
  char key2[255];
  char key3[255];
  int pointer;
  int found = 0;
  while (!currentBlockMetaData->isDataBlock) //searching for the block we need to to add the entry
  {
    for (i = 0; i < currentBlockMetaData->existingEntries; i++)
    {
      switch (fileInfo->attrType1)
      {
        {
        case 'c':
          memcpy(
              &key2,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (2 * (sizeof(int)) + fileInfo->attrLength1)),
              fileInfo->attrLength1);
          if (i + 1 < currentBlockMetaData->existingEntries)
          {
            memcpy(
                &key3,
                currentBlockData + sizeof(struct BlockMetaData) + ((i + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                fileInfo->attrLength1);
          }
          memcpy(
              &pointer,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              sizeof(int));
          if (strcmp(key1, key2) == 0 || (strcmp(key1, key2) > 0 && strcmp(key1, key3) < 0))
          {
            found = 1;
            *returnBlockNumber = i;
          }
          break;
        case 'i':
          memcpy(
              &key2,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (2 * (sizeof(int)) + fileInfo->attrLength1)),
              fileInfo->attrLength1);
          if (i + 1 < currentBlockMetaData->existingEntries)
          {
            memcpy(
                &key3,
                currentBlockData + sizeof(struct BlockMetaData) + ((i + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                fileInfo->attrLength1);
          }
          memcpy(
              &pointer,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              sizeof(int));
          if (atoi(key1) == atoi(key2) || atoi(key1) == atoi(key2) > 0 && atoi(key1) == atoi(key2))
          {
            found = 1;
            *returnBlockNumber = i;
          }
          break;
        case 'f':
          memcpy(
              &key2,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (2 * (sizeof(int)) + fileInfo->attrLength1)),
              fileInfo->attrLength1);
          if (i + 1 < currentBlockMetaData->existingEntries)
          {
            memcpy(
                &key3,
                currentBlockData + sizeof(struct BlockMetaData) + ((i + 1) * (2 * (sizeof(int)) + fileInfo->attrLength1)),
                fileInfo->attrLength1);
          }
          memcpy(
              &pointer,
              currentBlockData + sizeof(struct BlockMetaData) + (i * (sizeof(int) + fileInfo->attrLength1)),
              sizeof(int));
          if (atof(key1) == atof(key2) || atof(key1) == atof(key2) > 0 && atof(key1) == atof(key2))
          {
            found = 1;
            *returnBlockNumber = i;
          }
          break;
        }
      }
      if (found)
      {
        errorCode = BF_GetBlock(fileDesc, pointer, currentBlock); //Trying to get first block
        if (errorCode)
        {
          AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
          AM_PrintError("Failed to get the 0 block");
          return AM_errno;
        }
        currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
        currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
      }
      break;
    }
    errorCode = BF_GetBlock(fileDesc, pointer, currentBlock); //Trying to get first block
    if (errorCode)
    {
      AM_errno = AME_BLOCK_GETTING_FAILED_ERROR;
      AM_PrintError("Failed to get the 0 block");
      return AM_errno;
    }
    currentBlockData = BF_Block_GetData(currentBlock); // Reading the content
    currentBlockMetaData = (struct BlockMetaData *)currentBlockData;
  }
  // if we are here that means we found the needed data block
  *returnBlockNumber = currentBlockMetaData->blockNumber;

  int finalBlockNum = currentBlockMetaData->blockNumber;
  errorCode = BF_UnpinBlock(currentBlock); // Trying to unpin
  if (errorCode)
  {
    AM_errno = AME_BLOCK_UNPIN_FAILED_ERROR;
    AM_PrintError("Failed to unpin block");
    return AM_errno;
  }
}

//================================================================================================
//                               HELPER FUNCTIONS END
//================================================================================================
