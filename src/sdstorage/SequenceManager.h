#ifndef _SDStorage_SequenceManager_h
#define _SDStorage_SequenceManager_h


#include "../Sequence.h"
#include "FileHelper.h"
#include "StorageProvider.h"
#include "Strings.h"
#include "Transaction.h"
#include "TransactionManager.h"
#include <stdint.h>


using namespace sdstorage;
using namespace SDStorageStrings;

class SequenceManager {

  public:
    SequenceManager(FileHelper* fileHelper, StorageProvider* storageProvider,
          TransactionManager* txnManager, void (*errFunction)() = nullptr):
        _fileHelper(fileHelper), _storageProvider(storageProvider),
        _txnManager(txnManager), _errFunction(errFunction) {};

    // Disable moving and copying
    SequenceManager(SequenceManager&& other) = delete;
    SequenceManager& operator=(SequenceManager&& other) = delete;
    SequenceManager(const SequenceManager&) = delete;
    SequenceManager& operator=(const SequenceManager&) = delete;

  private:
    FileHelper* _fileHelper;
    StorageProvider* _storageProvider;
    TransactionManager* _txnManager;
    void (*_errFunction)();

    uint64_t current(Sequence seq, void* testState = nullptr);
    uint64_t next(void* testState, Sequence seq, Transaction* txn = nullptr);
    bool init(void* testState, Sequence seq, uint64_t initialValue, Transaction* txn = nullptr);

    bool current(Sequence seq, char* out, SeqToString f, void* testState = nullptr);
    bool next(void* testState, Sequence seq, char* out, SeqToString f, Transaction* txn = nullptr);

    // Loads a sequence's stored value if its file exists. Sets *fileExists
    // to whether the file was found at all, and *anomalous if it existed
    // but couldn't be read or parsed to a legitimate (non-zero) value.
    // next() and init() never persist 0, so an existing file that yields 0
    // is always corruption or a read failure, never a real stored state.
    uint64_t loadStoredValue(const char* seqFilename, void* testState, bool* fileExists, bool* anomalous);

    friend class SDStorage;
    friend class SDStorageTestHelper;

};


#endif
