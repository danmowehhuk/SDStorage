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
  char* tmpFilename = _txnManager->getTmpFilename(txn, idxFilename);  
  if (isEmpty(tmpFilename)) {
    // Problem with transaction that was passed in - leave state.didUpsert as false
  } else if (!_storageProvider->_exists(idxFilename, testState)) {
    // First write to the index
    _storageProvider->_writeIndexLine(tmpFilename, newLine, testState);
    state.didUpsert = true;
  } else {
    _storageProvider->_updateIndex(idxFilename, tmpFilename, IndexScanFilters::idxUpsertFilter, &state, testState);
    if (!state.didUpsert) {
      // new key goes at the end
      _storageProvider->_writeIndexLine(tmpFilename, newLine, testState);
      state.didUpsert = true;
    }
  }
  return _txnManager->finalizeTxn(txn, implicitTx, state.didUpsert, testState);
}

