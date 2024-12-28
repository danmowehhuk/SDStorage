#ifndef _SDStorage_Transaction_h
#define _SDStorage_Transaction_h


#include <StreamableDTO.h>


static const char _SDSTORAGE_TXN_TMP_EXTSN[]    PROGMEM = ".tmp";
static const char _SDSTORAGE_TXN_TX_EXTSN[]     PROGMEM = ".txn";
static const char _SDSTORAGE_TXN_COMMIT_EXTSN[] PROGMEM = ".cmt";

/*
 * Represents a group of one or more files that have been "locked" to this
 * transaction.
 */
class Transaction: public StreamableDTO {

  public:
    virtual ~Transaction() {};

  private:
    friend class SDStorage;
    static uint16_t _idSeq;
    static StreamableDTO* _locks; // file access locks
    bool _isCommitted = false;
    String _baseFilename = String();

    Transaction(const String &workDir): StreamableDTO() {
      _baseFilename = workDir + String(F("/")) + String(_idSeq++);
    };
    void setCommitted() {
      _isCommitted = true;
    };
    String getFilename() {
      String extension = _isCommitted 
              ? reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_TXN_COMMIT_EXTSN)
              : reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_TXN_TX_EXTSN);
      return _baseFilename + extension;
    };
    String getTmpFilename(const String &filename) {
      String tmpFilename = String();
      char* tmpFn = get(filename);
      if (tmpFn != nullptr) {
        tmpFilename = String(tmpFn);
      }
      return tmpFilename;
    };

    // Lock (waiting if necessary) and add to transaction
    void add(const String &filename, const String &workDir);
    void releaseLocks();

};


#endif