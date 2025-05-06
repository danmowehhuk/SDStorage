#include "Transaction.h"

static uint16_t Transaction::_idSeq = 0;

static StreamableDTO* Transaction::_locks = new StreamableDTO();

using namespace SDStorageStrings;

Transaction::Transaction(FileHelper* fileHelper):
      StreamableDTO(), _fileHelper(fileHelper) {
  char idBuffer[12];
  static const char fmt[] PROGMEM = "%u";
  snprintf_P(idBuffer, sizeof(idBuffer), fmt, _idSeq++);

  char* workDir = fileHelper->getWorkDir();
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
  static const char fmt[] PROGMEM = "%u";
  snprintf_P(idBuffer, sizeof(idBuffer), fmt, _idSeq++);
  size_t bufferSize = strlen(workDir) + 1 + strlen(idBuffer) + 5;
  char tmpFilename[bufferSize];
  char* extRAM = strdup_P(_SDSTORAGE_TXN_TMP_EXTSN);
  static const char fmt1[] PROGMEM = "%s/%s%s";
  snprintf_P(tmpFilename, bufferSize, fmt1, workDir, idBuffer, extRAM);
  free(extRAM);
  put(filename, tmpFilename);
}

void Transaction::releaseLocks() {
  auto unlockFunction = [](const char* filename, const char* tmpFilename, bool keyPmem, bool valPmem, void* capture) -> bool {
    bool result = _locks->remove(filename, keyPmem);
    return true;
  };
  processEntries(unlockFunction, nullptr);
}

bool Transaction::getFilename(char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0 || !FileHelper::verifyBufferSize(bufferSize)) return false;

  const char* extPmem = _isCommitted ? _SDSTORAGE_TXN_COMMIT_EXTSN : _SDSTORAGE_TXN_TX_EXTSN;
  const char* extRAM = strdup_P(extPmem);
  static const char fmt[] PROGMEM = "%s%s";
  int n = snprintf_P(buffer, bufferSize, fmt, _baseFilename, extRAM);
  bool success = true;
  if (n < 0 || static_cast<size_t>(n) >= bufferSize) {
    // Truncated or error
    buffer[bufferSize - 1] = '\0';
    success = false;
  }
  free(extRAM);
  return success;
}

void Transaction::setCommitted() {
  _isCommitted = true;
}

char* Transaction::getTmpFilename(const char* filename, bool isPmem = false) {
  return get(filename, isPmem);
};
