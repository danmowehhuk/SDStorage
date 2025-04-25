#include "Transaction.h"

static uint16_t Transaction::_idSeq = 0;

static StreamableDTO* Transaction::_locks = new StreamableDTO();

using namespace SDStorageStrings;

Transaction::Transaction(const char* workDir) : StreamableDTO() {
  char idBuffer[12];
  snprintf_P(idBuffer, sizeof(idBuffer), (const char*)F("%u"), _idSeq++);
  size_t workDirLen = strlen(workDir);
  size_t idLen = strlen(idBuffer);
  _baseFilename = new char[workDirLen + idLen + 2](); // +1 for '/' +1 for '\0'
  char* p = _baseFilename;
  memcpy(p, workDir, workDirLen);
  p += workDirLen;
  *p++ = '/';
  memcpy(p, idBuffer, idLen);
  p += idLen;
  *p = '\0'; // null-terminate
}

void Transaction::add(const char* filename, const char* workDir) {
  bool didLog = false;
  while (_locks->exists(filename)) {
    // wait until the key is removed (lock released)
#if defined(DEBUG)
    if (!didLog) {
      Serial.print(F("Lock contention on "));
      Serial.println(filename);
      didLog = true;
    }
#endif
  }
  _locks->putEmpty(filename);
  char idBuffer[12];
  snprintf_P(idBuffer, sizeof(idBuffer), (const char*)F("%u"), _idSeq++);
  size_t bufferSize = strlen(workDir) + 1 + strlen(idBuffer) + 5;
  char tmpFilename[bufferSize];
  char* extRAM = strdup_P(_SDSTORAGE_TXN_TMP_EXTSN);
  snprintf_P(tmpFilename, bufferSize, (const char*)F("%s/%s%s"), workDir, idBuffer, extRAM);
  delete[] extRAM;
  put(filename, strdup(tmpFilename));
}

void Transaction::releaseLocks() {
  auto unlockFunction = [](const char* filename, const char* tmpFilename, bool keyPmem, bool valPmem, void* capture) -> bool {
    bool result = _locks->remove(filename);
    return result;
  };
  processEntries(unlockFunction, nullptr);
}

char* Transaction::getFilename() {
  const char* extPmem = _isCommitted ? _SDSTORAGE_TXN_COMMIT_EXTSN : _SDSTORAGE_TXN_TX_EXTSN;
  const char* extRAM = strdup_P(extPmem);
  size_t totalLen = strlen(_baseFilename) + strlen(extRAM) + 1;
  char* out = new char[totalLen]();
  snprintf_P(out, totalLen, (const char*)F("%s%s"), _baseFilename, extRAM);
  delete[] extRAM;
  return out;
}

void Transaction::setCommitted() {
  _isCommitted = true;
}

char* Transaction::getTmpFilename(const char* filename) {
  return get(filename);
};
