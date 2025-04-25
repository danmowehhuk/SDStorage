/*

  SDStorage.h

  SD card storage manager for StreamableDTOs with index and transaction support

  Copyright (c) 2025, Dan Mowehhuk (danmowehhuk@gmail.com)
  All rights reserved.

*/

#ifndef _SDStorage_h
#define _SDStorage_h


#if defined(__SDSTORAGE_TEST)
  #include "../test/test-suite-sim/MockSdFat.h"
#else
  #include <SdFat.h>
#endif
#include <StreamableDTO.h>
#include <StreamableManager.h>
#include "Transaction.h"

static const char _SDSTORAGE_WORK_DIR[]          PROGMEM = "~WORK";
static const char _SDSTORAGE_IDX_DIR[]           PROGMEM = "~IDX";
static const char _SDSTORAGE_INDEX_EXTSN[]       PROGMEM = ".idx";
static const char _SDSTORAGE_TOMBSTONE[]         PROGMEM = "{TOMBSTONE}";

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
    SDStorage(uint8_t sdCsPin, const char* rootDir, void (*errFunction)() = nullptr): 
          _sdCsPin(sdCsPin), _rootDir(rootDir), _isRootDirPmem(false), _errFunction(errFunction) {};
    SDStorage(uint8_t sdCsPin, const char* rootDir, bool isRootDirPmem = false, void (*errFunction)() = nullptr): 
          _sdCsPin(sdCsPin), _rootDir(rootDir), _isRootDirPmem(isRootDirPmem), _errFunction(errFunction) {};
    SDStorage(uint8_t sdCsPin, const __FlashStringHelper* rootDir, void (*errFunction)() = nullptr):
          SDStorage(sdCsPin, reinterpret_cast<const char*>(rootDir), true, errFunction) {};
    SDStorage() = delete;

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
     * Creates a directory, prepending the path with the root directory
     * if necessary
     */
    bool mkdir(const char* dirName, void* testState = nullptr);

    /*
     * Returns the fully-qualified filename of the given index so it can
     * be added to a Transaction via beginTxn(filenames...)
     * These return dynamically allocated char[]'s that the caller must free
     */
    char* indexFilename(const char* idxName, bool isIdxNamePmem = false);
    char* indexFilename(const __FlashStringHelper* idxName);


    // FILE OPERATIONS //
    /*
     * All filenames will be automatically prepended with the root
     * directory if they aren't already. Optionally, a Transaction may
     * be passed so that no changes are applied until
     * commitTxn(transaction) is called. Likewise, all changes are
     * discarded if abortTransaction(transaction) is called.
     * 
     * See: beginTxn(filenames...)
     *      commitTxn(transaction)
     *      abortTransaction(transaction)
     */
    bool exists(const char* filename, void* testState = nullptr);
    bool load(const char* filename, StreamableDTO* dto, bool isFilenamePmem = false, void* testState = nullptr);
    bool load(const __FlashStringHelper* filename, StreamableDTO* dto, void* testState = nullptr);
    bool save(const char* filename, StreamableDTO* dto, Transaction* txn = nullptr, bool isFilenamePmem = false);
    bool save(void* testState, const char* filename, StreamableDTO* dto, Transaction* txn = nullptr, bool isFilenamePmem = false);
    bool save(const __FlashStringHelper* filename, StreamableDTO* dto, Transaction* txn = nullptr);
    bool save(void* testState, const __FlashStringHelper* filename, StreamableDTO* dto, Transaction* txn = nullptr);
    bool erase(const char* filename, Transaction* txn = nullptr);
    bool erase(void* testState, const char* filename, Transaction* txn = nullptr);

    // INDEX SEARCH RESULTS
    struct KeyValue {
      String key;
      String value;
      KeyValue* next = nullptr;
      KeyValue(const String& key, const String& value): key(key), value(value) {};
      ~KeyValue() {
        KeyValue* current = this->next;
        while (current != nullptr) {
          KeyValue* toDelete = current;
          current = current->next;
          toDelete->next = nullptr;
          delete toDelete;
        }
      }
    };
    struct SearchResults {
      String searchPrefix;
      bool trieMode = false;
      uint32_t trieBloom[3] = {0};
      uint8_t matchCount = 0;
      KeyValue* matchResult = nullptr;
      KeyValue* trieResult = nullptr;
      SearchResults(const String& searchPrefix): searchPrefix(searchPrefix) {};
      ~SearchResults() {
        if (matchResult) {
          delete matchResult;
        }
        if (trieResult) {
          delete trieResult;
        }
      }
    };

    // INDEX OPERATIONS
    void idxPrefixSearch(const String &idxName, SearchResults* results, void* testState = nullptr);
    bool idxHasKey(const String &idxName, const String &key, void* testState = nullptr);
    String idxLookup(const String &idxName, const String &key, void* testState = nullptr);
    bool idxUpsert(const String &idxName, const String &key, const String &value, Transaction* txn = nullptr);
    bool idxUpsert(void* testState, const String &idxName, const String &key, const String &value, Transaction* txn = nullptr);
    bool idxRemove(const String &idxName, const String &key, Transaction* txn = nullptr);
    bool idxRemove(void* testState, const String &idxName, const String &key, Transaction* txn = nullptr);
    bool idxRename(const String &idxName, const String &oldKey, const String &newKey, Transaction* txn = nullptr);
    bool idxRename(void* testState, const String &idxName, const String &oldKey, const String &newKey, Transaction* txn = nullptr);

    // TRANSACTION OPERATIONS
    template <typename... Args>
    Transaction* beginTxn(const char* filename, Args... moreFilenames);
    template <typename... Args>
    Transaction* beginTxn(void* testState, const char* filename, Args... moreFilenames);
    bool commitTxn(Transaction* txn, void* testState = nullptr);
    bool abortTxn(Transaction* txn, void* testState = nullptr);

  private:
    
    friend class SDStorageTestHelper;

    uint8_t _sdCsPin;
    void (*_errFunction)() = nullptr;
    StreamableManager _streams;
    char* _rootDir;
    bool _isRootDirPmem;
    char* _workDir = nullptr;
    char* _idxDir = nullptr;
