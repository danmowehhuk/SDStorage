#ifndef _MockSdFat_h
#define _MockSdFat_h


#include <Arduino.h>
#include <StringStream.h>

static const char _MOCK_TESTROOT[] PROGMEM = "TESTROOT";

// A read-only index file over a string, which can seek like a real file
// and counts the bytes read through it into *bytesRead.
class MockIndexFile : public Stream {
  public:
    MockIndexFile(const char* data, uint16_t* bytesRead):
        _data(data), _size(data ? strlen(data) : 0), _bytesRead(bytesRead) {};

    MockIndexFile(MockIndexFile&& other) = delete;
    MockIndexFile& operator=(MockIndexFile&& other) = delete;
    MockIndexFile(const MockIndexFile&) = delete;
    MockIndexFile& operator=(const MockIndexFile&) = delete;

    int available() override { return _size - _position; };

    int read() override {
      int c = -1;
      if (_position < _size) {
        c = static_cast<unsigned char>(_data[_position++]);
        (*_bytesRead)++;
      }
      return c;
    };

    int peek() override { return _position < _size ? static_cast<unsigned char>(_data[_position]) : -1; };

    size_t write(uint8_t) override { return 0; };

    bool seek(uint32_t position) {
      bool success = position <= _size;
      if (success) {
        _position = position;
      }
      return success;
    };

    uint32_t position() { return _position; };
    uint32_t size() { return _size; };

  private:
    const char* _data;
    uint32_t _size;
    uint32_t _position = 0;
    uint16_t* _bytesRead;
};

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
      bool onRemoveTreeReturn = false;
      bool onRenameReturn = false;
      char* mkdirCaptor = nullptr;
      char* onLoadData = nullptr;
      char* onReadIdxData = nullptr;
      char* loadFilenameCaptor = nullptr;
      char* writeTxnFilenameCaptor = nullptr;
      char* removeCaptor = nullptr;
      char* removeTreeCaptor = nullptr;
      char* renameOldCaptor = nullptr;
      char* renameNewCaptor = nullptr;
      char* readIdxFilenameCaptor = nullptr;
      char* writeIdxFilenameCaptor = nullptr;
      uint16_t idxBytesRead = 0;
      StringStream writeDataCaptor;
      StringStream writeTxnDataCaptor;
      StringStream writeIdxDataCaptor;

      ~TestState() {
        if (mkdirCaptor) free(mkdirCaptor);
        if (onLoadData) free(onLoadData);
        if (loadFilenameCaptor) free(loadFilenameCaptor);
        if (onReadIdxData) free(onReadIdxData);
        if (writeTxnFilenameCaptor) free(writeTxnFilenameCaptor);
        if (removeCaptor) free(removeCaptor);
        if (removeTreeCaptor) free(removeTreeCaptor);
        if (renameOldCaptor) free(renameOldCaptor);
        if (renameNewCaptor) free(renameNewCaptor);
        if (readIdxFilenameCaptor) free(readIdxFilenameCaptor);
        if (writeIdxFilenameCaptor) free(writeIdxFilenameCaptor);
        mkdirCaptor = nullptr;
        onLoadData = nullptr;
        onReadIdxData = nullptr;
        loadFilenameCaptor = nullptr;
        writeTxnFilenameCaptor = nullptr;
        removeCaptor = nullptr;
        removeTreeCaptor = nullptr;
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

    bool removeTree(const char* dirName, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->removeTreeCaptor) free(ts->removeTreeCaptor);
      ts->removeTreeCaptor = strdup(dirName);
      return ts->onRemoveTreeReturn;
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
      if (ts->readIdxFilenameCaptor) free(ts->readIdxFilenameCaptor);
      ts->readIdxFilenameCaptor = nullptr;
      ts->readIdxFilenameCaptor = strdup(filename);
      StringStream* ss = new StringStream(ts->onReadIdxData);
      return ss;
    };

    // As readIndexFileStream(), but seekable. The caller deletes it.
    MockIndexFile* readIndexFile(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->readIdxFilenameCaptor) free(ts->readIdxFilenameCaptor);
      ts->readIdxFilenameCaptor = strdup(filename);
      return new MockIndexFile(ts->onReadIdxData, &ts->idxBytesRead);
    };

    Stream* writeIndexFileStream(const char* filename, void* testState) {
      TestState* ts = static_cast<TestState*>(testState);
      if (ts->writeIdxFilenameCaptor) free(ts->writeIdxFilenameCaptor);
      ts->writeIdxFilenameCaptor = nullptr;
      ts->writeIdxFilenameCaptor = strdup(filename);
      return &(ts->writeIdxDataCaptor);
    };

};


#endif