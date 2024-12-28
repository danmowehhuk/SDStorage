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
    SDStorage(uint8_t sdCsPin, const String &rootDir): 
          _sdCsPin(sdCsPin), _rootDir(rootDir) {};
    
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
    bool mkdir(const String &dirName, void* testState = nullptr);

    // FILE OPERATIONS //
    /*
     * All filenames will be automatically prepended with the root
     * directory if they aren't already.
     */
    bool exists(const String &filename, void* testState = nullptr);
    bool load(const String &filename, StreamableDTO* dto, void* testState = nullptr);
    bool save(const String &filename, StreamableDTO* dto, Transaction* txn = nullptr, void* testState = nullptr);
    bool erase(const String &filename, void* testState = nullptr);
    bool erase(Transaction* txn, const String& filename, void* testState = nullptr);

    // INDEX OPERATIONS
    String indexFilename(const String &idxName);
    // bool idxPrefixSearch(const String &idxName, const String &prefix); //, IndexEntry<T>* resultHead);
    // bool idxHasKey(const String &idxName, const String &key);
    // template <typename T>
    // T idxLookup(const String &idxName, const String &key);
    bool idxUpsert(const String &idxName, const String &key, const String &value, Transaction* txn = nullptr);
    bool idxUpsert(void* testState, const String &idxName, const String &key, const String &value, Transaction* txn = nullptr);
    // bool idxRemove(const String &idxName, const String &key, Transaction* txn = nullptr);
    // bool idxRename(const String &idxName, const String &oldKey, const String &newKey, Transaction* txn = nullptr);

    // TRANSACTION OPERATIONS
    template <typename... Args>
    Transaction* beginTxn(const String &filename, Args... moreFilenames);
    template <typename... Args>
    Transaction* beginTxn(void* testState, const String &filename, Args... moreFilenames);
    bool commitTxn(Transaction* txn, void* testState = nullptr);
    bool abortTxn(Transaction* txn, void* testState = nullptr);

  private:
    friend class SDStorageTestHelper;
    SDStorage();
    SDStorage(SDStorage &t);

#if defined(__SDSTORAGE_TEST)
    MockSdFat _sd;
#else
    SdFat _sd;
#endif

    /*
     * Wrap the underlying calls to _sd so that a state capture object
     * can be passed to MockSdFat when testing
     */
    bool _exists(const String& filename, void* testState = nullptr);
    bool _mkdir(const String& filename, void* testState = nullptr);
    bool _loadFromStream(const String& filename, StreamableDTO* dto, void* testState = nullptr);
    void _writeToStream(const String& filename, StreamableDTO* dto, void* testState = nullptr);
    void _writeTxnToStream(const String& filename, StreamableDTO* dto, void* testState = nullptr);
    bool _remove(const String& filename, void* testState = nullptr);
    bool _rename(const String& oldFilename, const String& newFilename, void* testState = nullptr);


    String realFilename(const String &filename);
    bool fsck();

    uint8_t _sdCsPin;
    StreamableManager _streams;
    String _rootDir;
    String _workDir = String();
    String _idxDir = String();

    // transaction helpers
    template <typename... Args>
    bool _beginTxn(Transaction* txn, void* testState, const String &filename, Args... moreFilenames);
    bool _beginTxn(Transaction* txn, void* testState) { return true; };
    bool _applyChanges(Transaction* txn, void* testState = nullptr);
    void _cleanupTxn(Transaction* txn, void* testState = nullptr);
    String getTmpFilename(Transaction* txn, const String& filename);

    // For passing data in/out of Transaction-related lambda functions
    struct TransactionCapture {
      SDStorage* sdStorage;
      void* ts;
      bool success = true;
      TransactionCapture(SDStorage* sdStorage, void* testState):
          sdStorage(sdStorage), ts(testState) {};
    };

    // index helpers
    // bool _updateIndex(const String &idxName, StreamableManager::FilterFunction filter, void* statePtr, Transaction* txn = nullptr);
    // bool _scanIndex(const String &idxName, StreamableManager::FilterFunction filter, void* statePtr);
    // static String extractKey(const String& line);
    // static String extractValue(const String& line);
    // static String toLine(const String& key, const String& value);

};

template <typename... Args>
Transaction* SDStorage::beginTxn(const String &filename, Args... moreFilenames) {
  return beginTxn(nullptr, filename, moreFilenames...);
}

template <typename... Args>
Transaction* SDStorage::beginTxn(void* testState, const String &filename, Args... moreFilenames) {
  Transaction* txn = new Transaction(_workDir);
  if (!_beginTxn(txn, testState, filename, moreFilenames...)) {
    delete txn;
    return nullptr;
  }
  _writeTxnToStream(txn->getFilename(), txn, testState);
  return txn;
}

template <typename... Args>
bool SDStorage::_beginTxn(Transaction* txn, void* testState, const String &filename, Args... moreFilenames) {
  String resolvedFilename = realFilename(filename);
  txn->add(resolvedFilename, _workDir);
  String tmpFilename = txn->getTmpFilename(resolvedFilename);
  if (_exists(tmpFilename, testState)) {
#if defined(DEBUG)
    Serial.println(String(F("Transaction file already exists: ")) + tmpFilename);
#endif
    return false;
  }
  return _beginTxn(txn, testState, moreFilenames...);
}


#endif
