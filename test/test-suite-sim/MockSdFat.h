#ifndef _MockSdFat_h
#define _MockSdFat_h


#include <Arduino.h>
#include <StringStream.h>

static const char _MOCK_TESTROOT[] PROGMEM = "TESTROOT";

class MockSdFat {

  private:
    MockSdFat(MockSdFat &t) = delete;
    String file1str;
    StringStream file1;

  public:
    struct TestState {
      bool onExistsReturn = false;
      String mkdirCaptor = String();
      String onLoadData = String();
      String loadFilenameCaptor = String();
      String removeCaptor = String();
      bool onRemoveReturn = false;
      StringStream writeDataCaptor;
      String writeFilenameCaptor = String();
      StringStream writeTxnDataCaptor;
      String writeTxnFilenameCaptor = String();
      String renameOldCaptor = String();
      String renameNewCaptor = String();
      bool onRenameReturn = false;
      String onReadIdxData = String();
      String readIdxFilenameCaptor = String();
      StringStream writeIdxDataCaptor;
      String writeIdxFilenameCaptor = String();

    };

    MockSdFat(): file1str("foo=bar\nabc=def\n"),
      file1(file1str) {};

    bool begin(uint8_t sdCsPin) { return true; };
    bool exists(const String& filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      return ts->onExistsReturn;
    };
    bool mkdir(const String& filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      ts->mkdirCaptor = filename;
      return true;
    };
    bool remove(const String& filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      ts->removeCaptor = filename;
      return ts->onRemoveReturn;
    };

    bool rename(const String& oldFilename, const String& newFilename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      ts->renameOldCaptor = oldFilename;
      ts->renameNewCaptor = newFilename;
      return ts->onRenameReturn;
    };

    Stream* loadFileStream(const String& filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      ts->loadFilenameCaptor = filename;
      StringStream* ss = new StringStream(ts->onLoadData);
      return ss;
    };

    Stream* writeFileStream(const String& filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      return &(ts->writeDataCaptor);
    };

    Stream* writeTxnFileStream(const String& filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      ts->writeTxnFilenameCaptor = filename;
      return &(ts->writeTxnDataCaptor);
    };

    Stream* readIndexFileStream(const String& filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      ts->readIdxFilenameCaptor = filename;
      StringStream* ss = new StringStream(ts->onReadIdxData);
      return ss;
    };

    Stream* writeIndexFileStream(const String& filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      ts->writeIdxFilenameCaptor = filename;
      return &(ts->writeIdxDataCaptor);
    };

};


#endif