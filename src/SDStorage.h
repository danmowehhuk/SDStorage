/*

  SDStorage.h

  SD card storage manager for StreamableDTOs with index and transaction support

  Copyright (c) 2025, Dan Mowehhuk (danmowehhuk@gmail.com)
  All rights reserved.

*/

#ifndef _SDStorage_h
#define _SDStorage_h


#include <StreamableDTO.h>
#include <StreamableManager.h>
#include "sdstorage/FileHelper.h"
#include "sdstorage/Transaction.h"
#include "sdstorage/TransactionManager.h"
#include "sdstorage/StorageProvider.h"
#include "sdstorage/Strings.h"

static const char _SDSTORAGE_INDEX_EXTSN[]       PROGMEM = ".idx";

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
        _txnManager = new TransactionManager(&_fileHelper, &_storageProvider);
    };
    SDStorage(uint8_t sdCsPin, const char* rootDir, void (*errFunction)() = nullptr): 
          SDStorage(sdCsPin, rootDir, false, errFunction) {};
    SDStorage(uint8_t sdCsPin, const __FlashStringHelper* rootDir, void (*errFunction)() = nullptr):
          SDStorage(sdCsPin, reinterpret_cast<const char*>(rootDir), true, errFunction) {};
    SDStorage() = delete;
    ~SDStorage() {
      if (_txnManager) delete _txnManager;
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
     * The following prepend the path with the root directory if necessary
     */
    bool mkdir(const char* dirName, void* testState = nullptr);
    bool mkdir(const __FlashStringHelper* dirName, void* testState = nullptr);
    bool exists(const char* filename, void* testState = nullptr);
    bool exists(const __FlashStringHelper* filename, void* testState = nullptr);
    bool erase(const char* filename, Transaction* txn = nullptr);
    bool erase(const __FlashStringHelper* filename, Transaction* txn = nullptr);
    bool erase(void* testState, const char* filename, Transaction* txn = nullptr);
    bool erase(void* testState, const __FlashStringHelper* filename, Transaction* txn = nullptr);

    /*
     * Create a new transaction, locking the affected files
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
    Transaction* beginTxn(void* testState, const char* filename, Args... moreFilenames) {
      return _txnManager->beginTxn(testState, filename, moreFilenames...);
    };
    template <typename... Args>
    Transaction* beginTxn(void* testState, const __FlashStringHelper* filename, Args... moreFilenames) {
      return _txnManager->beginTxn(testState, filename, moreFilenames...);
    };

    /*
     * Applies the transaction's changes and unlocks the files.
     */
    // bool commitTxn(Transaction* txn, void* testState = nullptr) {
    //   return _txnManager->commitTxn(txn, testState);
    // };

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

};


#endif
