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

    size_t getBufferSize() const { return _streams.getBufferSize(); };

  private:
    uint8_t _sdCsPin;         // SD card chip select pin
    StreamableManager _streams;
#if defined(__SDSTORAGE_TEST)
    MockSdFat _sd;
#elif defined(NO_ARDUINO)
    // Carries FatFs's ~512-byte window buffer as static RAM (FF_FS_TINY=1
    // in ffconf.h) - worth knowing for anyone considering a smaller AVR part.
    FATFS _fatfs;
#else
    SdFat _sd;
#endif

#if defined(NO_ARDUINO)
    // Caller must have already called BareMetalHAL::spiBegin(sck, mosi,
    // miso) to configure the SPI bus - this only sets the chip-select
    // pin and mounts the filesystem over that already-configured bus.
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
    // Removes dirName and everything in it; true if it doesn't exist.
    bool _removeTree(const char* dirName, void* testState = nullptr);
    bool _rename(const char* oldFilename, const char* newFilename, void* testState = nullptr);
    bool _writeIndexLine(const char* indexFilename, const char* line, void* testState = nullptr);
    // Rewrites indexFilename into tmpFilename through filter, dropping repeated
    // keys. False if it couldn't, or if filter stopped it: tmpFilename is then
    // incomplete and must not be used.
    bool _updateIndex(const char* indexFilename, const char* tmpFilename, 
          StreamableManager::FilterFunction filter, void* statePtr, void* testState = nullptr);

    // Pipes an index's lines through filter until the end of the index or
    // until filter returns false. A binary search skips the lines before
    // startKey, so the index must be sorted, as every index write keeps it.
    // filter may still see a few lines with smaller keys first.
    // Params:
    //   indexFilename - the index file's absolute name
    //   startKey      - the key to start from; "" starts from the first line
    //   filter        - called with each line from there on
    //   statePtr      - passed to filter
    //   testState     - the mock's state, in the test build
    // Returns true if the index was read, false if it couldn't be opened
    // or read.
    bool _scanIndexFrom(const char* indexFilename, const char* startKey,
          StreamableManager::FilterFunction filter, void* statePtr, void* testState = nullptr);

    // Once a search narrows an index down to this many bytes, it scans them
    // line by line. A smaller window is no faster on an ATmega2560: each
    // extra probe parses a line, costing about as much as the lines it saves.
    static const uint16_t SEARCH_WINDOW = 256;

    template <typename IndexFile>
    bool _scanIndexFileFrom(IndexFile& file, const char* startKey,
          StreamableManager::FilterFunction filter, void* statePtr);

    template <typename IndexFile>
    uint32_t _findSearchStart(IndexFile& file, const char* key);

    friend class SDStorage;
    friend class SDStorageTestHelper;
    friend class TransactionManager;
    friend class IndexManager;
    friend class SequenceManager;

};


#endif