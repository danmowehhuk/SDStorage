/*

  SDStorage.h

  SD card storage manager for StreamableDTOs with index and transaction support

  Copyright (c) 2025, Dan Mowehhuk (danmowehhuk@gmail.com)
  All rights reserved.

*/

#ifndef _SDStorage_h
#define _SDStorage_h


#include "Index.h"
#include <StreamableDTO.h>
#include <StreamableManager.h>
#include "sdstorage/FileHelper.h"
#include "sdstorage/IndexManager.h"
#include "sdstorage/Transaction.h"
#include "sdstorage/TransactionManager.h"
#include "sdstorage/StorageProvider.h"
#include "sdstorage/Strings.h"

class SDStorage {

  public:
    /*
     * sdCsPin - SPI chip select pin for SD card
     * rootDir - A base directory for all SDStorage operations; e.g. "/ARDUINO"
     * isRootDirPmem - does rootDir point to PROGMEM?
     * errFunction - A function to call when an unrecoverable error occurs, so 
     *        the caller may display a message and/or halt the system. An 
     *        example of an unrecoverable error would be if a transaction
     *        commit or abort fails for some reason (like the card was yanked)
     *        and to continue would leave files in an inconsistent state.
     *
     * Note that if a transaction is not completed because of power loss,
     * it will be cleaned up the next time begin() is called, which will perform
     * a cleanup of the work directory, applying any finalized transactions
     * and rolling back any others.
     */
    SDStorage(uint8_t sdCsPin, const char* rootDir, bool isRootDirPmem = false, void (*errFunction)() = nullptr): 
          _fileHelper(rootDir, isRootDirPmem), _storageProvider(sdCsPin), _errFunction(errFunction) {
        _txnManager = new TransactionManager(&_fileHelper, &_storageProvider, _errFunction);
        _idxManager = new IndexManager(&_fileHelper, &_storageProvider, _txnManager);
    };
    SDStorage(uint8_t sdCsPin, const char* rootDir, void (*errFunction)() = nullptr): 
          SDStorage(sdCsPin, rootDir, false, errFunction) {};
    SDStorage(uint8_t sdCsPin, const __FlashStringHelper* rootDir, void (*errFunction)() = nullptr):
          SDStorage(sdCsPin, reinterpret_cast<const char*>(rootDir), true, errFunction) {};
    SDStorage() = delete;
    ~SDStorage() {
      if (_txnManager) delete _txnManager;
      if (_idxManager) delete _idxManager;
    }

    // Disable moving and copying
    SDStorage(SDStorage&& other) = delete;
    SDStorage& operator=(SDStorage&& other) = delete;
    SDStorage(const SDStorage&) = delete;
    SDStorage& operator=(const SDStorage&) = delete;

    /*
     * Initialize the SDStorage system, creating the root directory and 
     * other working directories if they don't exist. Also checks the 
     * file system state and cleans up any incomplete transactions. Returns
     * false if any of this fails.
     */
    bool begin(void* testState = nullptr);

    /*
     * FILE OPERATIONS
     *
     * The following prepend the path with the root directory if necessary
     */
    bool mkdir(const char* dirName, bool isDirNamePmem = false, void* testState = nullptr);
    bool mkdir(const __FlashStringHelper* dirName, void* testState = nullptr);
    bool mkdir_P(const char* dirName, void* testState = nullptr);
    bool load(const char* filename, StreamableDTO* dto, bool isFilenamePmem = false, void* testState = nullptr);
    bool load(const __FlashStringHelper* filename, StreamableDTO* dto, void* testState = nullptr);
    bool save(const char* filename, StreamableDTO* dto, Transaction* txn = nullptr, bool isFilenamePmem = false);
    bool save(void* testState, const char* filename, StreamableDTO* dto, Transaction* txn = nullptr, bool isFilenamePmem = false);
    bool save(const __FlashStringHelper* filename, StreamableDTO* dto, Transaction* txn = nullptr);
    bool save(void* testState, const __FlashStringHelper* filename, StreamableDTO* dto, Transaction* txn = nullptr);
    bool exists(const char* filename, bool isFilenamePmem = false, void* testState = nullptr);
    bool exists(const __FlashStringHelper* filename, void* testState = nullptr);
    bool exists_P(const char* filename, void* testState = nullptr);
    bool erase(const char* filename, bool isFilenamePmem = false, Transaction* txn = nullptr);
    bool erase(const __FlashStringHelper* filename, Transaction* txn = nullptr);
    bool erase_P(const char* filename, Transaction* txn = nullptr);
    bool erase(void* testState, const char* filename, bool isFilenamePmem = false, Transaction* txn = nullptr);
    bool erase(void* testState, const __FlashStringHelper* filename, Transaction* txn = nullptr);
    bool erase_P(void* testState, const char* filename, Transaction* txn = nullptr);

