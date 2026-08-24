#ifndef _SDStorage_StorageProvider_h
#define _SDStorage_StorageProvider_h


#include <StreamableDTO.h>
#include <StreamableManager.h>
#if defined(__SDSTORAGE_TEST)
  #include "../../test/test-suite-sim/MockSdFat.h"
#elif defined(NO_ARDUINO)
  #include "../hal/File.h"
  #include <BareMetalHAL.h>
  #include "avr/fatfs/ff.h"
  #include "avr/fatfs/sdcard.h"
#else
  #include <SdFat.h>
#endif
#include "Transaction.h"

class StorageProvider {

  public:
    StorageProvider(uint8_t sdCsPin): _sdCsPin(sdCsPin) {};

    // Disable moving and copying
    StorageProvider(StorageProvider&& other) = delete;
    StorageProvider& operator=(StorageProvider&& other) = delete;
    StorageProvider(const StorageProvider&) = delete;
    StorageProvider& operator=(const StorageProvider&) = delete;

    const size_t getBufferSize() const { return _streams.getBufferSize(); };

  private:
    uint8_t _sdCsPin;         // SD card chip select pin
    StreamableManager _streams;
#if defined(__SDSTORAGE_TEST)
    MockSdFat _sd;
#elif defined(NO_ARDUINO)
    FATFS _fatfs;
#else
    SdFat _sd;
#endif

#if defined(__SDSTORAGE_TEST)
    bool begin() {
      return _sd.begin(_sdCsPin);
    }
#elif defined(NO_ARDUINO)
    bool begin() {
      sdDiskSetCsPin(_sdCsPin);
      return f_mount(&_fatfs, "", 1) == FR_OK;
    }
#else
    bool begin() {
      return _sd.begin(_sdCsPin);
    }
#endif

    /*
     * Wrap the underlying calls to _sd so that a state capture object
     * can be passed to MockSdFat when testing
     */
    bool _exists(const char* filename, void* testState = nullptr);
    bool _mkdir(const char* filename, void* testState = nullptr);
    bool _loadFromStream(const char* filename, StreamableDTO* dto, void* testState = nullptr);
    bool _writeToStream(const char* filename, StreamableDTO* dto, void* testState = nullptr);
    bool _writeTxnToStream(const char* filename, Transaction* txn, void* testState = nullptr);
    bool _isDir(const char* filename, void* testState = nullptr);
    bool _remove(const char* filename, void* testState = nullptr);
    bool _rename(const char* oldFilename, const char* newFilename, void* testState = nullptr);
    bool _writeIndexLine(const char* indexFilename, const char* line, void* testState = nullptr);
    bool _updateIndex(const char* indexFilename, const char* tmpFilename, 
          StreamableManager::FilterFunction filter, void* statePtr, void* testState = nullptr);
    bool _scanIndex(const char* indexFilename, StreamableManager::FilterFunction filter, 
          void* statePtr, void* testState = nullptr);

    friend class SDStorage;
    friend class SDStorageTestHelper;
    friend class TransactionManager;
    friend class IndexManager;

};


#endif