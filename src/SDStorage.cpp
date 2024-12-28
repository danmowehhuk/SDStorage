#include <StreamableDTO.h>
#include "SDStorage.h"
#include "Transaction.h"


bool SDStorage::begin(void* testState = nullptr) {
  _rootDir.trim();
  bool sdInit = false;
  do {
    if (!_sd.begin(_sdCsPin)) break;
    if (_rootDir.indexOf('/') != -1) break; // no slashes in root dir
    _rootDir = String(F("/")) + _rootDir;
    if (!_exists(_rootDir, testState) && !_mkdir(_rootDir, testState)) break;
    _idxDir = realFilename(reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_IDX_DIR));
    if (!_exists(_idxDir, testState) && !_mkdir(_idxDir, testState)) break;
    _workDir = realFilename(reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_WORK_DIR));
    if (!_exists(_workDir, testState) && !_mkdir(_workDir, testState)) break;
    if (!fsck()) {
#if defined(DEBUG)
      Serial.println(String(F("SDStorage repair failed")));
#endif
      break;
    }
    sdInit = true;
  } while (false);
  return sdInit;
}

String SDStorage::realFilename(const String& filename) {
  if (filename.startsWith(_rootDir + String(F("/")))) return filename;
  String resolvedName;
  bool addSlash = false;
  if (!filename.startsWith(String(F("/")))) {
    addSlash = true;
  }
  resolvedName.reserve(_rootDir.length() + (addSlash ? 1 : 0) + filename.length());
  resolvedName = _rootDir + (addSlash ? String(F("/")):String()) + filename;
  return resolvedName;
}

String SDStorage::indexFilename(const String &idxName) {
  String idxFilename;
  String ext = String(reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_INDEX_EXTSN));
  idxFilename.reserve(_idxDir.length() + 1 + idxName.length() + ext.length());
  idxFilename = _idxDir + String(F("/")) + idxName + ext;
  return idxFilename;
}

bool SDStorage::exists(const String &filename, void* testState = nullptr) {
  return _exists(realFilename(filename), testState);
}

bool SDStorage::mkdir(const String &dirName, void* testState = nullptr) {
  return _mkdir(realFilename(dirName), testState);
}

bool SDStorage::load(const String &filename, StreamableDTO* dto, void* testState = nullptr) {
  String resolvedFilename = realFilename(filename);
  if (!_exists(resolvedFilename, testState)) {
    return false;
  }
  return _loadFromStream(resolvedFilename, dto, testState);
}

bool SDStorage::save(const String &filename, StreamableDTO* dto, Transaction* txn = nullptr, void* testState = nullptr) {
  String resolvedFilename = realFilename(filename);
  bool implicitTx = false;
  if (txn == nullptr) {
    // make an implicit, single-file transaction
    txn = beginTxn(testState, resolvedFilename);
    implicitTx = true;
  }
  String tmpFilename = getTmpFilename(txn, resolvedFilename);
  if (tmpFilename.length() == 0) return false;
  _writeToStream(tmpFilename, dto, testState);
  if (implicitTx) return commitTxn(txn, testState);
  return true;
}

bool SDStorage::erase(const String &filename, void* testState = nullptr) {
  return erase(nullptr, filename, testState);
}

bool SDStorage::erase(Transaction* txn, const String &filename, void* testState = nullptr) {
  String resolvedFilename = realFilename(filename);
  if (!_exists(resolvedFilename, testState)) {
    return false;
  }
  if (txn != nullptr) {
    String tmpFilename = getTmpFilename(txn, resolvedFilename);
    if (tmpFilename.length() == 0) return false;
    txn->put(resolvedFilename.c_str(), _SDSTORAGE_TOMBSTONE, false, true); // tombstone the filename
    _writeTxnToStream(txn->getFilename(), txn, testState);
    return true;
  } else {
    return _remove(resolvedFilename, testState);
  }
}

// bool SDStorage::idxPrefixSearch(const String &idxName, const String &prefix) { //, IndexEntry<T>* resultHead);

//   // TODO

//   return false;
// } 

