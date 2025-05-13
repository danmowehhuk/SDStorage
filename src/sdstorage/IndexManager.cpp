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
  char idxFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->indexFilename(idx, idxFilename, FileHelper::MAX_FILENAME_LENGTH)) return false;
  bool implicitTx = false;
  if (txn == nullptr) {
    // make an implicit, single-file transaction
    txn = _txnManager->beginTxn(testState, idxFilename);
    if (!txn) {
      return false;
    }
    implicitTx = true;
  }
  IndexScanFilters::IdxScanCapture state(entry->key, entry->value);
  size_t bufSize = _storageProvider->getBufferSize();
  char newLine[bufSize];
  if (!IndexHelpers::toIndexLine(entry, newLine, bufSize)) {
    // Problem with IndexEntry conversion - leave state.didUpsert as false
    return false;
  }
  bool success = false;
  char* tmpFilename = _txnManager->getTmpFilename(txn, idxFilename);
  if (isEmpty(tmpFilename)) {
    // Problem with transaction that was passed in - leave state.didUpsert as false
  } else if (!_storageProvider->_exists(idxFilename, testState)) {
    // First write to the index
    success = _storageProvider->_writeIndexLine(tmpFilename, newLine, testState);
    state.didUpsert = true;
  } else {
    success = _storageProvider->_updateIndex(idxFilename, tmpFilename, IndexScanFilters::idxUpsertFilter, &state, testState);
    if (success && !state.didUpsert) {
      // new key goes at the end
      success = _storageProvider->_writeIndexLine(tmpFilename, newLine, testState);
      state.didUpsert = true;
    }
  }
  success &= state.didUpsert;
  return _txnManager->finalizeTxn(txn, implicitTx, success, testState);
}

bool IndexManager::idxRemove(Index idx, const char* key, Transaction* txn = nullptr) {
  return idxRemove(nullptr, idx, key, txn);
}

bool IndexManager::idxRemove(void* testState, Index idx, const char* key, Transaction* txn = nullptr) {
    if (!idx.name || !key || isEmpty(key)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxRemove - index name and key cannot be empty"));
#endif
    return false;
  }
  char idxFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->indexFilename(idx, idxFilename, FileHelper::MAX_FILENAME_LENGTH)) return false;
  bool implicitTx = false;
  if (txn == nullptr) {
    // make an implicit, single-file transaction
    txn = _txnManager->beginTxn(testState, idxFilename);
    if (!txn) {
      return false;
    }
    implicitTx = true;
  }
  IndexScanFilters::IdxScanCapture state(key);
  bool success = false;
  char* tmpFilename = _txnManager->getTmpFilename(txn, idxFilename);
  if (isEmpty(tmpFilename)) {
    // Problem with transaction that was passed in - leave state.didRemove as false
  } else if (!_storageProvider->_exists(idxFilename, testState)) {
    // Empty index - leave state.didRemove as false
  } else {
    success = _storageProvider->_updateIndex(idxFilename, tmpFilename, IndexScanFilters::idxRemoveFilter, &state, testState);
  }
  success &= state.didRemove;
  return _txnManager->finalizeTxn(txn, implicitTx, success, testState);
}

bool IndexManager::idxRename(Index idx, const char* oldKey, const char* newKey, Transaction* txn = nullptr) {
  return idxRename(nullptr, idx, oldKey, newKey, txn);
}

bool IndexManager::idxRename(void* testState, Index idx, const char* oldKey, const char* newKey, Transaction* txn = nullptr) {

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
    Transaction* t = _txnManager->beginTxn(testState, idxFilename);
    if (t) idxTxn.txn = t;
  }
  idxTxn.tmpFilename = _txnManager->getTmpFilename(txn, idxFilename);
  return idxTxn;
}

