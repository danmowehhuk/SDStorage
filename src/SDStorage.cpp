#include <StreamableDTO.h>
#include "SDStorage.h"
#include "Transaction.h"

/*
 * Prepends the filename with the root directory if it isn't already.
 * Example:
 *    "foo.txt"             -->  "/<rootdir>/foo.txt"
 *    "/foo.txt"            -->  "/<rootdir>/foo.txt"
 *    "/<rootdir>/foo.txt"  -->   "/<rootdir>/foo.txt"
 */
String SDStorage::realFilename(const String& filename) {
  if (filename.startsWith(_rootDir + "/")) {
    return filename;
  }
  size_t bufferSize = _rootDir.length() + filename.length() + 2; // +2 for possible "/" and null terminator
  char resolvedName[bufferSize]; // Use a stack-based buffer for efficiency
  if (filename.startsWith("/")) {
    snprintf(resolvedName, bufferSize, "%s%s", _rootDir.c_str(), filename.c_str());
  } else {
    snprintf(resolvedName, bufferSize, "%s/%s", _rootDir.c_str(), filename.c_str());
  }
  return String(resolvedName);
}

/*
 * see SDStorage.h
 */
bool SDStorage::begin(void* testState = nullptr) {
  _rootDir.trim();
  bool sdInit = false;
  do {
   if (!_rootDir.startsWith(String(F("/")))) _rootDir = String(F("/")) + _rootDir;
   if (_rootDir.substring(1).indexOf('/') != -1) break; // no other slashes in root dir
   if (!_sd.begin(_sdCsPin)) break;
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

/*
 * see SDStorage.h
 */
String SDStorage::indexFilename(const String &idxName) {
  String idxFilename;
  String ext = String(reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_INDEX_EXTSN));
  idxFilename.reserve(_idxDir.length() + 1 + idxName.length() + ext.length());
  idxFilename = _idxDir + String(F("/")) + idxName + ext;
  return idxFilename;
}

/*
 * Returns true if the file exists (after prepending the root dir on the 
 * filename if necessary)
 */
bool SDStorage::exists(const String &filename, void* testState = nullptr) {
  return _exists(realFilename(filename), testState);
}

/*
 * Creates a new directory (after prepending the root dir on the 
 * dirName if necessary). Returns true if successful.
 */
bool SDStorage::mkdir(const String &dirName, void* testState = nullptr) {
  return _mkdir(realFilename(dirName), testState);
}

/*
 * Populates the DTO with data read from a file (after prepending the root dir
 * on the filename if necessary)
 */
bool SDStorage::load(const String &filename, StreamableDTO* dto, void* testState = nullptr) {
  String resolvedFilename = realFilename(filename);
  if (!_exists(resolvedFilename, testState)) {
    return false;
  }
  return _loadFromStream(resolvedFilename, dto, testState);
}

/*
 * Writes the DTO data with data to a file (after prepending the root dir on
 * the filename if necessary). If no transaction is provided, the write is
 * auto-committed.
 */
bool SDStorage::save(const String &filename, StreamableDTO* dto, Transaction* txn = nullptr) {
  return save(nullptr, filename, dto, txn);
}

bool SDStorage::save(void* testState, const String &filename, StreamableDTO* dto, Transaction* txn = nullptr) {
  String resolvedFilename = realFilename(filename);
  bool implicitTx = false;
  if (txn == nullptr) {
    // make an implicit, single-file transaction
    txn = beginTxn(testState, resolvedFilename);
    if (!txn) return false;
    implicitTx = true;
  }
  String tmpFilename = getTmpFilename(txn, resolvedFilename);  
  if (tmpFilename.length() == 0) return false;
  _writeToStream(tmpFilename, dto, testState);
  bool result = true;
  if (implicitTx) result = commitTxn(txn, testState);
  return result;
}

/*
 * Deletes a file (after prepending the root dir on the filename if necessary).
 * If no transaction is provided, the write is auto-committed.
 */
bool SDStorage::erase(const String &filename, Transaction* txn = nullptr) {
  return erase(nullptr, filename, txn);
}

bool SDStorage::erase(void* testState, const String &filename, Transaction* txn = nullptr) {
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

/*
 * Returns prefix matches from the specified index. If more than 10 matches are found,
 * the results will be in "trie mode" where only the next possible characters are returned.
 * In "trie mode" a value is only populated if the prefix+character exactly equals a key
 * in the index. Otherwise, the value is an empty string. For 10 or fewer matches, the 
 * entire key string is returned, and the value is always populated.
 */
void SDStorage::idxPrefixSearch(const String &idxName, SearchResults* results, void* testState = nullptr) {
  if (idxName.length() == 0) {
#if (defined(DEBUG))
    Serial.println(F("SDStorage::idxPrefixSearch - idxName cannot be empty"));
#endif
    return;
  }
  String idxFilename = indexFilename(idxName);
  if (!_exists(idxFilename, testState)) {
    // Index has no entries - return empty search results
  } else {
    _scanIndex(idxFilename, SDStorage::idxPrefixSearchFilter, results, testState);
    if (results->trieMode) {
      // clean up the partial matchResult
      if (results->matchResult) {
        KeyValue* toDelete = results->matchResult;
        results->matchResult = nullptr;
        delete toDelete;
      }
    } else { 
      // Not using trie mode, so clean up the partial trie result
      if (results->trieResult) {
        KeyValue* toDelete = results->trieResult;
        results->trieResult = nullptr;
        delete toDelete;
      }
    }
  }
}

/*
 * Scans the index looking for state->key and populating the state->keyExists and state->value
 * fields on the IdxScanCapture object passed in. Scanning stops when the key is found.
 */
void SDStorage::_idxScan(const String& idxName, IdxScanCapture* state, void* testState = nullptr) {
  if (idxName.length() == 0 || state->key.length() == 0) {
#if (defined(DEBUG))
    Serial.println(F("SDStorage::_idxScan - idxName and key cannot be empty"));
#endif
    return;
  }
  String idxFilename = indexFilename(idxName);
  if (!_exists(idxFilename, testState)) {
    // Index has no entries - leave state->keyExists false and state->value empty
  } else {
    _scanIndex(idxFilename, SDStorage::idxLookupFilter, state, testState);
  }
}

/*
 * Returns true if the index contains the provided key
 */
bool SDStorage::idxHasKey(const String &idxName, const String &key, void* testState = nullptr) {
  IdxScanCapture state(key);
  _idxScan(idxName, &state, testState);
  return state.keyExists;
}

/*
 * Returns the value associated with the key, or an empty string. Note that the empty
 * string is a legitimate value, so don't use this to determine if a key exists.
 */
String SDStorage::idxLookup(const String &idxName, const String &key, void* testState = nullptr) {
  IdxScanCapture state(key);
  _idxScan(idxName, &state, testState);
  return state.value;
}

/*
 * Inserts a key/value into the index, keeping the keys in their natural/alphabetical order
 * or updates the value if the key exists. If no transaction is provided, the insert/update
 * is auto-committed. Returns true if the insert/update was successful.
 */
bool SDStorage::idxUpsert(const String &idxName, const String &key, const String &value, Transaction* txn = nullptr) {
  return idxUpsert(nullptr, idxName, key, value, txn);
}

bool SDStorage::idxUpsert(void* testState, const String &idxName, const String &key, 
        const String &value, Transaction* txn = nullptr) {
  if (idxName.length() == 0 || key.length() == 0) {
#if (defined(DEBUG))
    Serial.println(F("SDStorage::idxUpsert - idxName and key cannot be empty"));
#endif
    return false;
  }
  String idxFilename = indexFilename(idxName);
  bool implicitTx = false;
  if (txn == nullptr) {
    // make an implicit, single-file transaction
    txn = beginTxn(testState, idxFilename);
    if (!txn) return false;
    implicitTx = true;
  }
  IdxScanCapture state(key, value);
  String tmpFilename = getTmpFilename(txn, idxFilename);
  if (tmpFilename.length() == 0) {
    // Problem with transaction that was passed in - leave state.didUpsert as false
    return false;
  } else if (!_exists(idxFilename, testState)) {
    // First write to the index
    _writeIndexLine(tmpFilename, toIndexLine(key, value), testState);
    state.didUpsert = true;
  } else {
    _updateIndex(idxFilename, tmpFilename, SDStorage::idxUpsertFilter, &state, testState);
    if (!state.didUpsert) {
      // new key goes at the end
      _writeIndexLine(tmpFilename, toIndexLine(key, state.valueIn), testState);
    }
  }
  return finalizeTxn(txn, implicitTx, true, testState);
}

/*
 * Removes a key from the index. If no transaction is provided, the removal
 * is auto-committed. Returns true if the key existed and was successfully removed.
 */
bool SDStorage::idxRemove(const String &idxName, const String &key, Transaction* txn = nullptr) {
  return idxRemove(nullptr, idxName, key, txn);
}

bool SDStorage::idxRemove(void* testState, const String &idxName, const String &key, Transaction* txn = nullptr) {
  if (idxName.length() == 0 || key.length() == 0) {
#if (defined(DEBUG))
    Serial.println(F("SDStorage::idxRemove - idxName and key cannot be empty"));
#endif
    return false;
  }
  String idxFilename = indexFilename(idxName);
  bool implicitTx = false;
  if (txn == nullptr) {
    // make an implicit, single-file transaction
    txn = beginTxn(testState, idxFilename);
    if (!txn) return false;
    implicitTx = true;
  }
  IdxScanCapture state(key);
  String tmpFilename = getTmpFilename(txn, idxFilename);
  if (tmpFilename.length() == 0) {
    // Problem with transaction that was passed in - leave state.didUpsert as false
    return false;
  } else if (!_exists(idxFilename, testState)) {
    // Empty index - leave state.didRemove as false
  } else {
    _updateIndex(idxFilename, tmpFilename, SDStorage::idxRemoveFilter, &state, testState);
  }
  return finalizeTxn(txn, implicitTx, state.didRemove, testState);
}

/*
 * Renames a key in the index, updating its position to preserve natural/alphabetical
 * key ordering. The new key name cannot already exist in the index. If no transaction
 * is provided, the change is auto-committed. Returns true if the rename was successful.
 */
bool SDStorage::idxRename(const String &idxName, const String &oldKey, const String &newKey, Transaction* txn = nullptr) {
  return idxRename(nullptr, idxName, oldKey, newKey, txn);
}

bool SDStorage::idxRename(void* testState, const String &idxName, 
        const String &oldKey, const String &newKey, Transaction* txn = nullptr) {
  if (idxName.length() == 0 || oldKey.length() == 0 || newKey.length() == 0) {
#if (defined(DEBUG))
    Serial.println(F("SDStorage::idxRename - idxName, oldKey and newKey cannot be empty"));
#endif
    return false;
  }
  String idxFilename = indexFilename(idxName);
  bool implicitTx = false;
  if (txn == nullptr) {
    // make an implicit, single-file transaction
    txn = beginTxn(testState, idxFilename);
    if (!txn) return false;
    implicitTx = true;
  }
  IdxScanCapture state(oldKey, newKey, true);
  String tmpFilename = getTmpFilename(txn, idxFilename);
  if (tmpFilename.length() == 0) {
    // Problem with transaction that was passed in - leave state.didUpsert as false
    return false;
  }
  IdxScanCapture lookupState(oldKey);
  _idxScan(idxName, &lookupState, testState);
  if (!lookupState.keyExists) {
    // Leave state->didInsert and state->didRemove as false
#if (defined(DEBUG))
    Serial.println(String(F("Key to rename does not exist in ")) + idxName);
#endif
  } else {
    state.value = lookupState.value;
    _updateIndex(idxFilename, tmpFilename, SDStorage::idxRenameFilter, &state, testState);
  }
  return finalizeTxn(txn, implicitTx, (state.didRemove && state.didInsert), testState);
}

bool SDStorage::finalizeTxn(Transaction* txn, bool autoCommit, bool success, void* testState) {
  if (autoCommit) {
    if (success) {
      return commitTxn(txn, testState);
    } else {
      abortTxn(txn, testState);
      return false;
    }
  } else if (success) {
    return true;
  } else {
    return false;
  }
}

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
    /*
     * Something failed that should not have failed. This means the files might
     * be in an inconsistent state. Call the supplied errFunction.
     */
#if defined(DEBUG)
    Serial.println(String(F("ERROR: SDStorage::commitTxn")));
#endif
    if (_errFunction != nullptr) _errFunction();
    return false;
  }
  return true;
}