#if defined(__SDSTORAGE_TEST)
    MockSdFat _sd;
#else
    SdFat _sd;
#endif

    /*
     * Returns the fully qualified path to a file. These methods return
     * dynamically allocated char[]'s that the caller needs to free.
     */
    char* realFilename(const char* filename, bool isFilenamePmem = false);
    char* realFilename(const __FlashStringHelper* filename);
    
    /*
     * Cleans up the _workDir on initialization in case any transactions were
     * left after a power interruption. Finalized transactions are completed, and
     * all others are rolled back.
     */
    bool fsck();

    // transaction helpers
    template <typename... Args>
    bool _beginTxn(Transaction* txn, void* testState, const char* filename, Args... moreFilenames);
    bool _beginTxn(Transaction* txn, void* testState) { return true; };
    bool _applyChanges(Transaction* txn, void* testState = nullptr);
    void _cleanupTxn(Transaction* txn, void* testState = nullptr);
    char* getTmpFilename(Transaction* txn, const char* filename);
    bool isValidFAT16Filename(const char* filename);
    char* getPathFromFilename(const char* filename);
    char* getFilenameFromFullName(const char* filename);
    bool finalizeTxn(Transaction* txn, bool autoCommit, bool success, void* testState);

    // For passing data in/out of Transaction-related lambda functions
    struct TransactionCapture {
      SDStorage* sdStorage;
      void* ts;
      bool success = true;
      TransactionCapture(SDStorage* sdStorage, void* testState):
          sdStorage(sdStorage), ts(testState) {};
    };

    // For passing data in/out of index-related lambda functions
    struct IdxScanCapture {
      const String& key;         // in
      const String& newKey;      // in
      const String& valueIn;     // in
      const bool isUpsert;       // in
      String value = String();   // out
      String prevKey = String(); // out
      bool keyExists = false;    // out
      bool didUpsert = false;    // out
      bool didRemove = false;    // out
      bool didInsert = false;    // out
      IdxScanCapture(const String& key): 
          key(key), newKey(String()), valueIn(String()), isUpsert(false) {};
      IdxScanCapture(const String& key, const String& value): 
          key(key), newKey(String()), valueIn(value), isUpsert(true) {};
      IdxScanCapture(const String& oldKey, const String& newKey, bool):
          key(oldKey), newKey(newKey), valueIn(String()), isUpsert(false) {};
    };

    // index helpers
    void _idxScan(const String& idxName, IdxScanCapture* state, void* testState = nullptr);
    static bool _pipeFast(const char* line, StreamableManager::DestinationStream* dest);
    static String toIndexLine(const String& key, const String& value);
    static String parseIndexKey(const String& line);
    static String parseIndexValue(const String& line);
    static void _appendKeyValue(KeyValue* head, KeyValue* result);
    static void appendMatchResult(SearchResults* sr, KeyValue* result);
    static void appendTrieResult(SearchResults* sr, KeyValue* result);
    static bool idxLookupFilter(const char* line, StreamableManager::DestinationStream* dest, void* statePtr);
    static bool idxUpsertFilter(const char* line, StreamableManager::DestinationStream* dest, void* statePtr);
    static bool idxRemoveFilter(const char* line, StreamableManager::DestinationStream* dest, void* statePtr);
    static bool idxRenameFilter(const char* line, StreamableManager::DestinationStream* dest, void* statePtr);
    static bool idxPrefixSearchFilter(const char* line, StreamableManager::DestinationStream* dest, void* statePtr);

    /*
     * Wrap the underlying calls to _sd so that a state capture object
     * can be passed to MockSdFat when testing
     */
    bool _exists(const char* filename, void* testState = nullptr);
    bool _isDir(const char* filename, void* testState = nullptr);
    bool _mkdir(const char* filename, void* testState = nullptr);
    bool _loadFromStream(const char* filename, StreamableDTO* dto, void* testState = nullptr);
    void _writeToStream(const char* filename, StreamableDTO* dto, void* testState = nullptr);
    void _writeTxnToStream(const char* filename, StreamableDTO* dto, void* testState = nullptr);
    bool _remove(const char* filename, void* testState = nullptr);
    bool _rename(const char* oldFilename, const char* newFilename, void* testState = nullptr);
    void _writeIndexLine(const char* indexFilename, const char* line, void* testState = nullptr);
    void _updateIndex(const char* indexFilename, const char* tmpFilename, 
          StreamableManager::FilterFunction filter, void* statePtr, void* testState = nullptr);
    void _scanIndex(const char* indexFilename, StreamableManager::FilterFunction filter, 
          void* statePtr, void* testState = nullptr);

};

