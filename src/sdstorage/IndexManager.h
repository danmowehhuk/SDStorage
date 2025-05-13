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

    // Creates an implicit txn if the one passed in is nullptr
    IndexTransaction _makeIndexTransaction(void* testState, Index idx, Transaction* txn);

    friend class IndexScanFilters;
    friend class SDStorage;
    friend class SDStorageTestHelper;

};


#endif