#include "StorageProvider.h"

/******
 *
 * The following wrapper methods allow sending a state object to a
 * mock version of SdFat when testing on a simulator. These all expect
 * the absolute filename as returned by realFilename(...)
 *
 ******/

bool StorageProvider::_exists(const char* filename, void* testState = nullptr) {
#if defined(__SDSTORAGE_TEST)
  return _sd.exists(filename, testState);
#elif defined(NO_ARDUINO)
  FILINFO info;
  return f_stat(filename, &info) == FR_OK;
#else
  return _sd.exists(filename);
#endif
}

bool StorageProvider::_mkdir(const char* filename, void* testState = nullptr) {
#if defined(__SDSTORAGE_TEST)
  return _sd.mkdir(filename, testState);
#elif defined(NO_ARDUINO)
  return f_mkdir(filename) == FR_OK;
#else
  return _sd.mkdir(filename);
#endif
}

bool StorageProvider::_writeTxnToStream(const char* filename, Transaction* txn, void* testState = nullptr) {
  Stream* dest = nullptr;
#if defined(__SDSTORAGE_TEST)
  dest = _sd.writeTxnFileStream(filename, testState);
  if (!dest) return false;
#elif defined(NO_ARDUINO)
  File file;
  if (!file.open(filename, FA_CREATE_ALWAYS | FA_WRITE)) return false;
  dest = &file;
#else
  File file = _sd.open(filename, FILE_WRITE);
  if (!file) return false;
  dest = &file;
#endif
  _streams.send(dest, txn);
#if (!defined(__SDSTORAGE_TEST))
  file.close();
#endif
  return true;
}

bool StorageProvider::_isDir(const char* filename, void* testState = nullptr) {
  bool isDir = false;
#if defined(__SDSTORAGE_TEST)
  isDir = _sd.isDirectory(filename, testState);
#elif defined(NO_ARDUINO)
  File file;
  file.open(filename, FA_READ);
  isDir = file.isDirectory();
  file.close();
#else
  File file = _sd.open(filename);
  isDir = file.isDirectory();
  file.close();
#endif
  return isDir;
}

bool StorageProvider::_remove(const char* filename, void* testState = nullptr) {
#if defined(__SDSTORAGE_TEST)
  return _sd.remove(filename, testState);
#elif defined(NO_ARDUINO)
  return f_unlink(filename) == FR_OK;
#else
  return _sd.remove(filename);
#endif
}

bool StorageProvider::_rename(const char* oldFilename, const char* newFilename, void* testState = nullptr) {
#if defined(__SDSTORAGE_TEST)
  return _sd.rename(oldFilename, newFilename, testState);
#elif defined(NO_ARDUINO)
  return f_rename(oldFilename, newFilename) == FR_OK;
#else
  return _sd.rename(oldFilename, newFilename);
#endif
}

bool StorageProvider::_loadFromStream(const char* filename, StreamableDTO* dto, void* testState = nullptr) {
  Stream* src = nullptr;
#if defined(__SDSTORAGE_TEST)
  src = _sd.loadFileStream(filename, testState);
#elif defined(NO_ARDUINO)
  File file;
  file.open(filename, FA_READ);
  src = &file;
#else
  File file = _sd.open(filename, FILE_READ);
  src = &file;
#endif
  bool result = _streams.load(src, dto);
#if defined(__SDSTORAGE_TEST)
  StringStream* ss = static_cast<StringStream*>(src);
  delete ss;
#else
  file.close();
#endif
  return result;
}

bool StorageProvider::_writeToStream(const char* filename, StreamableDTO* dto, void* testState = nullptr) {
  Stream* dest = nullptr;
#if defined(__SDSTORAGE_TEST)
  dest = _sd.writeFileStream(filename, testState);
  if (!dest) return false;
#elif defined(NO_ARDUINO)
  File file;
  if (!file.open(filename, FA_CREATE_ALWAYS | FA_WRITE)) return false;
  dest = &file;
#else
  File file = _sd.open(filename, FILE_WRITE);
  if (!file) return false;
  dest = &file;
#endif
  _streams.send(dest, dto);
#if (!defined(__SDSTORAGE_TEST))
  file.close();
#endif
  return true;
}

