#ifndef _SDStorage_IndexManager_h
#define _SDStorage_IndexManager_h


#include "FileHelper.h"
#include "IndexScanFilters.h"
#include "StorageProvider.h"
#include "Strings.h"
#include "Transaction.h"
#include "TransactionManager.h"

// forward declared
class SDStorage;

static const char _SDSTORAGE_INDEX_EXTSN[]       PROGMEM = ".idx";

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

    /*
     * Returns the fully-qualified filename of the given index so it can
     * be added to a Transaction via beginTxn(filenames...)
     * These return dynamically allocated char[]'s that the caller must free
     */
    char* indexFilename(const char* idxName, bool isIdxNamePmem = false);
    char* indexFilename(const __FlashStringHelper* idxName);
    char* indexFilename_P(const char* idxName);


    bool idxUpsert(const char* idxName, const char* key, const char* value, Transaction* txn = nullptr);
    bool idxUpsert(const __FlashStringHelper* idxName, const char* key, const char* value, Transaction* txn = nullptr);
    bool idxUpsert_P(const char* idxName, const char* key, const char* value, Transaction* txn = nullptr);
    bool idxUpsert(void* testState, const char* idxName, const char* key, const char* value, Transaction* txn = nullptr);
    bool idxUpsert(void* testState, const __FlashStringHelper* idxName, const char* key, const char* value, Transaction* txn = nullptr);
    bool idxUpsert_P(void* testState, const char* idxName, const char* key, const char* value, Transaction* txn = nullptr);

    friend class IndexScanFilters;
    friend class SDStorage;
    friend class SDStorageTestHelper;

};


#endif