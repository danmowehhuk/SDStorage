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
 * Populates the DTO with data read from a file (after prepending the root dir
 * on the filename if necessary)
 */
bool SDStorage::load(const char* filename, StreamableDTO* dto, bool isFilenamePmem = false, void* testState = nullptr) {
  FileHelper::Filename fname(filename, isFilenamePmem);
  char resolvedFilename[FileHelper::MAX_FILENAME_LENGTH];
  bool result = false;
  do {
    if (!_fileHelper.canonicalFilename(fname, resolvedFilename, FileHelper::MAX_FILENAME_LENGTH)) break;
    if (_storageProvider._exists(resolvedFilename, testState)) {
      if (!_storageProvider._loadFromStream(resolvedFilename, dto, testState)) break;
    }
    result = true;
  } while (false);
  return result;
}

bool SDStorage::load(const __FlashStringHelper* filename, StreamableDTO* dto, void* testState = nullptr) {
  return load(reinterpret_cast<const char*>(filename), dto, true, testState);
}

/*
 * Writes the DTO data with data to a file (after prepending the root dir on
 * the filename if necessary). If no transaction is provided, the write is
 * auto-committed.
 */
bool SDStorage::save(const char* filename, StreamableDTO* dto, Transaction* txn = nullptr, bool isFilenamePmem = false) {
  return save(nullptr, filename, dto, txn, isFilenamePmem);
}

bool SDStorage::save(void* testState, const char* filename, StreamableDTO* dto, Transaction* txn = nullptr, bool isFilenamePmem = false) {
  // Reading a newer format into old code is safe since StreamableDTO stores unrecognized
  // fields in a hashmap and can even pipe them, but any newer logic will be missing,
  // which could corrupt data, so do not save newer format dtos.
  if (dto->getTypeId() != -1 && (dto->getSerialVersion() < dto->getDeserializedVersion())) {
#if (defined(DEBUG))
    Serial.print("Cannot write v");
    Serial.print(dto->getDeserializedVersion());
    Serial.print(" object with v");
    Serial.print(dto->getSerialVersion());
    Serial.print(" custom DTO (typeId=");
    Serial.print(dto->getTypeId());
    Serial.println(")");
#endif
    return false;
  }

  FileHelper::Filename fname(filename, isFilenamePmem);
  char resolvedFilename[FileHelper::MAX_FILENAME_LENGTH];
  bool result = false;
  bool implicitTx = false;
  do {
    if (!_fileHelper.canonicalFilename(fname, resolvedFilename, FileHelper::MAX_FILENAME_LENGTH)) break;
    if (txn == nullptr) {
      // make an implicit, single-file transaction
      txn = beginTxn(testState, resolvedFilename);
      if (!txn) break;
      implicitTx = true;
    }
    char* tmpFilename = _txnManager->getTmpFilename(txn, resolvedFilename);  
    if (!tmpFilename || strlen(tmpFilename) == 0) break;
    if (!_storageProvider._writeToStream(tmpFilename, dto, testState)) break;
    result = true;
  } while (false);
  if (result && implicitTx) {
    result = commitTxn(txn, testState);
  }
  return result;
}

bool SDStorage::save(const __FlashStringHelper* filename, StreamableDTO* dto, Transaction* txn = nullptr) {
  return save(nullptr, filename, dto, txn);
}

bool SDStorage::save(void* testState, const __FlashStringHelper* filename, StreamableDTO* dto, Transaction* txn = nullptr) {
  return save(testState, reinterpret_cast<const char*>(filename), dto, txn, true);
}

/*
 * Returns true if the file exists (after prepending the root dir on the 
 * filename if necessary)
 */
bool SDStorage::exists(const char* filename, bool isFilenamePmem = false, void* testState = nullptr) {
  FileHelper::Filename fname(filename, isFilenamePmem);
  char resolvedFilename[FileHelper::MAX_FILENAME_LENGTH];
  bool result = false;
  do {
    if (!_fileHelper.canonicalFilename(fname, resolvedFilename, FileHelper::MAX_FILENAME_LENGTH)) break;
    if (!_storageProvider._exists(resolvedFilename, testState)) break;
    result = true;
  } while (false);
  return result;
}

bool SDStorage::exists(const __FlashStringHelper* filename, void* testState = nullptr) {
  return exists(reinterpret_cast<const char*>(filename), true, testState);
}

bool SDStorage::exists_P(const char* filename, void* testState = nullptr) {
  return exists(filename, true, testState);
}

/*
 * Deletes a file (after prepending the root dir on the filename if necessary).
 * If no transaction is provided, the write is auto-committed.
 */
bool SDStorage::erase(const char* filename, bool isFilenamePmem = false, Transaction* txn = nullptr) {
  if (!filename) return false;
  return erase(nullptr, filename, isFilenamePmem, txn);
}

bool SDStorage::erase(const __FlashStringHelper* filename, Transaction* txn = nullptr) {
  if (!filename) return false;
  return erase(nullptr, filename, txn);
}

bool SDStorage::erase_P(const char* filename, Transaction* txn = nullptr) {
  if (!filename) return false;
  return erase(nullptr, filename, true, txn);
}

bool SDStorage::erase(void* testState, const char* filename, bool isFilenamePmem = false, Transaction* txn = nullptr) {
  FileHelper::Filename fname(filename, isFilenamePmem);
  char resolvedFilename[FileHelper::MAX_FILENAME_LENGTH];
  bool result = false;
  do {
    if (!_fileHelper.canonicalFilename(fname, resolvedFilename, FileHelper::MAX_FILENAME_LENGTH)) break;
    if (!_storageProvider._exists(resolvedFilename, testState)) break;

    if (txn) {
      char* tmpFilename = _txnManager->getTmpFilename(txn, resolvedFilename);
      if (!tmpFilename || strlen(tmpFilename) == 0) break;
      txn->put(resolvedFilename, _SDSTORAGE_TOMBSTONE, false, true); // tombstone the filename
      char txnFilename[FileHelper::MAX_FILENAME_LENGTH];
      if (!txn->getFilename(txnFilename, FileHelper::MAX_FILENAME_LENGTH)) break;
      if (!_storageProvider._writeTxnToStream(txnFilename, txn, testState)) break;
      result = true;

    } else if (!_storageProvider._remove(resolvedFilename, testState)) break;
    result = true;

  } while (false);
  return result;
}

bool SDStorage::erase(void* testState, const __FlashStringHelper* filename, Transaction* txn = nullptr) {
  return erase(testState, reinterpret_cast<const char*>(filename), true, txn);
}

bool SDStorage::erase_P(void* testState, const char* filename, Transaction* txn = nullptr) {
  return erase(testState, filename, true, txn);
}
/*
 * Creates a new directory (after prepending the root dir on the 
 * dirName if necessary). Returns true if successful.
 */
bool SDStorage::mkdir(const char* dirName, bool isDirNamePmem = false, void* testState = nullptr) {
  FileHelper::Filename dname(dirName, isDirNamePmem);
  char resolvedName[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper.canonicalFilename(dname, resolvedName, FileHelper::MAX_FILENAME_LENGTH)) return false;
  return _storageProvider._mkdir(resolvedName, testState);
}

bool SDStorage::mkdir(const __FlashStringHelper* dirName, void* testState = nullptr) {
  return mkdir(reinterpret_cast<const char*>(dirName), true, testState);
}

bool SDStorage::mkdir_P(const char* dirName, void* testState = nullptr) {
  return mkdir(dirName, true, testState);
}
