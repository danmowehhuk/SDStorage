#ifndef _SDStorage_SequenceManager_h
#define _SDStorage_SequenceManager_h


#include "../Sequence.h"
#include "FileHelper.h"
#include "StorageProvider.h"
#include "Strings.h"
#include "Transaction.h"
#include "TransactionManager.h"


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

    friend class SDStorage;
    friend class SDStorageTestHelper;

};


#endif
