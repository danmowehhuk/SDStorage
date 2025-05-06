#ifndef _SDStorage_Transaction_h
#define _SDStorage_Transaction_h


#include <StreamableDTO.h>
#include "Strings.h"
#include "FileHelper.h"


static const char _SDSTORAGE_TXN_TMP_EXTSN[]    PROGMEM = ".tmp";
static const char _SDSTORAGE_TXN_TX_EXTSN[]     PROGMEM = ".txn";
static const char _SDSTORAGE_TXN_COMMIT_EXTSN[] PROGMEM = ".cmt";

/*
 * Represents a group of one or more files that have been "locked" to this
 * transaction.
 */
class Transaction: public StreamableDTO {

  public:
    Transaction() = delete;
    virtual ~Transaction() {
      releaseLocks();
      if (_baseFilename) delete[] _baseFilename;
      _baseFilename = nullptr;
    };

    // Disable moving and copying
    Transaction(Transaction&& other) = delete;
    Transaction& operator=(Transaction&& other) = delete;
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

  private:
    bool _isCommitted = false;
    char* _baseFilename = nullptr;
    FileHelper* _fileHelper;

    Transaction(FileHelper* fileHelper);

    // Sequence restarts from 0 with every reboot, so it's vital that
    // the SDStorage::fsck() process cleans up any lingering
    // transaction files
    static uint16_t _idSeq;

    // Puts filename as key (no values) to lock a file. These locks
    // are lost on reboot, so it's vital that SDStorage::fsck() process
    // cleans up any lingering transaction files
    static StreamableDTO* _locks; // file access locks

    // Sets the _isCommitted flag to true
    void setCommitted();

    // Filename of this transaction's file: <workDir>/<id>.<extension>
    bool getFilename(char* buffer, size_t bufferSize);

    // Temp filename where uncommitted changes will be written
    char* getTmpFilename(const char* filename, bool isPmem = false);

    // Lock (waiting if necessary) and add to transaction
    void add(const char* filename, const char* workDir);
    void releaseLocks();

    friend class SDStorage;
    friend class TransactionManager;
    friend class SDStorageTestHelper;
};


#endif