    /*
     * INDEX OPERATIONS
     * 
     */
    bool idxUpsert(Index idx, IndexEntry* entry, Transaction* txn = nullptr) {
      return _idxManager->idxUpsert(idx, entry, txn);
    };
    bool idxUpsert(void* testState, Index idx, IndexEntry* entry, Transaction* txn = nullptr) {
      return _idxManager->idxUpsert(testState, idx, entry, txn);
    };
    bool idxRemove(Index idx, const char* key, Transaction* txn = nullptr) {
      return _idxManager->idxRemove(idx, key, txn);
    };
    bool idxRemove(void* testState, Index idx, const char* key, Transaction* txn = nullptr) {
      return _idxManager->idxRemove(testState, idx, key, txn);
    };
    bool idxRemove(Index idx, const __FlashStringHelper* key, Transaction* txn = nullptr) {
      char* ramKey = strdup(key);
      bool result = _idxManager->idxRemove(idx, ramKey, txn);
      free(ramKey);
      return result;
    };
    bool idxRemove(void* testState, Index idx, const __FlashStringHelper* key, Transaction* txn = nullptr) {
      char* ramKey = strdup(key);
      bool result = _idxManager->idxRemove(testState, idx, ramKey, txn);
      free(ramKey);
      return result;
    };
    bool idxRename(Index idx, const char* oldKey, const char* newKey, Transaction* txn = nullptr) {
      return _idxManager->idxRename(idx, oldKey, newKey, txn);
    };
    bool idxRename(void* testState, Index idx, const char* oldKey, const char* newKey, Transaction* txn = nullptr) {
      return _idxManager->idxRename(testState, idx, oldKey, newKey, txn);
    };
    bool idxRename(Index idx, const __FlashStringHelper* oldKey, const __FlashStringHelper* newKey, Transaction* txn = nullptr) {
      char* ramOldKey = strdup(oldKey);
      char* ramNewKey = strdup(newKey);
      bool result = _idxManager->idxRename(idx, ramOldKey, ramNewKey, txn);
      free(ramOldKey);
      free(ramNewKey);
      return result;
    };
    bool idxRename(void* testState, Index idx, const __FlashStringHelper* oldKey, const __FlashStringHelper* newKey, Transaction* txn = nullptr) {
      char* ramOldKey = strdup(oldKey);
      char* ramNewKey = strdup(newKey);
      bool result = _idxManager->idxRename(testState, idx, ramOldKey, ramNewKey, txn);
      free(ramOldKey);
      free(ramNewKey);
      return result;
    };
    bool idxLookup(Index idx, const char* key, char* buffer, size_t bufferSize, void* testState = nullptr) {
      return _idxManager->idxLookup(idx, key, buffer, bufferSize, testState);
    };
    bool idxLookup(Index idx, const __FlashStringHelper* key, char* buffer, size_t bufferSize, void* testState = nullptr) {
      char* ramKey = strdup(key);
      bool result = _idxManager->idxLookup(idx, ramKey, buffer, bufferSize, testState);
      free(ramKey);
      return result;
    };
    bool idxHasKey(Index idx, const char* key, void* testState = nullptr) {
      return _idxManager->idxHasKey(idx, key, testState);
    };
    bool idxHasKey(Index idx, const __FlashStringHelper* key, void* testState = nullptr) {
      char* ramKey = strdup(key);
      bool result = _idxManager->idxHasKey(idx, ramKey, testState);
      free(ramKey);
      return result;
    };
    bool idxPrefixSearch(Index idx, SearchResults* results, void* testState = nullptr) {
      return _idxManager->idxPrefixSearch(idx, results, testState);
    };


    /*
     * TRANSACTION OPERATIONS
     *
     * Create a new transaction, locking the affected files. Filenames may be char* or F()-strings
     * or a mixture of both. Indexes may also be provided. All will be converted to absolute canonical
     * filenames, the root directory prepended if necessary.
     */
    template <typename... Args>
    Transaction* beginTxn(const char* filename, Args... moreFilenames) {
      return _txnManager->beginTxn(filename, moreFilenames...);
    };
    template <typename... Args>
    Transaction* beginTxn(const __FlashStringHelper* filename, Args... moreFilenames) {
      return _txnManager->beginTxn(filename, moreFilenames...);
    };
    template <typename... Args>
    Transaction* beginTxn(Index idx, Args... moreFilenames) {
      return _txnManager->beginTxn(idx, moreFilenames...);
    };
    template <typename... Args>
    Transaction* beginTxn(void* testState, const char* filename, Args... moreFilenames) {
      return _txnManager->beginTxn(testState, filename, moreFilenames...);
    };
    template <typename... Args>
    Transaction* beginTxn(void* testState, const __FlashStringHelper* filename, Args... moreFilenames) {
      return _txnManager->beginTxn(testState, filename, moreFilenames...);
    };
    template <typename... Args>
    Transaction* beginTxn(void* testState, Index idx, Args... moreFilenames) {
      return _txnManager->beginTxn(testState, idx, moreFilenames...);
    };

    /*
     * Applies the transaction's changes and unlocks the files.
     */
    bool commitTxn(Transaction* txn, void* testState = nullptr) {
      return _txnManager->commitTxn(txn, testState);
    };

    /*
     * Cancels all the transaction's changes and unlocks the files.
     */
    bool abortTxn(Transaction* txn, void* testState = nullptr) {
      return _txnManager->abortTxn(txn, testState);
    };

  private:

    friend class SDStorageTestHelper;

    void (*_errFunction)() = nullptr;
    FileHelper _fileHelper;
    StorageProvider _storageProvider;
    TransactionManager* _txnManager = nullptr;
    IndexManager* _idxManager = nullptr;

    /*
     * Cleans up the _workDir on initialization in case any transactions were
     * left after a power interruption. Finalized transactions are completed, and
     * all others are rolled back.
     */
    bool fsck();

};


#endif
