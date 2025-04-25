#include "TransactionManager.h"

// bool TransactionManager::commitTxn(Transaction* txn, void* testState = nullptr) {
//   char* oldName = txn->getFilename();
//   txn->setCommitted();
//   char* newName = txn->getFilename();
//   bool commitErr = true;
//   do {
//     if (!_storageProvider->_rename(oldName, newName, testState)) break;
// //    if (!_applyChanges(txn, testState)) break;
//     delete[] oldName;
//     delete[] newName;
//     cleanupTxn(txn, testState);
//     commitErr = false;
//   } while (false);
//   if (commitErr) {
//     /*
//      * Something failed that should not have failed. This means the files might
//      * be in an inconsistent state. Call the supplied errFunction.
//      */
// #if defined(DEBUG)
//     Serial.println(F("ERROR: TransactionManager::commitTxn"));
// #endif
// //    if (_errFunction != nullptr) _errFunction();
//     return false;
//   }
//   return true;
// }

bool TransactionManager::abortTxn(Transaction* txn, void* testState = nullptr) {

  auto cleanupFunction = [](const char* filename, const char* tmpFilename, bool keyPmem, bool valPmem, void* capture) -> bool {
    TransactionCapture* c = static_cast<TransactionCapture*>(capture);
    if (strcmp_P(tmpFilename, _SDSTORAGE_TOMBSTONE) == 0) {
      // Tombstone - nothing to clean up
    } else if (c->storageProvider->_exists(tmpFilename, c->ts)) {
      if (!c->storageProvider->_remove(tmpFilename, c->ts)) {
#if defined(DEBUG)
        Serial.print(tmpFilename);
        Serial.println(F(" could not be removed"));
#endif
        c->success = false;
        return false;
      }
    }
    return true;
  };

  TransactionCapture capture(_storageProvider, testState);
  txn->processEntries(cleanupFunction, &capture);
  if (!capture.success) {
    /*
     * At least one tmp file was not cleaned up. This should not cause
     * problems. They should be cleaned up by fsck() on the next startup.
     */
#if defined(DEBUG)
    Serial.println(F("TransactionManager abortTxn failed"));
#endif
  }
  cleanupTxn(txn, testState);
  return capture.success;
}

/*
 * Locks a file to the transaction, creating a temp file for writing
 */
bool TransactionManager::addFileToTxn(Transaction* txn, void* testState, const char* filename, bool isPmem = false) {
  char* resolvedFilename = _fileHelper->canonicalFilename(filename, isPmem);
  boolean result = true;
  if (!_storageProvider->_exists(resolvedFilename, testState)) {
    do {
      // Validate new file to be created
      char* shortFilename = FileHelper::getFilenameFromFullName(resolvedFilename);
      bool validFAT16Filename = FileHelper::isValidFAT16Filename(shortFilename);
      if (!validFAT16Filename) {
#if defined(DEBUG)
        Serial.print(F("Invalid FAT16 filename: "));
        Serial.println(shortFilename);
#endif
        result = false;
      }
      delete[] shortFilename;
      if (!result) break;

      char* path = FileHelper::getPathFromFilename(resolvedFilename);
      bool pathExists = _storageProvider->_exists(path, testState);
      bool pathIsDir = _storageProvider->_isDir(path, testState);
      if (!pathExists) {
#if defined(DEBUG)
        Serial.print(F("Directory does not exist: "));
        Serial.println(path);
#endif
        result = false;
      }

      if (!pathIsDir) {
#if defined(DEBUG)
        Serial.print(F("Not a directory: "));
        Serial.println(path);
#endif
        result = false;
      }
      delete[] path;
    } while (false);
  }

  if (result) {
    txn->add(resolvedFilename, _fileHelper->getWorkDir());
    char* tmpFilename = txn->getTmpFilename(resolvedFilename);
    if (tmpFilename && _storageProvider->_exists(tmpFilename, testState)) {
  #if defined(DEBUG)
      Serial.print(F("Transaction file already exists: "));
      Serial.println(tmpFilename);
  #endif
      result = false;
    }
  }
  delete[] resolvedFilename;
  return result;
}

char* TransactionManager::getTmpFilename(Transaction* txn, const char* filename, bool isPmem = false) {
  char* tmpFilename = txn->getTmpFilename(filename, isPmem);
  if (!tmpFilename) {
#if defined(DEBUG)
    if (isPmem) {
      Serial.print(reinterpret_cast<const __FlashStringHelper*>(filename));
    } else {
      Serial.print(filename);
    }
    Serial.println(F(" not part of this transaction"));
#endif
    return nullptr;
  } else if (strcmp_P(tmpFilename, _SDSTORAGE_TOMBSTONE) == 0) {
#if defined(DEBUG)
    if (isPmem) {
      Serial.print(reinterpret_cast<const __FlashStringHelper*>(filename));
    } else {
      Serial.print(filename);
    }
    Serial.println(F(" is marked for delete"));
#endif
    return nullptr;
  }
  return tmpFilename;
}

// char* TransactionManager::getTmpFilename(Transaction* txn, const __FlashStringHelper* filename) {
//   return getTmpFilename(txn, reinterpret_cast<const char*>(filename), true);
// }

void TransactionManager::cleanupTxn(Transaction* txn, void* testState = nullptr) {
  char* txnFilename = txn->getFilename();
  if (!_storageProvider->_remove(txnFilename, testState)) {
#if defined(DEBUG)
    Serial.print(F("Could not remove "));
    Serial.println(txnFilename);
#endif
  }
  delete[] txnFilename;
  delete txn;
}
