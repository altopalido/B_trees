#ifndef AM_H_
#define AM_H_

/* Error codes */

extern int AM_errno;

#define AME_OK 0
#define AME_EOF -1

#define EQUAL 1
#define NOT_EQUAL 2
#define LESS_THAN 3
#define GREATER_THAN 4
#define LESS_THAN_OR_EQUAL 5
#define GREATER_THAN_OR_EQUAL 6
#define MAX_NAME_SIZE 50

//================================================================================================
//                            Implimentation of enums starts
//================================================================================================
typedef enum AM_ErrorCode {
    AM_OK,
    // file errors
    AME_FILE_OPENED_ERROR,            // in case file is already opened
    AME_FILE_CREATE_FAILED_ERROR,     // in case file creation failed
    AME_FILE_OPEN_FAILED_ERROR,       // in case file opening failed
    AME_FILE_CLOSE_FAILED_ERROR,      // in case file closing failed
    AME_FILE_NOT_EXISTS_ERROR,        // in case file does not exists
    AME_FILE_SCAN_FAILED_FIND_ERROR,  // in case scan failed to find the file
    AME_FIND_NEXT_ENTRY_FAILED_ERRON, // in case failed to find next entry
    // block errors
    AME_BLOCK_ALOCATION_FAILED_ERROR,     // in case file opening failed
    AME_BLOCK_GETTING_FAILED_ERROR,       // in case file opening failed
    AME_BLOCK_UNPIN_FAILED_ERROR,         // in case unpin block failed
    AME_BLOCK_COUNT_GETTING_FAILED_ERROR, // in case unable to get block count
    AME_BLOCK_CREATION_FAILED_ERROR,      // in case unable to get block count
    // AM errors
    AME_AM_DISPOSE_ERROR,   // in case disposing of AM fails
    AME_AM_KEY_EXISTS_ERROR // in case key already exists
} AM_ErrorCode;

typedef enum BlockType {
    Index,
    Data
} BlockType;
//================================================================================================
//                            Implimentation of enums starts
//================================================================================================

//================================================================================================
//                             Implimentation structs starts
//================================================================================================
struct OpenedFile
{
    char fileName[MAX_NAME_SIZE];
    int fileDesc;
    char attrType1;
    int attrLength1;
    char attrType2;
    int attrLength2;
    int rootBlock;
    int entriesIndexBlockCanContain;
    int entriesDataBlockCanContain;
};

struct OpenedScan
{
    int fileDesc;
    int startBlock;
    int endBlock;
    int startEntry;
    int endEntry;
    char attrType1;
    int attrLength1;
    char attrType2;
    int attrLength2;
    int nextBlock;
    int nextEntry;
    int neq;
    int target;
    char currentValue[255];
};

struct FileMetaData
{
    int identifier; // 777 four our data struct
    int rootBlock;
    char attrType1;
    int attrLength1;
    char attrType2;
    int attrLength2;
    int entriesIndexBlockCanContain;
    int entriesDataBlockCanContain;
};

struct BlockMetaData
{
    int isDataBlock;
    int blockNumber;
    int maxEntries;
    int existingEntries;
    int parrent;
    int nextBlock;
    int previusBlock;
};
//================================================================================================
//                                Implimentation structs ends
//================================================================================================

//================================================================================================
//                            Implimentation Helper Functions starts
//================================================================================================
int CreateBlock(int blockType, int fileDesc, int previus, int next, int parrent);               // Returns the block number of the created new block
int WriteToBlock(int blockNumber, int fileDesc, int pointer, void *key, void *entry);           // Writes entry to block
void GetFileInfo(int fileDesc, struct OpenedFile *fileInfo);                                    // Returns file info struct from opened file array
void SortBlockContent(void *block, int fileDesc);                                               // Sorts Block content
void ReplaceInParrent(int fileDesc, int parrent, void *newKey, void *oldKey);                   // Replacing Key In Parrent
void UpdateRoot(int fileDesc, int newRootNumber);                                               // Updates Root In File metadata and opened file array
int SearchEntry(void *keyToFind, int fileDesc, int *returnBlockNumber, int *returnEntryNumber); // finds the block number and the entry number of a spesific key
//======================================================;==========================================
//                            Implimentation Helper Functions ends
//================================================================================================

void AM_Init(void);

int AM_CreateIndex(
    char *fileName,  /* όνομα αρχείου */
    char attrType1,  /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
    int attrLength1, /* μήκος πρώτου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
    char attrType2,  /* τύπος πρώτου πεδίου: 'c' (συμβολοσειρά), 'i' (ακέραιος), 'f' (πραγματικός) */
    int attrLength2  /* μήκος δεύτερου πεδίου: 4 γιά 'i' ή 'f', 1-255 γιά 'c' */
);

int AM_DestroyIndex(
    char *fileName /* όνομα αρχείου */
);

int AM_OpenIndex(
    char *fileName /* όνομα αρχείου */
);

int AM_CloseIndex(
    int fileDesc /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
);

int AM_InsertEntry(
    int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
    void *value1, /* τιμή του πεδίου-κλειδιού προς εισαγωγή */
    void *value2  /* τιμή του δεύτερου πεδίου της εγγραφής προς εισαγωγή */
);

int AM_OpenIndexScan(
    int fileDesc, /* αριθμός που αντιστοιχεί στο ανοιχτό αρχείο */
    int op,       /* τελεστής σύγκρισης */
    void *value   /* τιμή του πεδίου-κλειδιού προς σύγκριση */
);

void *AM_FindNextEntry(
    int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);

int AM_CloseIndexScan(
    int scanDesc /* αριθμός που αντιστοιχεί στην ανοιχτή σάρωση */
);

void AM_PrintError(
    char *errString /* κείμενο για εκτύπωση */
);

void AM_Close();

#endif /* AM_H_ */
