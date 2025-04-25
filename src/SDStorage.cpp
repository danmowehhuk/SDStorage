#include "SDStorage.h"
#include "sdstorage/Strings.h"

using namespace SDStorageStrings;

/*
 * see SDStorage.h
 */
bool SDStorage::begin(void* testState) {
  bool sdInit = false;
  do {
    // Check if there are any additional slashes after the leading '/'
    const char* secondSlash = strchr(_fileHelper.getRootDir() + 1, '/');
    if (secondSlash != nullptr) break; // Invalid root dir (subdirectories not allowed)
    if (!_storageProvider.begin()) break;
    if (!_storageProvider._exists(_fileHelper.getRootDir(), testState) 
          && !_storageProvider._mkdir(_fileHelper.getRootDir(), testState)) break;
    if (!_storageProvider._exists(_fileHelper.getIdxDir(), testState) 
          && !_storageProvider._mkdir(_fileHelper.getIdxDir(), testState)) break;
    if (!_storageProvider._exists(_fileHelper.getWorkDir(), testState) 
          && !_storageProvider._mkdir(_fileHelper.getWorkDir(), testState)) break;
//     if (!fsck()) {
// #if defined(DEBUG)
//       Serial.println(F("SDStorage repair failed"));
// #endif
//       break;
//     }
    sdInit = true;
  } while (false);
  return sdInit;
}

/*
 * Returns true if the file exists (after prepending the root dir on the 
 * filename if necessary)
 */
bool SDStorage::exists(const char* filename, void* testState = nullptr) {
  char* resolvedName = _fileHelper.canonicalFilename(filename);
  bool result = _storageProvider._exists(resolvedName, testState);
  delete[] resolvedName;
  return result;
}
bool SDStorage::exists(const __FlashStringHelper* filename, void* testState = nullptr) {
  char* filenameRAM = strdup(filename);
  bool result = exists(filenameRAM, testState);
  free(filenameRAM);
  return result;
}

/*
 * Deletes a file (after prepending the root dir on the filename if necessary).
 * If no transaction is provided, the write is auto-committed.
 */
bool SDStorage::erase(const char* filename, Transaction* txn = nullptr) {
  if (!filename) return false;
  return erase(nullptr, filename, txn);
}

bool SDStorage::erase(const __FlashStringHelper* filename, Transaction* txn = nullptr) {
  if (!filename) return false;
  char* filenameRAM = strdup(filename);
  bool result = erase(filenameRAM, txn);
  free(filenameRAM);
  return result;
}

bool SDStorage::erase(void* testState, const char* filename, Transaction* txn = nullptr) {
  if (!filename) return false;
  bool result = false;
  char* resolvedFilename = _fileHelper.canonicalFilename(filename);
  if (resolvedFilename && _storageProvider._exists(resolvedFilename, testState)) {
    if (txn) {
      char* tmpFilename = _txnManager->getTmpFilename(txn, resolvedFilename);
      if (tmpFilename && strlen(tmpFilename) > 0) {
        txn->put(resolvedFilename, _SDSTORAGE_TOMBSTONE, false, true); // tombstone the filename
        char* txnFilename = txn->getFilename();
       _storageProvider._writeTxnToStream(txnFilename, txn, testState);
        delete[] txnFilename;
        result = true;
      }
    } else {
      result = _storageProvider._remove(resolvedFilename, testState);
    }
  }
  if (resolvedFilename) delete[] resolvedFilename;
  return result;
}

bool SDStorage::erase(void* testState, const __FlashStringHelper* filename, Transaction* txn = nullptr) {
  if (!filename) return false;
  char* filenameRAM = strdup(filename);
  bool result = erase(testState, filenameRAM, txn);
  free(filenameRAM);
  return result;
}

/*
 * Creates a new directory (after prepending the root dir on the 
 * dirName if necessary). Returns true if successful.
 */
bool SDStorage::mkdir(const char* dirName, void* testState = nullptr) {
  char* resolvedName = _fileHelper.canonicalFilename(dirName);
  bool result = _storageProvider._mkdir(resolvedName, testState);
  delete[] resolvedName;
  return result;
}
bool SDStorage::mkdir(const __FlashStringHelper* dirName, void* testState = nullptr) {
  char* dirNameRAM = strdup(dirName);
  bool result = mkdir(dirNameRAM, testState);
  free(dirNameRAM);
  return result;
}
