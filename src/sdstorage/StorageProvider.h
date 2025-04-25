#ifndef _SDStorage_StorageProvider_h
#define _SDStorage_StorageProvider_h


#include <StreamableManager.h>
#if defined(__SDSTORAGE_TEST)
  #include "../../test/test-suite-sim/MockSdFat.h"
#else
  #include <SdFat.h>
#endif

class StorageProvider {

  public:
    StorageProvider(uint8_t sdCsPin) {};

    // Disable moving and copying
    StorageProvider(StorageProvider&& other) = delete;
    StorageProvider& operator=(StorageProvider&& other) = delete;
    StorageProvider(const StorageProvider&) = delete;
    StorageProvider& operator=(const StorageProvider&) = delete;

  private:
    uint8_t _sdCsPin;         // SD card chip select pin
    StreamableManager _streams;
#if defined(__SDSTORAGE_TEST)
    MockSdFat _sd;
#else
    SdFat _sd;
#endif

    bool begin() {
      return _sd.begin(_sdCsPin);
    }

    /*
     * Wrap the underlying calls to _sd so that a state capture object
     * can be passed to MockSdFat when testing
     */
    bool _exists(const char* filename, void* testState = nullptr);
    bool _mkdir(const char* filename, void* testState = nullptr);
    void _writeTxnToStream(const char* filename, StreamableDTO* dto, void* testState = nullptr);
    bool _isDir(const char* filename, void* testState = nullptr);
    bool _remove(const char* filename, void* testState = nullptr);
    bool _rename(const char* oldFilename, const char* newFilename, void* testState = nullptr);

    friend class SDStorage;
    friend class TransactionManager;

};


#endif