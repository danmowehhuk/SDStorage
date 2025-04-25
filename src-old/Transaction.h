#ifndef _SDStorage_Transaction_h
#define _SDStorage_Transaction_h


#include <StreamableDTO.h>
#include "SDStorageStrings.h"


static const char _SDSTORAGE_TXN_TMP_EXTSN[]    PROGMEM = ".tmp";
static const char _SDSTORAGE_TXN_TX_EXTSN[]     PROGMEM = ".txn";
static const char _SDSTORAGE_TXN_COMMIT_EXTSN[] PROGMEM = ".cmt";

/*
 * Represents a group of one or more files that have been "locked" to this
 * transaction.
 */
class Transaction: public StreamableDTO {

  public:
    virtual ~Transaction() {
      if (_baseFilename) delete[] _baseFilename;
    };

  private:
    bool _isCommitted = false;
    char* _baseFilename = nullptr;
    Transaction(const char* workDir);

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
    // Returns a dynamically allocated char[] must be freed by the caller
    char* getFilename();

    // Temp filename where uncommitted changes will be written
    char* getTmpFilename(const char* filename);

    // Lock (waiting if necessary) and add to transaction
    void add(const char* filename, const char* workDir);
    void releaseLocks();

    friend class SDStorage;
    friend class SDStorageTestHelper;
};


#endif