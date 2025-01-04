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

    /*
     * Returns the fully-qualified filename of the given index so it can
     * be added to a Transaction via beginTxn(filenames...)
     */
    String indexFilename(const String &idxName);


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
    bool exists(const String &filename, void* testState = nullptr);
    bool load(const String &filename, StreamableDTO* dto, void* testState = nullptr);
    bool save(const String &filename, StreamableDTO* dto, Transaction* txn = nullptr);
    bool save(void* testState, const String &filename, StreamableDTO* dto, Transaction* txn = nullptr);
    bool erase(const String &filename, Transaction* txn = nullptr);
    bool erase(void* testState, const String& filename, Transaction* txn = nullptr);

    // INDEX SEARCH RESULTS
    struct KeyValue {
      const String& key;
      const String& value;
      KeyValue* next = nullptr;
      KeyValue(const String& key, const String& value): key(key), value(value) {};
      ~KeyValue() {
        KeyValue* current = this->next;
        while (current != nullptr) {
          KeyValue* toDelete = current;
          current = current->next;
          delete toDelete;
        }
      }
    };
    struct SearchResults {
      const String& searchPrefix;
      bool trieMode = false;
      uint32_t trieBloom[3] = {0};
      uint8_t matchCount = 0;
      KeyValue* matchResult = nullptr;
      KeyValue* trieResult = nullptr;
      SearchResults(const String& searchPrefix): searchPrefix(searchPrefix) {};
      ~SearchResults() {
        if (matchResult) delete matchResult;
        if (trieResult) delete trieResult;
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
    Transaction* beginTxn(const String &filename, Args... moreFilenames);
    template <typename... Args>
    Transaction* beginTxn(void* testState, const String &filename, Args... moreFilenames);
    bool commitTxn(Transaction* txn, void* testState = nullptr);
    bool abortTxn(Transaction* txn, void* testState = nullptr);

  private:
    friend class SDStorageTestHelper;
    SDStorage();
    SDStorage(SDStorage &t);
    uint8_t _sdCsPin;
    StreamableManager _streams;
    String _rootDir;
    String _workDir = String();
    String _idxDir = String();
#if defined(__SDSTORAGE_TEST)
    MockSdFat _sd;
#else
    SdFat _sd;
#endif
    String realFilename(const String &filename);
    bool fsck();

    // transaction helpers
    template <typename... Args>
    bool _beginTxn(Transaction* txn, void* testState, const String &filename, Args... moreFilenames);
    bool _beginTxn(Transaction* txn, void* testState) { return true; };
    bool _applyChanges(Transaction* txn, void* testState = nullptr);
    void _cleanupTxn(Transaction* txn, void* testState = nullptr);
    String getTmpFilename(Transaction* txn, const String& filename);
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
    static bool _pipeFast(const String& line, StreamableManager::DestinationStream* dest);
    static String toIndexLine(const String& key, const String& value);
    static String parseIndexKey(const String& line);
    static String parseIndexValue(const String& line);
    static void _appendKeyValue(KeyValue* head, KeyValue* result);
    static void appendMatchResult(SearchResults* sr, KeyValue* result);
    static void appendTrieResult(SearchResults* sr, KeyValue* result);
    static bool idxLookupFilter(const String& line, StreamableManager::DestinationStream* dest, void* statePtr);
    static bool idxUpsertFilter(const String& line, StreamableManager::DestinationStream* dest, void* statePtr);
    static bool idxRemoveFilter(const String& line, StreamableManager::DestinationStream* dest, void* statePtr);
    static bool idxRenameFilter(const String& line, StreamableManager::DestinationStream* dest, void* statePtr);
    static bool idxPrefixSearchFilter(const String& line, StreamableManager::DestinationStream* dest, void* statePtr);

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
    void _writeIndexLine(const String& indexFilename, const String& line, void* testState = nullptr);
    void _updateIndex(const String& indexFilename, const String& tmpFilename, 
          StreamableManager::FilterFunction filter, void* statePtr, void* testState = nullptr);
    void _scanIndex(const String &indexFilename, StreamableManager::FilterFunction filter, 
          void* statePtr, void* testState = nullptr);

};

template <typename... Args>
Transaction* SDStorage::beginTxn(const String &filename, Args... moreFilenames) {
  return beginTxn(nullptr, filename, moreFilenames...);
}

template <typename... Args>
Transaction* SDStorage::beginTxn(void* testState, const String &filename, Args... moreFilenames) {
  Transaction* txn = new Transaction(_workDir);
  if (!_beginTxn(txn, testState, filename, moreFilenames...)) {
    txn->releaseLocks();
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
