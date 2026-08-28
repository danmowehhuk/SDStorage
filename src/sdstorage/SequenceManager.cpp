#include "SequenceManager.h"
#include <stdint.h>

using namespace SDStorageStrings;

// PROGMEM key for the single field persisted in a sequence file
static const char _SDSTORAGE_SEQ_VALUE_KEY[] PROGMEM = "v";

// Minimal internal DTO wrapping a sequence's persisted uint64_t value.
// Never exposed outside this file - SDStorage users only ever see the
// uint64_t (or converted string) that comes out of current()/next().
class SequenceValue: public StreamableDTO {
  public:
    uint64_t value = 0;

    void setValue(uint64_t v) {
      value = v;
      char strBuffer[21]; // max uint64_t digits (20) + null terminator
      if (uint64ToString(value, strBuffer, sizeof(strBuffer))) {
        put_P(_SDSTORAGE_SEQ_VALUE_KEY, strBuffer);
      }
    }

  protected:
    void parseValue(uint16_t lineNumber, const char* key, const char* val) override {
      if (strcmp_P(key, _SDSTORAGE_SEQ_VALUE_KEY) == 0) {
        value = stringToUint64(val);
      } else {
        StreamableDTO::parseValue(lineNumber, key, val);
      }
    }
};

uint64_t SequenceManager::current(Sequence seq, void* testState = nullptr) {
  if (!seq.name) return 0;
  char seqFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->sequenceFilename(seq, seqFilename, FileHelper::MAX_FILENAME_LENGTH)) return 0;

  SequenceValue val;
  if (_storageProvider->_exists(seqFilename, testState)) {
    if (!_storageProvider->_loadFromStream(seqFilename, &val, testState)) return 0;
  }
  return val.value;
}

uint64_t SequenceManager::next(void* testState, Sequence seq, Transaction* txn = nullptr) {
  if (!seq.name) return 0;

  uint64_t curr = current(seq, testState);
  if (curr == UINT64_MAX) {
    // Incrementing would wrap to 0, risking a collision with the sequence's
    // initial value. Let the caller's errFunction decide how to handle this
    // rather than hanging or silently wrapping.
    if (_errFunction != nullptr) _errFunction();
    return 0;
  }
  uint64_t nextVal = curr + 1;

  char seqFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->sequenceFilename(seq, seqFilename, FileHelper::MAX_FILENAME_LENGTH)) return 0;

  bool isImplicitTxn = (txn == nullptr);
  if (isImplicitTxn) {
    txn = _txnManager->beginTxn(testState, seqFilename);
  }
  if (!txn) return 0;

  char* tmpFilename = _txnManager->getTmpFilename(txn, seqFilename);
  bool success = false;
  if (!isEmpty(tmpFilename)) {
    SequenceValue val;
    val.setValue(nextVal);
    success = _storageProvider->_writeToStream(tmpFilename, &val, testState);
  }
  bool committed = _txnManager->finalizeTxn(txn, isImplicitTxn, success, testState);
  return (success && committed) ? nextVal : 0;
}