// bool SDStorage::idxHasKey(const String &idxName, const String &key) {
//   struct State {
//     const String& key;
//     bool hasKey = false;
//     State(const String& key): key(key) {};
//   };
//   key.trim();
//   State state(key);
//   auto filter = [](const String &line, StreamableManager::DestinationStream* dest, void* statePtr) -> bool {
//     State* state = static_cast<State*>(statePtr);
//     if (extractKey(line).equals(state->key)) {
//       state->hasKey = true;
//       return false; // stop scanning
//     }
//     return true; // keep going
//   };
//   return (_scanIndex(idxName, filter, &state) && state.hasKey);
// }

bool SDStorage::idxUpsert(const String &idxName, const String &key, const String &value, Transaction* txn = nullptr) {
  return idxUpsert(nullptr, idxName, key, value, txn);
}

bool SDStorage::idxUpsert(void* testState, const String &idxName, const String &key, const String &value, Transaction* txn = nullptr) {


//   struct State {
//     const String& key;
//     const String& value;
//     String prevKey = String();
//     State(const String& key, const String& value): key(key), value(value) {};
//   };
//   State state(key, value);

  // TODO preserve key order

  //  auto filter = [](const String &line, StreamableManager::DestinationStream* dest, void* statePtr) -> bool {
  //   int separatorIndex = line.indexOf('=');
  //   String k = line.substring(0, separatorIndex);
  //   k.trim();
  //   String v = line.substring(separatorIndex + 1);
  //   v.trim();
  //   if (k.equals(key)) {
  //     // update the value
  //     dest->println(key + String(F("=")) + value);
  //   } else {
  //     // copy the line unchanged
  //     dest->println(line);
  //   }
  //   return true;
  // };
  // return _updateIndex(idxName, filter, txn);

  return false;
}

// bool SDStorage::idxRemove(const String &idxName, const String &key, Transaction* txn = nullptr) {
//   struct State {
//     const String& key;
//     State(const String& key): key(key) {};
//   };
//   State state(key);
//   auto filter = [](const String &line, StreamableManager::DestinationStream* dest, void* statePtr) -> bool {
//     State* state = static_cast<State*>(statePtr);
//     String key = extractKey(line);
//     if (key.equals(state->key)) { /* skip it */ } else { dest->println(line); }
//     return true;
//   };
//   return _updateIndex(idxName, filter, &state, txn);
// }

// bool SDStorage::idxRename(const String &idxName, const String &oldKey, const String &newKey, Transaction* txn = nullptr) {

//   // TODO: check that new key name doesn't exist already

//   // TODO: insert keeping the keys sorted, remove oldKey

//   return false;
// }

// bool SDStorage::_updateIndex(const String &idxName, StreamableManager::FilterFunction filter, void* statePtr, Transaction* txn = nullptr) {
//   String idxFilename = indexFilename(idxName);
//   bool implicitTx = false;
//   if (txn == nullptr) {
//     // make an implicit, single-file transaction
//     txn = beginTxn(idxFilename);
//     implicitTx = true;
//   }
//   String tmpFilename = txn->getTmpFilename(idxFilename);
//   if (tmpFilename.length() == 0) {
// #if defined(DEBUG)
//     Serial.println(idxFilename + String(F(" not part of this transaction")));
// #endif
//     return false;
//   }
//   bool updateSuccess = false;
//   do {
//     File inFile = _sd.open(idxFilename, FILE_READ);
//     File outFile = _sd.open(tmpFilename, FILE_WRITE);
//     if (!outFile) break;
//     if (!inFile) {
//       // first write to this index, pass an empty string to the filter function
//       StreamableManager::DestinationStream destStream(&outFile);
//       filter(String(), &destStream, statePtr);
//     } else {
//       _streams.pipe(&inFile, &outFile, filter, statePtr);
//       filter(String(), &outFile, statePtr); // optional last line
//       inFile.close();
//     }
//     outFile.close();
//     if (implicitTx) {
//       if (!commitTxn(txn)) {
//   #if defined(DEBUG)
//         Serial.println(String(F("Index update failed for: ")) + idxFilename);
//   #endif
//         abortTxn(txn);
//         return false;
//       }
//     }
//     updateSuccess = true;
//   } while (false);
//   return updateSuccess;
// }

// bool SDStorage::_scanIndex(const String &idxName, StreamableManager::FilterFunction filter, void* statePtr) {
//   String idxFilename = indexFilename(idxName);
//   File idxFile = _sd.open(idxFilename, FILE_READ);
//   if (!idxFile) return false;
//   _streams.pipe(&idxFile, nullptr, filter, statePtr);
//   return true;
// }

