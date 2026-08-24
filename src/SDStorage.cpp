/*

  SDStorage.cpp

  SD card storage manager for StreamableDTOs with index and transaction support

  Copyright (c) 2025, Dan Mowehhuk (danmowehhuk@gmail.com)
  All rights reserved.

*/

#include "SDStorage.h"
#include "hal/SDStorageHal.h"
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
    if (!_storageProvider._exists(_fileHelper.getRootDir(), testState)) {
      if (!_storageProvider._mkdir(_fileHelper.getRootDir(), testState)) break;
    } 
    if (!_storageProvider._exists(_fileHelper.getIdxDir(), testState)) {
      if (!_storageProvider._mkdir(_fileHelper.getIdxDir(), testState)) break;
    } 
    if (!_storageProvider._exists(_fileHelper.getWorkDir(), testState)) {
      if (!_storageProvider._mkdir(_fileHelper.getWorkDir(), testState)) break;
    } 
    if (!fsck()) {
#if defined(DEBUG)
      SDStorageHal::println(F("SDStorage repair failed"));
#endif
      break;
    }
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

bool SDStorage::load(const FlashStr* filename, StreamableDTO* dto, void* testState = nullptr) {
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
    SDStorageHal::print(F("Cannot write v"));
    SDStorageHal::print(dto->getDeserializedVersion());
    SDStorageHal::print(F(" object with v"));
    SDStorageHal::print(dto->getSerialVersion());
    SDStorageHal::print(F(" custom DTO (typeId="));
    SDStorageHal::print(dto->getTypeId());
    SDStorageHal::println(F(")"));
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

bool SDStorage::save(const FlashStr* filename, StreamableDTO* dto, Transaction* txn = nullptr) {
  return save(nullptr, filename, dto, txn);
}

bool SDStorage::save(void* testState, const FlashStr* filename, StreamableDTO* dto, Transaction* txn = nullptr) {
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

bool SDStorage::exists(const FlashStr* filename, void* testState = nullptr) {
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

bool SDStorage::erase(const FlashStr* filename, Transaction* txn = nullptr) {
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

bool SDStorage::erase(void* testState, const FlashStr* filename, Transaction* txn = nullptr) {
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

bool SDStorage::mkdir(const FlashStr* dirName, void* testState = nullptr) {
  return mkdir(reinterpret_cast<const char*>(dirName), true, testState);
}

bool SDStorage::mkdir_P(const char* dirName, void* testState = nullptr) {
  return mkdir(dirName, true, testState);
}


/******
 * 
 * Cleans up the _workDir on initialization in case any transactions were
 * left after a power interruption. Finalized transactions are completed, and
 * all others are rolled back.
 * 
 ******/

#if (!defined(__SDSTORAGE_TEST) && defined(NO_ARDUINO))
/*
 * Same recovery logic as the SdFat branch below, ported to FatFs's
 * directory API (no SdFat::File::openNextFile() equivalent in the
 * NO_ARDUINO hal/File - directory listing goes through f_opendir/
 * f_readdir/f_closedir directly instead).
 */
bool SDStorage::fsck() {
  StreamableManager* _streams = &(_storageProvider._streams);
  FATFS_DIR dir;
  if (f_opendir(&dir, _fileHelper.getWorkDir()) != FR_OK) {
#if (defined(DEBUG))
    SDStorageHal::print(F("ERROR: SDStorage::fsck() - Could not open work dir: "));
    SDStorageHal::println(_fileHelper.getWorkDir());
#endif
    return false;
  }
  bool notifyDirty = true;
  const char* commitExtension = strdup_P(_SDSTORAGE_TXN_COMMIT_EXTSN);
  static const char fmt[] PROGMEM = "%s/%s";
  while (true) {
    FILINFO fno;
    if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break; // no more files or error
    if (notifyDirty) {
#if (defined(DEBUG))
      SDStorageHal::println(F("SDStorage::fsck() - Recovering filesystem..."));
#endif
      notifyDirty = false;
    }
    char filename[FileHelper::MAX_FILENAME_LENGTH];
    snprintf_P(filename, FileHelper::MAX_FILENAME_LENGTH, fmt, _fileHelper.getWorkDir(), fno.fname);

    if (endsWith(filename, commitExtension)) {
      // Leftover commit file needs to be applied
      Transaction* txn = new Transaction(&_fileHelper, filename);
      File file;
      if (file.open(filename, FA_READ)) {
        _streams->load(&file, txn);
        file.close();
      }
      bool commitErr = true;
      do {
#if (defined(DEBUG))
        SDStorageHal::print(F("  Applying finalized transaction: "));
        SDStorageHal::print(filename);
#endif
        if (!_txnManager->applyChanges(txn)) {
#if (defined(DEBUG))
          SDStorageHal::println(F(" - FAILED"));
#endif
          break;
        }
        _txnManager->cleanupTxn(txn);
        commitErr = false;
#if (defined(DEBUG))
        SDStorageHal::println(F(" - SUCCESS"));
#endif
      } while (false);
      if (commitErr) {
        /*
         * Something failed that should not have failed. This means the files might
         * be in an inconsistent state. Call the supplied errFunction.
         */
#if defined(DEBUG)
        SDStorageHal::print(F("ERROR: SDStorage::fsck() - Failed to apply commit "));
        SDStorageHal::println(filename);
#endif
        if (_errFunction != nullptr) _errFunction();
        f_closedir(&dir);
        free(commitExtension);
        return false;
      }
    }
  }
  free(commitExtension);
  f_closedir(&dir);

  // All commits successfully applied, so delete all remaining files.
  // This effectively aborts any incomplete transactions.
  if (f_opendir(&dir, _fileHelper.getWorkDir()) != FR_OK) {
#if (defined(DEBUG))
    SDStorageHal::print(F("ERROR: SDStorage::fsck() - Could not reopen work dir: "));
    SDStorageHal::println(_fileHelper.getWorkDir());
#endif
    return false;
  }
  while (true) {
    FILINFO fno;
    if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break; // no more files or error
    char filename[FileHelper::MAX_FILENAME_LENGTH];
    snprintf_P(filename, FileHelper::MAX_FILENAME_LENGTH, fmt, _fileHelper.getWorkDir(), fno.fname);
#if (defined(DEBUG))
    SDStorageHal::print(F("  Cleaning up: "));
    SDStorageHal::print(filename);
#endif
    if (f_unlink(filename) != FR_OK) {
#if (defined(DEBUG))
      SDStorageHal::println(F(" - FAILED"));
#endif
      /*
       * Cleanup failed, but any outstanding commits were applied, so nothing
       * is in an inconsistent state, although the workdir needs to be cleaned up
       * to prevent tmp file name collisions.
       */
#if defined(DEBUG)
      SDStorageHal::print(F("ERROR: SDStorage::fsck() - Failed to clean up work dir "));
      SDStorageHal::println(_fileHelper.getWorkDir());
#endif
      if (_errFunction != nullptr) _errFunction();
      f_closedir(&dir);
      return false;
    } else {
#if (defined(DEBUG))
      SDStorageHal::println(F(" - SUCCESS"));
#endif
    }
  }
  f_closedir(&dir);
  return true;
}
#else
bool SDStorage::fsck() {
#if (!defined(__SDSTORAGE_TEST))
  SdFat* _sd = &(_storageProvider._sd);
  StreamableManager* _streams = &(_storageProvider._streams);
  File workDirFile = _sd->open(_fileHelper.getWorkDir());
  if (!workDirFile) {
#if (defined(DEBUG))
    SDStorageHal::print(F("ERROR: SDStorage::fsck() - Could not open work dir: "));
    SDStorageHal::println(_fileHelper.getWorkDir());
#endif
    return false;
  }
  if (!workDirFile.isDirectory()) {
#if (defined(DEBUG))
    SDStorageHal::print(F("ERROR: SDStorage::fsck() - Not a directory: "));
    SDStorageHal::println(_fileHelper.getWorkDir());
#endif
    return false;
  }
  bool notifyDirty = true;
  const char* commitExtension = strdup_P(_SDSTORAGE_TXN_COMMIT_EXTSN);
  static const char fmt[] PROGMEM = "%s/%s"; 
  while (true) {
    File file = workDirFile.openNextFile();
    if (!file) break; // no more files
    if (notifyDirty) {
#if (defined(DEBUG))
      SDStorageHal::println(F("SDStorage::fsck() - Recovering filesystem..."));
#endif
      notifyDirty = false;
    }
    char shortname[13];
    file.getName(shortname, 13);
    char filename[FileHelper::MAX_FILENAME_LENGTH];
    snprintf_P(filename, FileHelper::MAX_FILENAME_LENGTH, fmt, _fileHelper.getWorkDir(), shortname);

    if (endsWith(filename, commitExtension)) {
      // Leftover commit file needs to be applied
      Transaction* txn = new Transaction(&_fileHelper, filename);
      _streams->load(&file, txn);
      file.close();
      bool commitErr = true;
      do {
#if (defined(DEBUG))
        SDStorageHal::print(F("  Applying finalized transaction: "));
        SDStorageHal::print(filename);
#endif
        if (!_txnManager->applyChanges(txn)) {
#if (defined(DEBUG))
          SDStorageHal::println(F(" - FAILED"));
#endif
          break;
        }
        _txnManager->cleanupTxn(txn);
        commitErr = false;
#if (defined(DEBUG))
        SDStorageHal::println(F(" - SUCCESS"));
#endif
      } while (false);
      if (commitErr) {
        /*
         * Something failed that should not have failed. This means the files might
         * be in an inconsistent state. Call the supplied errFunction.
         */
#if defined(DEBUG)
        SDStorageHal::print(F("ERROR: SDStorage::fsck() - Failed to apply commit "));
        SDStorageHal::println(filename);
        delay(250); // Allow message to print before potentially crashing
#endif
        if (_errFunction != nullptr) _errFunction();
        return false;
      }
    }
  }
  free(commitExtension);
  workDirFile.close();

  // All commits successfully applied, so delete all remaining files.
  // This effectively aborts any incomplete transactions.
  workDirFile = _sd->open(_fileHelper.getWorkDir());
  while (true) {
    File file = workDirFile.openNextFile();
    if (!file) break; // no more files
    char shortname[13];
    file.getName(shortname, 13);
    char filename[FileHelper::MAX_FILENAME_LENGTH];
    snprintf_P(filename, FileHelper::MAX_FILENAME_LENGTH, fmt, _fileHelper.getWorkDir(), shortname);
#if (defined(DEBUG))
    SDStorageHal::print(F("  Cleaning up: "));
    SDStorageHal::print(filename);
#endif
    if (!_sd->remove(filename)) {
#if (defined(DEBUG))
      SDStorageHal::println(F(" - FAILED"));
#endif
      /*
       * Cleanup failed, but any outstanding commits were applied, so nothing
       * is in an inconsistent state, although the workdir needs to be cleaned up
       * to prevent tmp file name collisions.
       */
#if defined(DEBUG)
      SDStorageHal::print(F("ERROR: SDStorage::fsck() - Failed to clean up work dir "));
      SDStorageHal::println(_fileHelper.getWorkDir());
      delay(250); // Allow message to print before potentially crashing
#endif
      if (_errFunction != nullptr) _errFunction();
      return false;
    } else {
#if (defined(DEBUG))
      SDStorageHal::println(F(" - SUCCESS"));
#endif
    }
  }
  workDirFile.close();
#endif
  return true;
}
#endif

