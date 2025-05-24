#include "Transaction.h"

static uint16_t Transaction::_idSeq = 0;

static StreamableDTO* Transaction::_locks = new StreamableDTO();

using namespace SDStorageStrings;

Transaction::Transaction(FileHelper* fileHelper):
      StreamableDTO(), _fileHelper(fileHelper) {
  char idBuffer[12];
  static const char fmt[] PROGMEM = "%u";
  snprintf_P(idBuffer, sizeof(idBuffer), fmt, _idSeq++);
  setBaseFilename(idBuffer);
}

Transaction::Transaction(FileHelper* fileHelper, const char* txnFilename):
      StreamableDTO(), _fileHelper(fileHelper) {
  const char* commitExtension = strdup_P(_SDSTORAGE_TXN_COMMIT_EXTSN);
  if (endsWith(txnFilename, commitExtension)) {
    setCommitted();    
  }
  free(commitExtension);
  char shortName[13];
  _fileHelper->getFilenameFromFullName(txnFilename, shortName, sizeof(shortName));

  // trim off the file extension
  char* dot = strchr(shortName, '.');
  if (dot) {
    *dot = '\0';
  }
  setBaseFilename(shortName);
}

void Transaction::setBaseFilename(const char* shortFilename) {
  char* workDir = _fileHelper->getWorkDir();
  size_t workDirLen = strlen(workDir);
  size_t shortNameLen = strlen(shortFilename);
  _baseFilename = new char[workDirLen + shortNameLen + 2](); // +1 for '/' +1 for '\0'
  char* p = _baseFilename;
  memcpy(p, workDir, workDirLen);
  p += workDirLen;
  *p++ = '/';
  memcpy(p, shortFilename, shortNameLen);
  p += shortNameLen;
  *p = '\0'; // null-terminate
}

void Transaction::add(const char* filename) {
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
  size_t bufferSize = strlen(_fileHelper->getWorkDir()) + 1 + strlen(idBuffer) + 5;
  char tmpFilename[bufferSize];
  char* extRAM = strdup_P(_SDSTORAGE_TXN_TMP_EXTSN);
  static const char fmt1[] PROGMEM = "%s/%s%s";
  snprintf_P(tmpFilename, bufferSize, fmt1, _fileHelper->getWorkDir(), idBuffer, extRAM);
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
