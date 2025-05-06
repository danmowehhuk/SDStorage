#ifndef _SDStorage_IndexManager_h
#define _SDStorage_IndexManager_h


#include "../Index.h"
#include "FileHelper.h"
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

    bool idxUpsert(Index idx, IndexEntry* entry, Transaction* txn = nullptr);
    bool idxUpsert(void* testState, Index idx, IndexEntry* entry, Transaction* txn = nullptr);

    friend class IndexScanFilters;
    friend class SDStorage;
    friend class SDStorageTestHelper;

};


#endif