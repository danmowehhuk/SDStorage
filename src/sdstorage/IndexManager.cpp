#include "IndexManager.h"

using namespace SDStorageStrings;

bool IndexManager::idxUpsert(Index idx, IndexEntry* entry, Transaction* txn = nullptr) {
  return idxUpsert(nullptr, idx, entry, txn);
}

bool IndexManager::idxUpsert(void* testState, Index idx, IndexEntry* entry, Transaction* txn = nullptr) {
  if (!idx.name || isEmpty(entry->key)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxUpsert - index name and entry key cannot be empty"));
#endif
    return false;
  }
  IndexTransaction iTxn = _makeIndexTransaction(testState, idx, txn);
  if (!iTxn.idxFilename || !iTxn.txn) return false;

  IndexScanFilters::IdxScanCapture state(entry->key, entry->value);
  size_t bufSize = _storageProvider->getBufferSize();
  char newLine[bufSize];
  if (!IndexHelpers::toIndexLine(entry, newLine, bufSize)) {
    // Problem with IndexEntry conversion - leave state.didUpsert as false
    return false;
  }
  bool success = false;
  if (isEmpty(iTxn.tmpFilename)) {
    // Problem with transaction that was passed in - leave state.didUpsert as false
  } else if (!_storageProvider->_exists(iTxn.idxFilename, testState)) {
    // First write to the index
    success = _storageProvider->_writeIndexLine(iTxn.tmpFilename, newLine, testState);
    state.didUpsert = true;
  } else {
    success = _storageProvider->_updateIndex(iTxn.idxFilename, iTxn.tmpFilename, IndexScanFilters::idxUpsertFilter, &state, testState);
    if (success && !state.didUpsert) {
      // new key goes at the end
      success = _storageProvider->_writeIndexLine(iTxn.tmpFilename, newLine, testState);
      state.didUpsert = true;
    }
  }
  iTxn.success = (success & state.didUpsert);
  return _txnManager->finalizeTxn(iTxn.txn, iTxn.isImplicitTxn, iTxn.success, testState);
}

bool IndexManager::idxRemove(Index idx, const char* key, Transaction* txn = nullptr) {
  return idxRemove(nullptr, idx, key, txn);
}

bool IndexManager::idxRemove(void* testState, Index idx, const char* key, Transaction* txn = nullptr) {
    if (!idx.name || isEmpty(key)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxRemove - index name and key cannot be empty"));
#endif
    return false;
  }
  IndexTransaction iTxn = _makeIndexTransaction(testState, idx, txn);
  if (!iTxn.idxFilename || !iTxn.txn) return false;

  IndexScanFilters::IdxScanCapture state(key);
  bool success = false;
  if (!isEmpty(iTxn.tmpFilename) && _storageProvider->_exists(iTxn.idxFilename, testState)) {
    success = _storageProvider->_updateIndex(iTxn.idxFilename, iTxn.tmpFilename, IndexScanFilters::idxRemoveFilter, &state, testState);
  }
  iTxn.success = (success & state.didRemove);
  return _txnManager->finalizeTxn(iTxn.txn, iTxn.isImplicitTxn, iTxn.success, testState);
}

bool IndexManager::idxRename(Index idx, const char* oldKey, const char* newKey, Transaction* txn = nullptr) {
  return idxRename(nullptr, idx, oldKey, newKey, txn);
}

bool IndexManager::idxRename(void* testState, Index idx, const char* oldKey, const char* newKey, Transaction* txn = nullptr) {
  if (!idx.name || isEmpty(oldKey) || isEmpty(newKey)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxRename - index name, oldKey and newKey cannot be empty"));
#endif
    return false;
  }

  IndexTransaction iTxn = _makeIndexTransaction(testState, idx, txn);
  if (!iTxn.idxFilename || !iTxn.txn) return false;

  boolean success = false;
  IndexScanFilters::IdxScanCapture lookupState(oldKey);
  IndexScanFilters::IdxScanCapture state(oldKey, newKey, true);
  success = _idxScan(iTxn.idxFilename, &lookupState, testState);

  if (!success) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxRename - index scan failed"));
#endif    
  } else {
    if (!lookupState.keyExists) {
#if (defined(DEBUG))
      Serial.print(F("Key to rename '"));
      Serial.print(oldKey);
      Serial.print(F("' not in "));
      Serial.println(iTxn.idxFilename);
#endif
    }

    if (!isEmpty(iTxn.tmpFilename) && lookupState.keyExists) {
      if (state.value) free(state.value);
      state.value = nullptr;
      state.value = strdup(lookupState.value);
      success = _storageProvider->_updateIndex(iTxn.idxFilename, iTxn.tmpFilename, IndexScanFilters::idxRenameFilter, &state, testState);
    }
  }
  iTxn.success = (success && state.didRemove && state.didInsert);
  return _txnManager->finalizeTxn(iTxn.txn, iTxn.isImplicitTxn, iTxn.success, testState);
}