// String SDStorage::extractKey(const String& line) {
//   int separatorIndex = line.indexOf('=');
//   String key = line.substring(0, separatorIndex);
//   key.trim();
//   return key;
// }

// String SDStorage::extractValue(const String& line) {
//   int separatorIndex = line.indexOf('=');
//   String value = line.substring(separatorIndex + 1);
//   value.trim();
//   return value;
// }

// String SDStorage::toLine(const String& key, const String& value) {
//   key.trim();
//   value.trim();
//   return key + String(F("=")) + value;
// }

bool SDStorage::commitTxn(Transaction* txn, void* testState = nullptr) {
  String oldName = txn->getFilename();
  txn->setCommitted();
  String newName = txn->getFilename();
  bool commitErr = true;
  do {
    if (!_rename(oldName, newName, testState)) break;
    if (!_applyChanges(txn, testState)) break;
    _cleanupTxn(txn, testState);
    commitErr = false;
  } while (false);
  if (commitErr) {
#if defined(DEBUG)
    Serial.println(String(F("commitTxn failed")));
#endif
    if (!abortTxn(txn, testState)) {
#if defined(DEBUG)
      Serial.println(String(F("abortTxn ALSO failed!")));
#endif
    }
    return false;
  }
  return true;
}

bool SDStorage::abortTxn(Transaction* txn, void* testState = nullptr) {
  TransactionCapture capture(this, testState);
  auto cleanupFunction = [](const String& filename, const String& tmpFilename, void* capture) -> bool {
    TransactionCapture* c = static_cast<TransactionCapture*>(capture);
    if (strcmp_P(tmpFilename.c_str(), _SDSTORAGE_TOMBSTONE) == 0) {
      // Tombstone - nothing to clean up
    } else if (c->sdStorage->_exists(tmpFilename, c->ts)) {
      if (!c->sdStorage->_remove(tmpFilename, c->ts)) {
#if defined(DEBUG)
        Serial.println(tmpFilename + String(F(" could not be removed")));
#endif
        c->success = false;
        return false;
      }
    }
    return true;
  };
  txn->processEntries(cleanupFunction, &capture);
  if (!capture.success) {
    return false;
  }
  _cleanupTxn(txn, testState);
  return true;
}

bool SDStorage::_applyChanges(Transaction* txn, void* testState = nullptr) {
  TransactionCapture capture(this, testState);
  auto applyChangesFunction = [](const String& filename, const String& tmpFilename, void* capture) -> bool {
    TransactionCapture* c = static_cast<TransactionCapture*>(capture);
    if (strcmp_P(tmpFilename.c_str(), _SDSTORAGE_TOMBSTONE) == 0) {
      // tombstone - delete the file
      c->sdStorage->_remove(filename, c->ts);
    } else if (!c->sdStorage->_exists(tmpFilename, c->ts)) {
      // No changes to apply (tmpFile was never written)
    } else {
      if (c->sdStorage->_exists(filename, c->ts) && !c->sdStorage->_remove(filename, c->ts)) {
#if defined(DEBUG)
        Serial.println(filename + String(F(" could not be removed")));
#endif
        c->success = false;
        return false;
      }
      if (!c->sdStorage->_rename(tmpFilename, filename, c->ts)) {
#if defined(DEBUG)
        Serial.println(String(F("Could not move ")) + tmpFilename
              + String(F(" to ")) + filename);
#endif
        c->success = false;
        return false;
      }
    }
    return true;
  };
  txn->processEntries(applyChangesFunction, &capture);
  if (!capture.success) {
#if defined(DEBUG)
    /*
     * One of the changes was not successfully applied. This leaves the
     * data in an inconsistent state that must be manually repaired.
     *
     * If this ever happens, it's a bug!
     */
    Serial.println(String(F("ERROR: All transaction changes NOT applied!")));
#endif
    return false;
  }
  return true;
}

void SDStorage::_cleanupTxn(Transaction* txn, void* testState = nullptr) {
  txn->releaseLocks();
  if (!_remove(txn->getFilename(), testState)) {
#if defined(DEBUG)
    Serial.println(String(F("Could not remove ")) + txn->getFilename());
#endif
  }
  delete txn;
}

