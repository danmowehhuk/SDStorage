#ifndef _SDStorageTestHelper_h
#define _SDStorageTestHelper_h


#include <Arduino.h>

class SDStorageTestHelper {

  public:
    char* canonicalFilename(SDStorage* sdStorage, const char* filename) {
      return sdStorage->_fileHelper.canonicalFilename(filename);
    };
    char* canonicalFilename(SDStorage* sdStorage, const __FlashStringHelper* filename) {
      return sdStorage->_fileHelper.canonicalFilename(filename);
    };
    char* getRootDir(SDStorage* sdStorage) {
      return sdStorage->_fileHelper.getRootDir();
    }
    char* getWorkDir(SDStorage* sdStorage) {
      return sdStorage->_fileHelper.getWorkDir();
    }
    char* getIdxDir(SDStorage* sdStorage) {
      return sdStorage->_fileHelper.getIdxDir();
    }
    bool isValidFAT16Filename(SDStorage* sdStorage, __FlashStringHelper* filename) {
      return sdStorage->_fileHelper.isValidFAT16Filename(filename);
    }
    char* getPathFromFilename(SDStorage* sdStorage, __FlashStringHelper* filename) {
      return sdStorage->_fileHelper.getPathFromFilename(filename);
    }
    char* getFilenameFromFullName(SDStorage* sdStorage, __FlashStringHelper* filename) {
      return sdStorage->_fileHelper.getFilenameFromFullName(filename);
    }
};


#endif