bool SDStorage::abortTxn(Transaction* txn, void* testState = nullptr) {
  TransactionCapture capture(this, testState);
  auto cleanupFunction = [](const char* filename, const char* tmpFilename, bool keyPmem, bool valPmem, void* capture) -> bool {
    TransactionCapture* c = static_cast<TransactionCapture*>(capture);
    if (strcmp_P(tmpFilename, _SDSTORAGE_TOMBSTONE) == 0) {
      // Tombstone - nothing to clean up
    } else if (c->sdStorage->_exists(String(tmpFilename), c->ts)) {
      if (!c->sdStorage->_remove(String(tmpFilename), c->ts)) {
#if defined(DEBUG)
        Serial.println(String(tmpFilename) + String(F(" could not be removed")));
#endif
        c->success = false;
        return false;
      }
    }
    return true;
  };
  txn->processEntries(cleanupFunction, &capture);
  if (!capture.success) {
    /*
     * At least one tmp file was not cleaned up. This should not cause
     * problems. They should be cleaned up by fsck() on the next startup.
     */
#if defined(DEBUG)
    Serial.println(String(F("SDStorage abortTxn failed")));
#endif
  }
  _cleanupTxn(txn, testState); // releases locks
  return capture.success;
}

bool SDStorage::_applyChanges(Transaction* txn, void* testState = nullptr) {
  TransactionCapture capture(this, testState);
  auto applyChangesFunction = [](const char* filename, const char* tmpFilename, bool keyPmem, bool valuePmem, void* capture) -> bool {
    TransactionCapture* c = static_cast<TransactionCapture*>(capture);
    if (strcmp_P(tmpFilename, _SDSTORAGE_TOMBSTONE) == 0) {
      // tombstone - delete the file
      c->sdStorage->_remove(String(filename), c->ts);
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
      if (!c->sdStorage->_rename(String(tmpFilename), String(filename), c->ts)) {
#if defined(DEBUG)
        Serial.println(String(F("Could not move ")) + String(tmpFilename)
              + String(F(" to ")) + String(filename));
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
#if (!defined(__SDSTORAGE_TEST))
  File workDirFile = _sd.open(_workDir);
  if (!workDirFile) return false;
  if (!workDirFile.isDirectory()) return false;
  String commitExtension = reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_TXN_COMMIT_EXTSN);
  while (true) {
    File file = workDirFile.openNextFile();
    if (!file) break; // no more files
    char buffer[64];
    file.getName(buffer, sizeof(buffer));
    String filename = String(buffer);
    if (filename.endsWith(commitExtension)) {
      // Leftover commit file needs to be applied
      Transaction* txn = new Transaction(_workDir);
      _streams.load(&file, txn);
      file.close();
      bool commitErr = true;
      do {
        if (!_applyChanges(txn)) break;
        _cleanupTxn(txn);
        commitErr = false;
      } while (false);
      if (commitErr) {
        /*
         * Something failed that should not have failed. This means the files might
         * be in an inconsistent state. Call the supplied errFunction.
         */
#if defined(DEBUG)
        Serial.println(String(F("ERROR: SDStorage::fsck() - Failed to apply commit ")) + filename);
#endif
        if (_errFunction != nullptr) _errFunction();
        return false;
      }
    }
  }

  // All commits successfully applied, so delete all remaining files.
  // This effectively aborts any incomplete transactions.
  workDirFile.close();
  workDirFile = _sd.open(_workDir);
  while (true) {
    File file = workDirFile.openNextFile();
    if (!file) break; // no more files
    char buffer[64];
    file.getName(buffer, sizeof(buffer));
    String filename = String(buffer);
    if (!_sd.remove(filename)) {
      /*
       * Cleanup failed, but any outstanding commits were applied, so nothing
       * is in an inconsistent state, although the workdir needs to be cleaned up
       * to prevent tmp file name collisions.
       */
#if defined(DEBUG)
      Serial.println(String(F("ERROR: SDStorage::fsck() - Failed to clean up work dir ")) + _workDir);
#endif
      if (_errFunction != nullptr) _errFunction();
      return false;
    }
  }
#endif
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

bool SDStorage::isValidFAT16Filename(const String& filename) {
  if (filename.length() == 0) return false;

  // Check for more than one dot or invalid length
  int dotIndex = filename.indexOf('.');
  if (dotIndex == -1) {
      // No extension
      if (filename.length() > 8) return false;
  } else {
      // With extension
      String namePart = filename.substring(0, dotIndex);
      String extPart = filename.substring(dotIndex + 1);

      if (namePart.length() == 0 || namePart.length() > 8) return false;
      if (extPart.length() == 0 || extPart.length() > 3) return false;
  }

  // Check each character
  for (int i = 0; i < filename.length(); i++) {
      char c = filename[i];
      if (!((c >= 'A' && c <= 'Z') ||  // Uppercase letters
            (c >= 'a' && c <= 'z') ||  // Lowercase is ok too
            (c >= '0' && c <= '9') ||  // Numbers
            (c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
             c == '\'' || c == '(' || c == ')' || c == '-' || c == '@' ||
             c == '^' || c == '_' || c == '`' || c == '{' || c == '}' ||
             c == '~' || (c == '.' && i != 0 && i != filename.length() - 1)))) {
          return false;
      }
  }
  return true;
}

String SDStorage::getPathFromFilename(const String& filename) {
  int lastSlashIndex = filename.lastIndexOf('/');
  return filename.substring(0, lastSlashIndex);
}

String SDStorage::getFilenameFromFullName(const String& filename) {
  int lastSlashIndex = filename.lastIndexOf('/');
  return filename.substring(lastSlashIndex + 1);
}

String SDStorage::toIndexLine(const String& key, const String& value) {
  return key + String(F("=")) + value;
}

String SDStorage::parseIndexKey(const String& line) {
  int separatorIndex = line.indexOf('=');
  String key = line.substring(0, separatorIndex);
  key.trim();
  return key;
}

String SDStorage::parseIndexValue(const String& line) {
  int separatorIndex = line.indexOf('=');
  String value = line.substring(separatorIndex + 1);
  value.trim();
  return value;
}

bool SDStorage::_pipeFast(const char* line, StreamableManager::DestinationStream* dest) {
  dest->println(line);
  return true;
}

bool SDStorage::idxLookupFilter(const char* line, StreamableManager::DestinationStream* dest, 
      void* statePtr) {
  IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
  if (strcmp(state->key.c_str(), parseIndexKey(String(line)).c_str()) == 0) {
    state->keyExists = true;
    state->value = parseIndexValue(line);
    return false; // stop scanning
  }
  return true; // keep going
}

bool SDStorage::idxUpsertFilter(const char* line, StreamableManager::DestinationStream* dest, 
      void* statePtr) {
  IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
  if (state->didUpsert) {
    // Upsert already happened. Pipe the rest in fast mode.
    return _pipeFast(line, dest);
  }
  String key = parseIndexKey(String(line));
  if (strcmp(state->key.c_str(), key.c_str()) == 0) {
    // Found matching key - update the value
    dest->println(toIndexLine(key, state->valueIn).c_str());
    state->didUpsert = true;
  } else if (strcmp(state->key.c_str(), key.c_str()) < 0 &&               // state->key is before key
          (state->prevKey.length() == 0                                   // this is the first key OR
           || strcmp(state->key.c_str(), state->prevKey.c_str()) > 0)) {  // state->key is after prevKey
      // insert new key/value before line
      dest->println(toIndexLine(state->key, state->valueIn).c_str());
      dest->println(line);
      state->didUpsert = true;
  } else {
    // state-key comes after this key, so keep going
    dest->println(line);
    state->prevKey = key;
  }
  return true;
}

bool SDStorage::idxRemoveFilter(const char* line, StreamableManager::DestinationStream* dest, 
      void* statePtr) {
  IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
  if (state->didRemove) {
    // Remove already happened. Pipe the rest in fast mode.
    return _pipeFast(line, dest);
  }
  if (strcmp(state->key.c_str(), parseIndexKey(String(line)).c_str()) == 0) {
    /* skip it */ 
    state->didRemove = true;
  } else { dest->println(line); }
  return true;
}

bool SDStorage::idxRenameFilter(const char* line, StreamableManager::DestinationStream* dest, 
      void* statePtr) {
  IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
  if (state->didRemove && state->didInsert) {
    // Rename already happened. Pipe the rest in fast mode.
    return _pipeFast(line, dest);
  }
  String key = parseIndexKey(String(line));
  if (strcmp(state->key.c_str(), key.c_str()) == 0) {
    /* skip the old key */
    state->didRemove = true;
  } else if (strcmp(state->newKey.c_str(), key.c_str()) == 0) {
    // new key already exists - abort
    return false;
  } else if (strcmp(state->newKey.c_str(), key.c_str()) < 0 &&               // state->newKey is before key
          (state->prevKey.length() == 0                                      // this is the first key OR
           || strcmp(state->newKey.c_str(), state->prevKey.c_str()) > 0)) {  // state->newKey is after prevKey
      // insert new newKey/value before line
      dest->println(toIndexLine(state->newKey, state->value).c_str());
      dest->println(line);
      state->didInsert = true;
  } else {
    // state-key comes after this key, so keep going
    dest->println(line);
    state->prevKey = key;
  }
  return true;
}

bool SDStorage::idxPrefixSearchFilter(const char* line, StreamableManager::DestinationStream* dest, 
      void* statePtr) {
  SearchResults* results = static_cast<SearchResults*>(statePtr);
  String key = parseIndexKey(String(line));
  String prefix = results->searchPrefix;

  if (prefix.length() > 0 && !key.startsWith(prefix) && strcmp(key.c_str(), prefix.c_str()) > 0) {
    // Indexes are sorted, so if key comes after prefix, there's no
    // point continuing to scan the index
    return false;
  }
  if (prefix.length() == 0 || key.startsWith(prefix)) {
    String value = parseIndexValue(String(line));
    // Handle up to 10 matches, then switch to trie mode
    if (results->matchCount < 10) {
      KeyValue* match = new KeyValue(key, value);
      appendMatchResult(results, match);
      results->matchCount++;
    } else {
      results->trieMode = true;
    }

    // Populate the trieResult and bloom filter
    uint8_t pos = prefix.length();
    if (key.length() > prefix.length()) {
      char c = key[pos];
      if (c >= 32 && c <= 122) { // Ensure it's an acceptable ASCII character
        uint8_t index = c - 32;  // Map ASCII to 0-90
        uint8_t wordIndex = index / 32; // Determine which 32-bit word to use
        uint8_t bitIndex = index % 32; // Determine the bit within the word
        if ((results->trieBloom[wordIndex] & (1UL << bitIndex)) == 0) { // Check if this char is new
          results->trieBloom[wordIndex] |= (1UL << bitIndex);  // Mark this char as seen
          // Add it to the trieResult
          KeyValue* kv;
          if (key.equals(prefix + c)) {
            kv = new KeyValue(String(c), value);
          } else {
            kv = new KeyValue(String(c), "");
          }
          appendTrieResult(results, kv);
        }
      }
    }
  }
  return true;
}

void SDStorage::_appendKeyValue(KeyValue* head, KeyValue* result) {
  while (head->next != nullptr) head = head->next;
  head->next = result;
}

void SDStorage::appendMatchResult(SearchResults* sr, KeyValue* result) {
  if (sr->matchResult == nullptr) {
    sr->matchResult = result;
  } else {
    _appendKeyValue(sr->matchResult, result);
  }
}

void SDStorage::appendTrieResult(SearchResults* sr, KeyValue* result) {
  if (sr->trieResult == nullptr) {
    sr->trieResult = result;
  } else {
    _appendKeyValue(sr->trieResult, result);
  }
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

bool SDStorage::_isDir(const String& filename, void* testState = nullptr) {
  bool isDir = false;
#if defined(__SDSTORAGE_TEST)
  isDir = _sd.isDirectory(filename, testState);
#else
  File file = _sd.open(filename);
  isDir = file.isDirectory();
  file.close();
#endif
  return isDir;
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
  StringStream* ss = static_cast<StringStream*>(src);
  delete ss;
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

void SDStorage::_writeIndexLine(const String& indexFilename, const String& line, void* testState = nullptr) {
  Stream* dest;
#if defined(__SDSTORAGE_TEST)
  dest = _sd.writeIndexFileStream(indexFilename, testState);
#else
  File file = _sd.open(indexFilename, FILE_WRITE);
  dest = &file;
#endif
  for (size_t i = 0; i < line.length(); i++) {
    dest->write(line[i]);
  }
  dest->write('\n');
#if (!defined(__SDSTORAGE_TEST))
  file.close();
#endif
}

void SDStorage::_updateIndex(
      const String& indexFilename, const String& tmpFilename, 
      StreamableManager::FilterFunction filter, void* statePtr, void* testState = nullptr) {
  Stream* src;
  Stream* dest;
#if defined(__SDSTORAGE_TEST)
  src = _sd.readIndexFileStream(indexFilename, testState);
  dest = _sd.writeIndexFileStream(tmpFilename, testState);
#else
  File srcFile = _sd.open(indexFilename, FILE_READ);
  File destFile = _sd.open(tmpFilename, FILE_WRITE);
  src = &srcFile;
  dest = &destFile;
#endif
  _streams.pipe(src, dest, filter, statePtr);
#if defined(__SDSTORAGE_TEST)
  StringStream* sss = static_cast<StringStream*>(src);
  delete sss;
#else
  srcFile.close();
  destFile.close();
#endif
}

void SDStorage::_scanIndex(const String &indexFilename, StreamableManager::FilterFunction filter, void* statePtr, 
      void* testState = nullptr) {
  Stream* src;
#if defined(__SDSTORAGE_TEST)
  src = _sd.readIndexFileStream(indexFilename, testState);
#else
  File srcFile = _sd.open(indexFilename, FILE_READ);
  src = &srcFile;
#endif
  _streams.pipe(src, nullptr, filter, statePtr);
#if defined(__SDSTORAGE_TEST)
  StringStream* ss = static_cast<StringStream*>(src);
  delete ss;
#else
  srcFile.close();
#endif
}

