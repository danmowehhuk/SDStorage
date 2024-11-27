#ifndef _SDStorage_h
#define _SDStorage_h


#include <SdFat.h>
#include <StreamableDTO.h>
#include <StreamableManager.h>

class SDStorage {

  public:
    SDStorage(uint8_t sdCsPin, const String &rootDir): 
          _rootDir(rootDir), _sdCsPin(sdCsPin) {};
    bool begin();
    bool mkdir(const String &dirName);

    // FILE OPERATIONS //
    bool load(const String &filename, StreamableDTO* dto);
    bool save(const String &filename, StreamableDTO* dto);
    bool erase(const String &filename);
    bool exists(const String &filename);

  private:
    SDStorage();
    SDStorage(SDStorage &t);
    String realFilename(const String &filename);
    uint8_t _sdCsPin;
    uint8_t _txn = 0; // correlates files in the same transaction
    StreamableManager _streams;
    SdFat _sd;
    String _rootDir;

};

namespace SDStorageStrings {

  static const char W_EXTENSION[]       PROGMEM = ".w~"; // being written
  static const char D_EXTENSION[]       PROGMEM = ".d~"; // done
  static const char C_EXTENSION[]       PROGMEM = ".c~"; // commit ready

};


#endif
