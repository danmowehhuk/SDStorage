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

  char seqFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->sequenceFilename(seq, seqFilename, FileHelper::MAX_FILENAME_LENGTH)) return 0;

  // If a prior next() call already ran on this same still-open explicit
  // transaction, its result is sitting unread in the txn's tmp file - the
  // committed file hasn't changed yet. Read that pending value instead of
  // current()'s committed value, or a second call on the same txn would
  // silently return the same value twice.
  uint64_t curr = 0;
  bool usedPendingTxnValue = false;
  if (txn != nullptr) {
    char* pendingTmpFilename = _txnManager->getTmpFilename(txn, seqFilename);
    if (!isEmpty(pendingTmpFilename) && _storageProvider->_exists(pendingTmpFilename, testState)) {
      SequenceValue pendingVal;
      bool loadedOk = _storageProvider->_loadFromStream(pendingTmpFilename, &pendingVal, testState);
      if (!loadedOk || pendingVal.value == 0) {
        // The txn's own pending tmp file exists but couldn't be read, or
        // yielded 0 - next() never writes 0, and addFileToTxn never
        // pre-creates a placeholder, so a pending tmp file always holds a
        // value >= 1. Treat this the same as the committed-file anomaly
        // below rather than silently overwriting the pending value with 1.
        if (_errFunction != nullptr) _errFunction();
        return 0;
      }
      curr = pendingVal.value;
      usedPendingTxnValue = true;
    }
  }
  if (!usedPendingTxnValue) {
    curr = current(seq, testState);
    if (curr == 0 && _storageProvider->_exists(seqFilename, testState)) {
      // The file exists but yielded 0, which next() never legitimately writes.
      // This is a read/parse failure (or corruption), not a brand-new sequence -
      // let the caller's errFunction decide rather than silently resetting what
      // might have been a much larger counter.
      if (_errFunction != nullptr) _errFunction();
      return 0;
    }
  }
  if (curr == UINT64_MAX) {
    // Incrementing would wrap to 0, risking a collision with the sequence's
    // initial value. Let the caller's errFunction decide how to handle this
    // rather than hanging or silently wrapping.
    if (_errFunction != nullptr) _errFunction();
    return 0;
  }
  uint64_t nextVal = curr + 1;

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

bool SequenceManager::current(Sequence seq, char* out, SeqToString f, void* testState = nullptr) {
  if (!out || !f) return false;
  return f(current(seq, testState), out);
}

bool SequenceManager::next(void* testState, Sequence seq, char* out, SeqToString f, Transaction* txn = nullptr) {
  if (!out || !f) return false;
  uint64_t val = next(testState, seq, txn);
  if (val == 0) return false;
  return f(val, out);
}