bool IndexManager::idxLookup(Index idx, const char* key, char* buffer, size_t bufferSize, void* testState = nullptr) {
  if (!idx.name || isEmpty(key) || !buffer || bufferSize <= 0) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxLookup - index name, key and buffer required"));
#endif
    return false;
  }
  char idxFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->indexFilename(idx, idxFilename, FileHelper::MAX_FILENAME_LENGTH)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxLookup - indexFilename failure"));
#endif
    return false;
  }
  IndexScanFilters::IdxScanCapture state(key);
  bool success = false;
  success = _idxScan(idxFilename, &state, testState);
  if (success) { // the scan worked, but was the key found?
    if (state.keyExists) {
      static const char fmt[] PROGMEM = "%s";
      int n = snprintf_P(buffer, bufferSize, fmt, state.value);
      if (n < 0 || static_cast<size_t>(n) >= bufferSize) {
        // Truncated or error
        buffer[bufferSize - 1] = '\0';
        success = false;
      }
    } else {
      // key not found
      success = false;
    }
  }
  return success;
}

bool IndexManager::idxHasKey(Index idx, const char* key, void* testState = nullptr) {
  if (!idx.name || isEmpty(key)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxLookup - index name and key"));
#endif
    return false;
  }
  char idxFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->indexFilename(idx, idxFilename, FileHelper::MAX_FILENAME_LENGTH)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxLookup - indexFilename failure"));
#endif
    return false;
  }
  IndexScanFilters::IdxScanCapture state(key);
  bool success = false;
  success = _idxScan(idxFilename, &state, testState);
  return (success && state.keyExists);
}

bool IndexManager::idxPrefixSearch(Index idx, SearchResults* results, void* testState = nullptr) {
  if (!idx.name) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxPrefixSearch - index is required"));
#endif
    return false;
  }
  char idxFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->indexFilename(idx, idxFilename, FileHelper::MAX_FILENAME_LENGTH)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxLookup - indexFilename failure"));
#endif
    return false;
  }
  if (!_storageProvider->_exists(idxFilename, testState)) {
    // Index has no entries - return empty search results
    return true;
  }
  bool success = _storageProvider->_scanIndex(idxFilename, IndexScanFilters::idxPrefixSearchFilter, results, testState);
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
  return success;
};

/*
 * Scans the index looking for state->key and populating the state->keyExists and state->value
 * fields on the IdxScanCapture object passed in. Scanning stops when the key is found.
 */
bool IndexManager::_idxScan(const char* idxFilename, IndexScanFilters::IdxScanCapture* state, void* testState = nullptr) {
  if (!idxFilename || !state || isEmpty(state->key)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::_idxScan - index name and key cannot be empty"));
#endif
    return false;
  }
  boolean success = false;
  if (_storageProvider->_exists(idxFilename, testState)) {
    success = _storageProvider->_scanIndex(idxFilename, IndexScanFilters::idxLookupFilter, state, testState);
  }
  return success;
}

IndexManager::IndexTransaction IndexManager::_makeIndexTransaction(void* testState, Index idx, Transaction* txn) {
  IndexManager::IndexTransaction idxTxn;
  idxTxn.txn = txn;
  idxTxn.idx = &idx;
  char idxFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (_fileHelper->indexFilename(idx, idxFilename, FileHelper::MAX_FILENAME_LENGTH)) {
    idxTxn.idxFilename = strdup(idxFilename);
  }
  if (txn == nullptr) {
    idxTxn.isImplicitTxn = true;
    idxTxn.txn = _txnManager->beginTxn(testState, idxTxn.idxFilename);
  } else {
    idxTxn.txn = txn;
  }
  idxTxn.tmpFilename = _txnManager->getTmpFilename(idxTxn.txn, idxTxn.idxFilename);
  return idxTxn;
}

