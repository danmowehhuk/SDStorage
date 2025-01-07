#include "Transaction.h"

static uint16_t Transaction::_idSeq = 0;

static StreamableDTO* Transaction::_locks = new StreamableDTO();

void Transaction::add(const String &filename, const String &workDir) {
  bool didLog = false;
//  Serial.println("CHECK  == " + filename);
  while (_locks->exists(filename)) {
    // wait until the key is removed (lock released)
#if defined(DEBUG)
    if (!didLog) {
      Serial.println(String(F("Lock contention on ")) + filename);
      didLog = true;
    }
#endif
  }
  // Serial.println("LOCK   == " + filename);
  _locks->putEmpty(filename);

  size_t bufferSize = workDir.length() + 16; // for /<seq_no>.tmp
  char tmpFilename[bufferSize];
  snprintf(tmpFilename, bufferSize, "%s/%s%s", workDir.c_str(), 
        String(_idSeq++).c_str(), reinterpret_cast<const __FlashStringHelper *>(_SDSTORAGE_TXN_TMP_EXTSN));
  put(filename, String(tmpFilename));
}

void Transaction::releaseLocks() {
  auto unlockFunction = [](const String& filename, const String& tmpFilename, void* capture) -> bool {
    bool result = _locks->remove(filename);
    // Serial.println("UNLOCK == " + filename + " (" + String(result) + ")");
    return result;
  };
  processEntries(unlockFunction, nullptr);
}