template <typename... Args>
Transaction* SDStorage::beginTxn(const char* filename, Args... moreFilenames) {
  return beginTxn(nullptr, filename, moreFilenames...);
}

template <typename... Args>
Transaction* SDStorage::beginTxn(void* testState, const char* filename, Args... moreFilenames) {

  Serial.println("In beginTxn");
  delay(100);

  Transaction* txn = new Transaction(_workDir);
  if (!_beginTxn(txn, testState, filename, moreFilenames...)) {
    txn->releaseLocks();
    delete txn;
    return nullptr;
  }
  Serial.println("writing Txn");
  delay(100);

  _writeTxnToStream(txn->getFilename(), txn, testState);
  return txn;
}

template <typename... Args>
bool SDStorage::_beginTxn(Transaction* txn, void* testState, const char* filename, Args... moreFilenames) {
  char* resolvedFilename = realFilename(filename);
  if (!_exists(resolvedFilename, testState)) {

    // New file to be created, so check the name is valid
    char* shortFilename = getFilenameFromFullName(resolvedFilename);
    if (!isValidFAT16Filename(shortFilename)) {
#if defined(DEBUG)
      Serial.print(F("Invalid FAT16 filename: "));
      Serial.println(shortFilename);
#endif
      delete[] shortFilename;
      delete[] resolvedFilename;
      return false;
    }
    char* path = getPathFromFilename(resolvedFilename);
    if (!_exists(path, testState)) {
#if defined(DEBUG)
      Serial.print(F("Directory does not exist: "));
      Serial.println(path);
#endif
      delete[] path;
      delete[] shortFilename;
      delete[] resolvedFilename;
      return false;
    }
    if (!_isDir(path, testState)) {
#if defined(DEBUG)
      Serial.print(F("Not a directory: "));
      Serial.println(path);
#endif
      delete[] path;
      delete[] shortFilename;
      delete[] resolvedFilename;
      return false;
    }
    delete[] path;
    delete[] shortFilename;
  }
  txn->add(resolvedFilename, _workDir);
  char* tmpFilename = txn->getTmpFilename(resolvedFilename);
  if (tmpFilename && _exists(tmpFilename, testState)) {
#if defined(DEBUG)
    Serial.print(F("Transaction file already exists: "));
    Serial.println(tmpFilename);
#endif
    delete[] resolvedFilename;
    return false;
  }
  delete[] resolvedFilename;
  return _beginTxn(txn, testState, moreFilenames...);
}


#endif
