#include "Transaction.h"

static uint16_t Transaction::_idSeq = 0;

static StreamableDTO* Transaction::_locks = new StreamableDTO();

void Transaction::add(const String &filename, const String &workDir) {
  bool didLog = false;
  while (_locks->exists(filename.c_str())) {
    // wait until the key is removed (lock released)
#if defined(DEBUG)
    if (!didLog) {
      Serial.println(String(F("Lock contention on ")) + filename);
      didLog = true;
    }
#endif
  }
  _locks->putEmpty(filename.c_str());

  size_t bufferSize = workDir.length() + 16; // for /<seq_no>.tmp
  char tmpFilename[bufferSize];
  snprintf(tmpFilename, bufferSize, "%s/%s%s", workDir.c_str(), 
        String(_idSeq++).c_str(), reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_TXN_TMP_EXTSN));
  put(filename.c_str(), strdup(tmpFilename));
}

void Transaction::releaseLocks() {
  auto unlockFunction = [](const char* filename, const char* tmpFilename, bool keyPmem, bool valPmem, void* capture) -> bool {
    bool result = _locks->remove(filename);
    return result;
  };
  processEntries(unlockFunction, nullptr);
}
