#ifndef _MockSdFat_h
#define _MockSdFat_h


#include <Arduino.h>
#include <StringStream.h>

static const char _MOCK_TESTROOT[] PROGMEM = "TESTROOT";

class MockSdFat {

  public:
    MockSdFat() {};

    // Disable moving and copying
    MockSdFat(MockSdFat&& other) = delete;
    MockSdFat& operator=(MockSdFat&& other) = delete;
    MockSdFat(const MockSdFat&) = delete;
    MockSdFat& operator=(const MockSdFat&) = delete;

    struct TestState {
      uint8_t existsCallCount = 0;
      bool onExistsReturn[8] = { false };
      bool onExistsAlways = false;
      bool onExistsAlwaysReturn = false;
      bool onIsDirectoryReturn = false;
      bool onRemoveReturn = false;
      bool onRenameReturn = false;
      char* mkdirCaptor = nullptr;
      char* onLoadData = nullptr;
      char* onReadIdxData = nullptr;
      char* loadFilenameCaptor = nullptr;
      char* writeTxnFilenameCaptor = nullptr;
      char* removeCaptor = nullptr;
      char* renameOldCaptor = nullptr;
      char* renameNewCaptor = nullptr;
      char* readIdxFilenameCaptor = nullptr;
      char* writeIdxFilenameCaptor = nullptr;
      StringStream writeDataCaptor;
      StringStream writeTxnDataCaptor;
      StringStream writeIdxDataCaptor;

      ~TestState() {
        if (mkdirCaptor) free(mkdirCaptor);
        if (onLoadData) free(onLoadData);
        if (loadFilenameCaptor) free(loadFilenameCaptor);
        if (writeTxnFilenameCaptor) free(writeTxnFilenameCaptor);
        if (removeCaptor) free(removeCaptor);
        if (renameOldCaptor) free(renameOldCaptor);
        if (renameNewCaptor) free(renameNewCaptor);
        if (readIdxFilenameCaptor) free(readIdxFilenameCaptor);
        if (writeIdxFilenameCaptor) free(writeIdxFilenameCaptor);
        mkdirCaptor = nullptr;
        writeTxnFilenameCaptor = nullptr;
        removeCaptor = nullptr;
        renameOldCaptor = nullptr;
        renameNewCaptor = nullptr;
        readIdxFilenameCaptor = nullptr;
        writeIdxFilenameCaptor = nullptr;
      };
    };

    bool begin(uint8_t sdCsPin) { return true; };

    bool mkdir(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      ts->mkdirCaptor = strdup(filename);
      return true;
    };

    bool exists(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      bool result = false;
      if (ts->onExistsAlways) {
        result = ts->onExistsAlwaysReturn;
      } else {
        result = ts->onExistsReturn[ts->existsCallCount++];
      }
      return result;
    };

    bool remove(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->removeCaptor) free(ts->removeCaptor);
      ts->removeCaptor = nullptr;
      ts->removeCaptor = strdup(filename);
      return ts->onRemoveReturn;
    };

    bool rename(const char* oldFilename, const char* newFilename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->renameOldCaptor) free(ts->renameOldCaptor);
      ts->renameOldCaptor = nullptr;
      ts->renameOldCaptor = strdup(oldFilename);
      if (ts->renameNewCaptor) free(ts->renameNewCaptor);
      ts->renameNewCaptor = nullptr;
      ts->renameNewCaptor = strdup(newFilename);
      return ts->onRenameReturn;
    };

    Stream* loadFileStream(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->loadFilenameCaptor) free(ts->loadFilenameCaptor);
      ts->loadFilenameCaptor = nullptr;
      ts->loadFilenameCaptor = strdup(filename);
      StringStream* ss = new StringStream(ts->onLoadData);
      return ss;
    };

    bool isDirectory(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      return ts->onIsDirectoryReturn;
    };

    Stream* writeFileStream(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      return &(ts->writeDataCaptor);
    };

    Stream* writeTxnFileStream(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->writeTxnFilenameCaptor) free(ts->writeTxnFilenameCaptor);
      ts->writeTxnFilenameCaptor = nullptr;
      ts->writeTxnFilenameCaptor = strdup(filename);
      return &(ts->writeTxnDataCaptor);
    };

    Stream* readIndexFileStream(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->writeIdxFilenameCaptor) free(ts->writeTxnFilenameCaptor);
      ts->writeTxnFilenameCaptor = nullptr;
      ts->writeIdxFilenameCaptor = strdup(filename);
      ts->readIdxFilenameCaptor = filename;
      StringStream* ss = new StringStream(ts->onReadIdxData);
      return ss;
    };

    Stream* writeIndexFileStream(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->writeIdxFilenameCaptor) free(ts->writeTxnFilenameCaptor);
      ts->writeTxnFilenameCaptor = nullptr;
      ts->writeIdxFilenameCaptor = strdup(filename);
      return &(ts->writeIdxDataCaptor);
    };

};


#endif