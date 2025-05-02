#include "IndexManager.h"

using namespace SDStorageStrings;

char* IndexManager::indexFilename(const char* idxName, bool isIdxNamePmem = false) {
  char* extRAM = strdup_P(_SDSTORAGE_INDEX_EXTSN);
  char* idxNameRAM = isIdxNamePmem ? strdup_P(idxName) : idxName;
  char* idxDir = _fileHelper->getIdxDir();
  size_t totalLen = strlen(idxDir) + 1 + strlen(idxNameRAM) + strlen(extRAM) + 1;
  char* idxFilename = new char[totalLen]();
  static const char fmt[] PROGMEM = "%s/%s%s";
  snprintf_P(idxFilename, totalLen, fmt, idxDir, idxNameRAM, extRAM);
  if (isIdxNamePmem) delete[] idxNameRAM;
  Serial.println(idxFilename);
  delete[] extRAM;
  return idxFilename;
}

char* IndexManager::indexFilename(const __FlashStringHelper* idxName) {
  return indexFilename(reinterpret_cast<const char*>(idxName), true);
}

char* IndexManager::indexFilename_P(const char* idxName) {
  return indexFilename(idxName, true);
}

bool IndexManager::idxUpsert(void* testState, const char* idxName, const char* key, const char* value, 
        Transaction* txn = nullptr) {
  if (isEmpty(idxName) || isEmpty(key)) {
#if (defined(DEBUG))
    Serial.println(F("IndexManager::idxUpsert - idxName and key cannot be empty"));
#endif
    return false;
  }
  char* idxFilename = indexFilename(idxName);
  bool implicitTx = false;
  if (txn == nullptr) {
    // make an implicit, single-file transaction
    txn = _txnManager->beginTxn(testState, idxFilename);
    if (!txn) {
      if (idxFilename) delete[] idxFilename;
      return false;
    }
    implicitTx = true;
  }
  IndexScanFilters::IdxScanCapture state(key, value);
  char* tmpFilename = _txnManager->getTmpFilename(txn, idxFilename);  
  if (isEmpty(tmpFilename)) {
    // Problem with transaction that was passed in - leave state.didUpsert as false
    return false;
  } else if (!_storageProvider->_exists(idxFilename, testState)) {
    // First write to the index
    char* line = IndexScanFilters::toIndexLine(key, value);
    _storageProvider->_writeIndexLine(tmpFilename, line, testState);
    delete[] line;
    state.didUpsert = true;
  } else {
    _storageProvider->_updateIndex(idxFilename, tmpFilename, IndexScanFilters::idxUpsertFilter, &state, testState);
    if (!state.didUpsert) {
      // new key goes at the end
      char* line = IndexScanFilters::toIndexLine(key, value);
      _storageProvider->_writeIndexLine(tmpFilename, line, testState);
      delete[] line;
    }
  }
  return _txnManager->finalizeTxn(txn, implicitTx, true, testState);
}

bool IndexManager::idxUpsert(const char* idxName, const char* key, const char* value, 
        Transaction* txn = nullptr) {
  return idxUpsert(nullptr, idxName, key, value, txn);
}

bool IndexManager::idxUpsert(const __FlashStringHelper* idxName, const char* key, const char* value, 
        Transaction* txn = nullptr) {
  return idxUpsert(nullptr, idxName, key, value, txn);
}

bool IndexManager::idxUpsert_P(const char* idxName, const char* key, const char* value, 
        Transaction* txn = nullptr) {
  return idxUpsert(nullptr, idxName, key, value, txn);
}

bool IndexManager::idxUpsert(void* testState, const __FlashStringHelper* idxName, const char* key, 
        const char* value, Transaction* txn = nullptr) {
  char* idxNameRAM = strdup(idxName);
  bool result = idxUpsert(testState, idxNameRAM, key, value, txn);
  delete[] idxNameRAM;
  return result;
}

bool IndexManager::idxUpsert_P(void* testState, const char* idxName, const char* key, 
        const char* value, Transaction* txn = nullptr) {
  char* idxNameRAM = strdup_P(idxName);
  bool result = idxUpsert(testState, idxNameRAM, key, value, txn);
  delete[] idxNameRAM;
  return result;
}


