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

uint64_t SequenceManager::loadStoredValue(const char* seqFilename, void* testState, bool* fileExists, bool* anomalous) {
  *fileExists = false;
  *anomalous = false;
  if (!_storageProvider->_exists(seqFilename, testState)) return 0;
  *fileExists = true;
  SequenceValue val;
  if (!_storageProvider->_loadFromStream(seqFilename, &val, testState) || val.value == 0) {
    *anomalous = true;
    return 0;
  }
  return val.value;
}

uint64_t SequenceManager::current(Sequence seq, void* testState = nullptr) {
  if (!seq.name) return 0;
  char seqFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->sequenceFilename(seq, seqFilename, FileHelper::MAX_FILENAME_LENGTH)) return 0;

  bool fileExists = false, anomalous = false;
  uint64_t value = loadStoredValue(seqFilename, testState, &fileExists, &anomalous);
  if (!fileExists || anomalous) {
    // Reading a sequence before seqNext()/init() has ever written it is a
    // usage error, not a legitimate "starts at 0" state - only next()'s own
    // bootstrap path (below) is allowed to treat a missing file as 0. An
    // existing-but-unreadable/corrupt file is likewise always an error here.
    if (_errFunction != nullptr) _errFunction();
    return 0;
  }
  return value;
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
    // Unlike the public current(), next() legitimately treats "file doesn't
    // exist yet" as the start of a brand-new sequence (0, about to become
    // 1) - only an existing-but-unreadable/corrupt file is an error here.
    bool fileExists = false, anomalous = false;
    curr = loadStoredValue(seqFilename, testState, &fileExists, &anomalous);
    if (anomalous) {
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

bool SequenceManager::init(void* testState, Sequence seq, uint64_t initialValue, Transaction* txn = nullptr) {
  if (!seq.name) return false;
  if (initialValue == 0) {
    // 0 is reserved throughout this API to mean "never initialized" (and is
    // never a value next()/init() legitimately persist) - an explicit
    // initial value of 0 would be indistinguishable from that state, and
    // from a corrupted read, on every later current()/next() call.
    if (_errFunction != nullptr) _errFunction();
    return false;
  }
  char seqFilename[FileHelper::MAX_FILENAME_LENGTH];
  if (!_fileHelper->sequenceFilename(seq, seqFilename, FileHelper::MAX_FILENAME_LENGTH)) return false;

  bool isImplicitTxn = (txn == nullptr);
  if (isImplicitTxn) {
    txn = _txnManager->beginTxn(testState, seqFilename);
  }
  if (!txn) return false;

  char* tmpFilename = _txnManager->getTmpFilename(txn, seqFilename);
  bool success = false;
  if (!isEmpty(tmpFilename)) {
    SequenceValue val;
    val.setValue(initialValue);
    success = _storageProvider->_writeToStream(tmpFilename, &val, testState);
  }
  bool committed = _txnManager->finalizeTxn(txn, isImplicitTxn, success, testState);
  return success && committed;
}

bool SequenceManager::current(Sequence seq, char* out, SeqToString f, void* testState = nullptr) {
  if (!out || !f) return false;
  uint64_t val = current(seq, testState);
  if (val == 0) return false; // current() already invoked errFunction on failure
  return f(val, out);
}

bool SequenceManager::next(void* testState, Sequence seq, char* out, SeqToString f, Transaction* txn = nullptr) {
  if (!out || !f) return false;
  uint64_t val = next(testState, seq, txn);
  if (val == 0) return false;
  return f(val, out);
}