bool SDStorage::fsck() {
//   File workDirFile = _sd.open(workDir());
//   if (!workDirFile) return false;
//   if (!workDirFile.isDirectory()) return false;
//   while (true) {
//     File file = workDirFile.openNextFile();
//     if (!file) break; // no more files
//     char buffer[64];
//     file.getName(buffer, sizeof(buffer));
//     String filename = String(buffer);
//     if (filename.endsWith(StreamableDTO::pgmToString(TransactionStrings::COMMIT_EXTSN))) {
//       // Leftover commit file needs to be applied
//       Transaction* txn = new Transaction(workDir());
//       _streams.load(&file, txn);
//       file.close();
//       txn->setCommitted();
//       if (!commitTxn(txn)) return false;
//     }
//   }

  // All commits successfully applied, so delete all remaining files.
  // This effectively aborts any incomplete transactions.
//   workDirFile.close();
//   workDirFile = _sd.open(workDir());
//   while (true) {
//     File file = workDirFile.openNextFile();
//     if (!file) break; // no more files
//     char buffer[64];
//     file.getName(buffer, sizeof(buffer));
//     String filename = String(buffer);
//     if (!_sd.remove(filename)) return false;
//   }
  return true;
}

String SDStorage::getTmpFilename(Transaction* txn, const String& filename) {
  String tmpFilename = txn->getTmpFilename(filename);
  if (tmpFilename.length() == 0) {
#if defined(DEBUG)
    Serial.println(filename + String(F(" not part of this transaction")));
#endif
    return String();
  } else if (strcmp_P(tmpFilename.c_str(), _SDSTORAGE_TOMBSTONE) == 0) {
#if defined(DEBUG)
    Serial.println(filename + String(F(" is marked for delete")));
#endif
    return String();
  }
  return tmpFilename;
}

/******
 * 
 * The following wrapper methods allow sending a state object to a 
 * mock version of SdFat when testing on a simulator. These all expect
 * the absolute filename as returned by realFilename(...)
 * 
 ******/

bool SDStorage::_exists(const String& filename, void* testState = nullptr) {
#if defined(__SDSTORAGE_TEST)
  return _sd.exists(filename, testState);
#else
  return _sd.exists(filename);
#endif
}

bool SDStorage::_mkdir(const String& filename, void* testState = nullptr) {
#if defined(__SDSTORAGE_TEST)
  return _sd.mkdir(filename, testState);
#else
  return _sd.mkdir(filename);
#endif
}

bool SDStorage::_loadFromStream(const String& filename, StreamableDTO* dto, void* testState = nullptr) {
  Stream* src;
#if defined(__SDSTORAGE_TEST)
  src = _sd.loadFileStream(filename, testState);
#else
  File file = _sd.open(filename, FILE_READ);
  src = &file;
#endif
  bool result = _streams.load(src, dto);  
#if defined(__SDSTORAGE_TEST)
  delete src;
#else
  file.close();
#endif
  return result;
}

void SDStorage::_writeToStream(const String& filename, StreamableDTO* dto, void* testState = nullptr) {
  Stream* dest;
#if defined(__SDSTORAGE_TEST)
  dest = _sd.writeFileStream(filename, testState);
#else
  File file = _sd.open(filename, FILE_WRITE);
  dest = &file;
#endif
  _streams.send(dest, dto);  
#if (!defined(__SDSTORAGE_TEST))
  file.close();
#endif
}

void SDStorage::_writeTxnToStream(const String& filename, StreamableDTO* dto, void* testState = nullptr) {
  Stream* dest;
#if defined(__SDSTORAGE_TEST)
  dest = _sd.writeTxnFileStream(filename, testState);
#else
  File file = _sd.open(filename, FILE_WRITE);
  dest = &file;
#endif
  _streams.send(dest, dto);  
#if (!defined(__SDSTORAGE_TEST))
  file.close();
#endif
}

bool SDStorage::_remove(const String& filename, void* testState = nullptr) {
#if defined(__SDSTORAGE_TEST)
  return _sd.remove(filename, testState);
#else
  return _sd.remove(filename);
#endif
}

bool SDStorage::_rename(const String& oldFilename, const String& newFilename, void* testState = nullptr) {
#if defined(__SDSTORAGE_TEST)
  return _sd.rename(oldFilename, newFilename, testState);
#else
  return _sd.rename(oldFilename, newFilename);
#endif
}

