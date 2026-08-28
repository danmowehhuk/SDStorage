#ifndef _SDStorageTestHelper_h
#define _SDStorageTestHelper_h


#include <Arduino.h>
#include <Sequence.h>
#include <sdstorage/FileHelper.h>
#include <sdstorage/IndexHelpers.h>
#include <sdstorage/Strings.h>

using namespace SDStorageStrings;

class SDStorageTestHelper {

  public:
    bool canonicalFilename(SDStorage* sdStorage, FileHelper::Filename filename, char* buffer, size_t bufferSize) {
      return sdStorage->_fileHelper.canonicalFilename(filename, buffer, bufferSize);
    };
    char* getRootDir(SDStorage* sdStorage) {
      return sdStorage->_fileHelper.getRootDir();
    };
    char* getWorkDir(SDStorage* sdStorage) {
      return sdStorage->_fileHelper.getWorkDir();
    };
    char* getIdxDir(SDStorage* sdStorage) {
      return sdStorage->_fileHelper.getIdxDir();
    };
    bool isValidFAT16Filename(SDStorage* sdStorage, const __FlashStringHelper* filename) {
      char* fname = strdup(filename);
      bool result = sdStorage->_fileHelper.isValidFAT16Filename(fname);
      free(fname);
      return result;
    };
    bool getPathFromFilename(SDStorage* sdStorage, const __FlashStringHelper* filename, char* buffer, size_t bufferSize) {
      char* fname = strdup(filename);
      bool result = sdStorage->_fileHelper.getPathFromFilename(fname, buffer, bufferSize);
      free(fname);
      return result;
    };
    bool getFilenameFromFullName(SDStorage* sdStorage, const __FlashStringHelper* filename, char* buffer, size_t bufferSize) {
      char* fname = strdup(filename);
      bool result = sdStorage->_fileHelper.getFilenameFromFullName(fname, buffer, bufferSize);
      free(fname);
      return result;
    };
    bool getIndexFilename(SDStorage* sdStorage, Index idx, char* buffer, size_t bufferSize) {
      return sdStorage->_fileHelper.indexFilename(idx, buffer, bufferSize);
    };
    char* getSeqDir(SDStorage* sdStorage) {
      return sdStorage->_fileHelper.getSeqDir();
    };
    bool getSequenceFilename(SDStorage* sdStorage, sdstorage::Sequence seq, char* buffer, size_t bufferSize) {
      return sdStorage->_fileHelper.sequenceFilename(seq, buffer, bufferSize);
    };
    FileHelper::Filename toFilename(const __FlashStringHelper* filename) {
      return FileHelper::Filename(filename);
    };
    bool toIndexLine(IndexEntry* entry, char* buffer, size_t bufferSize) {
      return IndexHelpers::toIndexLine(entry, buffer, bufferSize);
    };
    IndexEntry parseIndexEntry(const __FlashStringHelper* line) {
      char* l = strdup(line);  // this is a RAM copy of the flash string
      if (!l) return IndexEntry("");
      IndexEntry result = IndexHelpers::parseIndexEntry(l);
      free(l);
      return result;
    };
    uint64_t seqCurrentRaw(SDStorage* sdStorage, sdstorage::Sequence seq, void* testState = nullptr) {
      return sdStorage->_seqManager->current(seq, testState);
    };
    uint64_t seqNextRaw(SDStorage* sdStorage, void* testState, sdstorage::Sequence seq, Transaction* txn = nullptr) {
      return sdStorage->_seqManager->next(testState, seq, txn);
    };
    bool seqCurrentStrRaw(SDStorage* sdStorage, sdstorage::Sequence seq, char* out, sdstorage::SeqToString f, void* testState = nullptr) {
      return sdStorage->_seqManager->current(seq, out, f, testState);
    };
    bool seqNextStrRaw(SDStorage* sdStorage, void* testState, sdstorage::Sequence seq, char* out, sdstorage::SeqToString f, Transaction* txn = nullptr) {
      return sdStorage->_seqManager->next(testState, seq, out, f, txn);
    };
};


#endif