bool StorageProvider::_writeIndexLine(const char* indexFilename, const char* line, void* testState = nullptr) {
  Stream* dest = nullptr;
#if defined(__SDSTORAGE_TEST)
  dest = _sd.writeIndexFileStream(indexFilename, testState);
  if (!dest) return false;
#elif defined(NO_ARDUINO)
  // FA_OPEN_APPEND (open-existing-or-create, seek to end) matches
  // SdFat's own FILE_WRITE (O_RDWR|O_CREAT|O_AT_END) - not
  // FA_CREATE_ALWAYS, which truncates. This call site can run against
  // a tmpFilename that idxUpsert() already piped existing index lines
  // into (via _updateIndex) before appending a new key - truncating
  // here would silently discard that content. Confirmed as a real bug
  // on real hardware: a second idxUpsert() call was wiping the first
  // upserted key.
  File file;
  if (!file.open(indexFilename, FA_OPEN_APPEND | FA_WRITE)) return false;
  dest = &file;
#else
  File file = _sd.open(indexFilename, FILE_WRITE);
  if (!file) return false;
  dest = &file;
#endif
  for (size_t i = 0; i < strlen(line); i++) {
    dest->write(line[i]);
  }
  dest->write('\n');
#if (!defined(__SDSTORAGE_TEST))
  file.close();
#endif
  return true;
}

bool StorageProvider::_updateIndex(
      const char* indexFilename, const char* tmpFilename,
      StreamableManager::FilterFunction filter, void* statePtr, void* testState = nullptr) {
  Stream* src = nullptr;
  Stream* dest = nullptr;
#if defined(__SDSTORAGE_TEST)
  src = _sd.readIndexFileStream(indexFilename, testState);
  dest = _sd.writeIndexFileStream(tmpFilename, testState);
#elif defined(NO_ARDUINO)
  File srcFile;
  if (!srcFile.open(indexFilename, FA_READ)) return false;
  File destFile;
  if (!destFile.open(tmpFilename, FA_CREATE_ALWAYS | FA_WRITE)) {
    srcFile.close();
    return false;
  }
  src = &srcFile;
  dest = &destFile;
#else
  File srcFile = _sd.open(indexFilename, FILE_READ);
  if (!srcFile) return false;
  File destFile = _sd.open(tmpFilename, FILE_WRITE);
  if (!destFile) {
    srcFile.close();
    return false;
  }
  src = &srcFile;
  dest = &destFile;
#endif
  _streams.pipe(src, dest, filter, false, statePtr);
#if defined(__SDSTORAGE_TEST)
  StringStream* ss = static_cast<StringStream*>(src);
  delete ss;
#else
  srcFile.close();
  destFile.close();
#endif
  return true;
}

bool StorageProvider::_scanIndex(const char* indexFilename, StreamableManager::FilterFunction filter, void* statePtr,
      void* testState = nullptr) {
  Stream* src = nullptr;
#if defined(__SDSTORAGE_TEST)
  src = _sd.readIndexFileStream(indexFilename, testState);
#elif defined(NO_ARDUINO)
  File srcFile;
  if (!srcFile.open(indexFilename, FA_READ)) return false;
  src = &srcFile;
#else
  File srcFile = _sd.open(indexFilename, FILE_READ);
  if (!srcFile) return false;
  src = &srcFile;
#endif
  _streams.pipe(src, nullptr, filter, false, statePtr);
#if defined(__SDSTORAGE_TEST)
  StringStream* ss = static_cast<StringStream*>(src);
  delete ss;
#else
  srcFile.close();
#endif
  return true;
}

