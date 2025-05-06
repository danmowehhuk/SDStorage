#ifndef _SDStorage_TransactionManager_h
#define _SDStorage_TransactionManager_h


#include "../Index.h"
#include "FileHelper.h"
#include "StorageProvider.h"
#include "Strings.h"
#include "Transaction.h"

// forward declared
class SDStorage;

static const char _SDSTORAGE_TOMBSTONE[]         PROGMEM = "{TOMBSTONE}";

class TransactionManager {

  public:
    TransactionManager(FileHelper* fileHelper, StorageProvider* storageProvider, void (*errFunction)() = nullptr):
        _fileHelper(fileHelper), _storageProvider(storageProvider), _errFunction(errFunction) {};

    // Disable moving and copying
    TransactionManager(TransactionManager&& other) = delete;
    TransactionManager& operator=(TransactionManager&& other) = delete;
    TransactionManager(const TransactionManager&) = delete;
    TransactionManager& operator=(const TransactionManager&) = delete;


  private:
    void (*_errFunction)();
    FileHelper* _fileHelper;
    StorageProvider* _storageProvider;

    /*
     * Create a new transaction, locking the affected files
     */
    template <typename... Args>
    Transaction* beginTxn(const char* filename, Args... moreFilenames);
    template <typename... Args>
    Transaction* beginTxn(const __FlashStringHelper* filename, Args... moreFilenames);
    template <typename... Args>
    Transaction* beginTxn(sdstorage::Index idx, Args... moreFilenames);
    template <typename... Args>
    Transaction* beginTxn(void* testState, const char* filename, Args... moreFilenames);
    template <typename... Args>
    Transaction* beginTxn(void* testState, const __FlashStringHelper* filename, Args... moreFilenames);
    template <typename... Args>
    Transaction* beginTxn(void* testState, sdstorage::Index idx, Args... moreFilenames);

    /*
     * Applies the transaction's changes and unlocks the files.
     */
    bool commitTxn(Transaction* txn, void* testState = nullptr);

    /*
     * Cancels all the transaction's changes and unlocks the files.
     */
    bool abortTxn(Transaction* txn, void* testState = nullptr);

    /*
     * Recursive transaction helper methods to handle the variadic 'moreFilenames' argument
     */
    template <typename... Args>
    bool _beginTxn(Transaction* txn, void* testState, const char* filename, Args... moreFilenames);
    template <typename... Args>
    bool _beginTxn(Transaction* txn, void* testState, const __FlashStringHelper* filename, Args... moreFilenames);
    template <typename... Args>
    bool _beginTxn(Transaction* txn, void* testState, sdstorage::Index idx, Args... moreFilenames);
    bool _beginTxn(Transaction* txn, void* testState) { return true; }; // termination case

    /*
     * Transaction helper methods
     */
    bool addFileToTxn(Transaction* txn, void* testState, const char* filename, bool isPmem = false);
    char* getTmpFilename(Transaction* txn, const char* filename, bool isPmem = false);
    void cleanupTxn(Transaction* txn, void* testState = nullptr);
    bool applyChanges(Transaction* txn, void* testState = nullptr);
    bool finalizeTxn(Transaction* txn, bool autoCommit, bool success, void* testState);

    // For passing data in/out of Transaction-related lambda functions
    struct TransactionCapture {
      StorageProvider* storageProvider;
      void* ts;
      bool success = true;
      TransactionCapture(StorageProvider* storageProvider, void* testState):
          storageProvider(storageProvider), ts(testState) {};
    };

    friend class SDStorage;
    friend class IndexManager;

};

using namespace SDStorageStrings;

template <typename... Args>
Transaction* TransactionManager::beginTxn(const char* filename, Args... moreFilenames) {
  return beginTxn(nullptr, filename, moreFilenames...);
}

template <typename... Args>
Transaction* TransactionManager::beginTxn(const __FlashStringHelper* filename, Args... moreFilenames) {
  return beginTxn(nullptr, filename, moreFilenames...);
}

template <typename... Args>
Transaction* TransactionManager::beginTxn(sdstorage::Index idx, Args... moreFilenames) {
  return beginTxn(nullptr, idx, moreFilenames...);
}

template <typename... Args>
Transaction* TransactionManager::beginTxn(void* testState, const char* filename, Args... moreFilenames) {
  Transaction* txn = new Transaction(_fileHelper);
  bool success = false;
  do {
    if (!_beginTxn(txn, testState, filename, moreFilenames...)) break;
    char txnFilename[FileHelper::MAX_FILENAME_LENGTH];
    if (!txn->getFilename(txnFilename, FileHelper::MAX_FILENAME_LENGTH)) break;
    if (!_storageProvider->_writeTxnToStream(txnFilename, txn, testState)) break;
    success = true;
  } while (false);
  if (!success) {
    delete txn;
    txn = nullptr;
  }
  return txn;
}

template <typename... Args>
Transaction* TransactionManager::beginTxn(void* testState, const __FlashStringHelper* filename, Args... moreFilenames) {
  char* filenameRAM = strdup(filename);
  Transaction* txn = beginTxn(testState, filenameRAM, moreFilenames...);
  free(filenameRAM);
  return txn;
}

template <typename... Args>
Transaction* TransactionManager::beginTxn(void* testState, sdstorage::Index idx, Args... moreFilenames) {
  char filenameRAM[FileHelper::MAX_FILENAME_LENGTH];
  _fileHelper->indexFilename(idx, filenameRAM, FileHelper::MAX_FILENAME_LENGTH);
  Transaction* txn = beginTxn(testState, filenameRAM, moreFilenames...);
  return txn;
}

template <typename... Args>
bool TransactionManager::_beginTxn(Transaction* txn, void* testState, const char* filename, Args... moreFilenames) {
  if (!addFileToTxn(txn, testState, filename)) return false;
  return _beginTxn(txn, testState, moreFilenames...);
}

template <typename... Args>
bool TransactionManager::_beginTxn(Transaction* txn, void* testState, const __FlashStringHelper* filename, Args... moreFilenames) {
  char* filenameRAM = strdup(filename);
  bool result = _beginTxn(txn, testState, filenameRAM, moreFilenames...);
  free(filenameRAM);
  return result;
}

template <typename... Args>
bool TransactionManager::_beginTxn(Transaction* txn, void* testState, sdstorage::Index idx, Args... moreFilenames) {
  if (!idx.name) {
#if (defined(DEBUG))
    Serial.println(F("TransactionManager::beginTxn - index name cannot be empty"));
#endif
    return false;
  }
  char filenameRAM[FileHelper::MAX_FILENAME_LENGTH];
  _fileHelper->indexFilename(idx, filenameRAM, FileHelper::MAX_FILENAME_LENGTH);
  bool result = _beginTxn(txn, testState, filenameRAM, moreFilenames...);
  return result;
}


#endif