#ifndef _SDStorageTestHelper_h
#define _SDStorageTestHelper_h


#include <Arduino.h>
#include <SdFat.h>
#include <SDStorage.h>
#include <sdstorage/Strings.h>


using namespace SDStorageStrings;

class SDStorageTestHelper {

  public:
    SdFat* getSdFat(SDStorage* sdStorage) {
      return &(sdStorage->_storageProvider._sd);
    };
    FileHelper* getFileHelper(SDStorage* sdStorage) {
      return &(sdStorage->_fileHelper);
    };
    Transaction* newTransaction(SDStorage* sdStorage) {
      return new Transaction(getFileHelper(sdStorage));
    };
    void addToTxn(Transaction* txn, const char* filename) {
      txn->add(filename);
    };
    char* getTmpFilename(Transaction* txn, const char* filename) {
      return txn->getTmpFilename(filename);
    };
    bool createFile(SdFat* sdFat, const char* filename) {
      File file = sdFat->open(filename, FILE_WRITE);
      if (!file) return false;
      file.println("abc=123");
      file.close();
      return sdFat->exists(filename);
    };
    void commit(Transaction* txn) {
      txn->setCommitted();
    };
    bool writeTxn(SdFat* sdFat, Transaction* txn, const char* fname) {
      char filename[64];
      txn->getFilename(filename, 64);
      File file = sdFat->open(filename, FILE_WRITE);
      if (!file) return false;
      char line[64];
      static const char fmt[] PROGMEM = "%s=%s";
      snprintf_P(line, 64, fmt, fname, txn->getTmpFilename(fname));
      file.println(line);
      file.close();
      return sdFat->exists(filename);
    };
    bool doFsck(SDStorage* sdStorage) {
      return sdStorage->fsck();
    };

};


#endif