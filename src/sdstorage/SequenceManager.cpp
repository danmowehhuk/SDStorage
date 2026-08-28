#include "SequenceManager.h"

using namespace SDStorageStrings;

// PROGMEM key for the single field persisted in a sequence file
static const char _SDSTORAGE_SEQ_VALUE_KEY[] PROGMEM = "v";

// Minimal internal DTO wrapping a sequence's persisted uint64_t value.
// Never exposed outside this file - SDStorage users only ever see the
// uint64_t (or converted string) that comes out of current()/next().
class SequenceValue: public StreamableDTO {
  public:
    uint64_t value = 0;

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
