#ifndef _SDStorage_IndexManager_h
#define _SDStorage_IndexManager_h


#include "../Index.h"
#include "FileHelper.h"
#include "IndexHelpers.h"
#include "IndexScanFilters.h"
#include "StorageProvider.h"
#include "Strings.h"
#include "Transaction.h"
#include "TransactionManager.h"


using namespace sdstorage;
using namespace SDStorageStrings;

class IndexManager {

  public:
    IndexManager(FileHelper* fileHelper, StorageProvider* storageProvider, TransactionManager* txnManager):
        _fileHelper(fileHelper), _storageProvider(storageProvider), _txnManager(txnManager) {};

    // Disable moving and copying
    IndexManager(IndexManager&& other) = delete;
    IndexManager& operator=(IndexManager&& other) = delete;
    IndexManager(const IndexManager&) = delete;
    IndexManager& operator=(const IndexManager&) = delete;

  private:
    FileHelper* _fileHelper;
    StorageProvider* _storageProvider;
    TransactionManager* _txnManager;

    struct IndexTransaction {
      Index* idx = nullptr;
      Transaction* txn = nullptr;
      char* idxFilename = nullptr;
      char* tmpFilename = nullptr;
      char* readSource = nullptr;  // non-owning - either idxFilename or tmpFilename, never freed here
      bool isImplicitTxn = false;
      bool success = false;
      ~IndexTransaction() {
        if (idxFilename) free(idxFilename);
        idxFilename = nullptr;
      }
    };

    bool idxUpsert(Index idx, IndexEntry* entry, Transaction* txn = nullptr);
    bool idxUpsert(void* testState, Index idx, IndexEntry* entry, Transaction* txn = nullptr);
    bool idxRemove(Index idx, const char* key, Transaction* txn = nullptr);
    bool idxRemove(void* testState, Index idx, const char* key, Transaction* txn = nullptr);
    bool idxRename(Index idx, const char* oldKey, const char* newKey, Transaction* txn = nullptr);
    bool idxRename(void* testState, Index idx, const char* oldKey, const char* newKey, Transaction* txn = nullptr);
    bool idxLookup(Index idx, const char* key, char* buffer, size_t bufferSize, void* testState = nullptr);
    bool idxHasKey(Index idx, const char* key, void* testState = nullptr);
    bool idxPrefixSearch(Index idx, SearchResults* results, void* testState = nullptr);

    // Creates an implicit txn if the one passed in is nullptr
    IndexTransaction _makeIndexTransaction(void* testState, Index idx, Transaction* txn);

    // Derives the scratch filename used to stage a streamed index update:
    // tmpFilename with its ".tmp" extension swapped for ".tm2" (still a
    // valid 8.3 extension). Streaming into a scratch file - never
    // directly into tmpFilename - is what makes it safe for a repeat
    // edit to read tmpFilename (as readSource) and write a new version
    // of it in the same operation.
    static bool _getScratchFilename(const char* tmpFilename, char* buffer, size_t bufferSize);

    // Swaps a completed scratch file into tmpFilename's place: removes
    // the previous tmpFilename if one exists (FAT's rename does not
    // overwrite an existing destination), then renames scratchFilename
    // to tmpFilename.
    bool _swapIntoPlace(const char* scratchFilename, const char* tmpFilename, void* testState);

    // General purpose index scanner
    bool IndexManager::_idxScan(const char* idxFilename, IndexScanFilters::IdxScanCapture* state, void* testState = nullptr);
    
    friend class IndexScanFilters;
    friend class SDStorage;
    friend class SDStorageTestHelper;

};


#endif