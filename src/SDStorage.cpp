#include "SDStorage.h"
#include <SdFat.h>
#include <StreamableDTO.h>


bool SDStorage::begin() {
  _rootDir.trim();
  bool sdInit = false;
  do {
    if (!_sd.begin(_sdCsPin)) break;
    if (_rootDir.indexOf('/') != -1) break;
    _rootDir = String(F("/")) + _rootDir;
    if (!_sd.exists(_rootDir) && !_sd.mkdir(_rootDir)) break;

    // fsck & repair

    sdInit = true;
  } while (false);
  return sdInit;
}

String SDStorage::realFilename(const String &filename) {
  String realName;
  bool addSlash = false;
  if (!filename.startsWith('/')) {
    addSlash = true;
  }
  realName.reserve(_rootDir.length() + (addSlash ? 1 : 0) + filename.length());
  realName = _rootDir + (addSlash ? String(F("/")):String()) + filename;
  return realName;
}

bool SDStorage::exists(const String &filename) {
  return _sd.exists(filename);
}

bool SDStorage::mkdir(const String &dirName) {
  return _sd.mkdir(dirName);
}

bool SDStorage::load(const String &filename, StreamableDTO* dto) {
  filename = realFilename(filename);
  if (!_sd.exists(filename)) {
    return false;
  }
  File file = _sd.open(filename, FILE_READ);
  _streams.load(&file, dto);
  file.close();
  return true;
}

/*
 * Perform a safe file save by writing to a temp file, removing the
 * original file, and then renaming the temp file to the original.
 * This ensures that in the event of a failure, at least one complete
 * version of the file is always present for some kind of recovery.
 */
bool SDStorage::save(const String &filename, StreamableDTO* dto) {
  filename = realFilename(filename);


  return true;
}

bool SDStorage::erase(const String &filename) {
  if (!_sd.exists(filename) || !_sd.remove(filename)) {
    return false;
  }
  return true;
}

