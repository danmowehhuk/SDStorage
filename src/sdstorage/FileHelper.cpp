#include "FileHelper.h"
#include "Strings.h"


using namespace SDStorageStrings;

const size_t FileHelper::MAX_FILENAME_LENGTH = 64;

FileHelper::FileHelper(const char* rootDir, bool isRootDirPmem) {
  _rootDir = isRootDirPmem ? strdup_P(rootDir) : strdup(rootDir);
  trim(_rootDir);

  // Make sure rootDir starts with "/"
  if (_rootDir[0] != '/') {
    size_t rootLen = strlen(_rootDir);
    char* newRoot = (char*)malloc(rootLen + 2); // uses free like strdup_P
    newRoot[0] = '/';
    strcpy(newRoot + 1, _rootDir);
    free(_rootDir);
    _rootDir = nullptr;
    newRoot[rootLen + 1] = '\0';
    _rootDir = newRoot;
  }
  char buffer[MAX_FILENAME_LENGTH];
  bool result = true;
  result &= canonicalFilename(Filename::fromProgmem(_SDSTORAGE_IDX_DIR), buffer, MAX_FILENAME_LENGTH);
  if (result) _idxDir = strdup(buffer);
  result &= canonicalFilename(Filename::fromProgmem(_SDSTORAGE_WORK_DIR), buffer, MAX_FILENAME_LENGTH);
  if (result) _workDir = strdup(buffer);
#if (defined(DEBUG))
  if (!result) {
    Serial.println(F("FileHelper initialization failed"));
  }
#endif
};

FileHelper::~FileHelper() {
  if (_rootDir) free(_rootDir);
  if (_workDir) free(_workDir);
  if (_idxDir) free(_idxDir);
  _rootDir = nullptr;
  _workDir = nullptr;
  _idxDir = nullptr;
}

/*
 * Prepends the filename with the root directory if it isn't already.
 * Example:
 *    "foo.txt"             -->  "/<rootdir>/foo.txt"
 *    "/foo.txt"            -->  "/<rootdir>/foo.txt"
 *    "/<rootdir>/foo.txt"  -->   "/<rootdir>/foo.txt"
 */
bool FileHelper::canonicalFilename(Filename filename, char* buffer, size_t bufferSize) {
  if (!filename.name || !buffer || bufferSize == 0 || !verifyBufferSize(bufferSize)) return false;

  const char* filenameRAM = filename.isPmem ? strdup_P(filename.name) : strdup(filename.name);
  size_t rootLen = strlen(_rootDir);
  size_t filenameLen = strlen(filenameRAM);
  int n = 0;
  if (strncmp(filenameRAM, _rootDir, rootLen) == 0 && filenameRAM[rootLen] == '/') {
    // Already prefixed — just return a copy
    static const char fmt[] PROGMEM = "%s";
    n = snprintf_P(buffer, bufferSize, fmt, filenameRAM);
  } else {
    // Otherwise construct new filename
    if (filenameRAM[0] == '/') {
      static const char fmt[] PROGMEM = "%s%s";
      snprintf_P(buffer, bufferSize, fmt, _rootDir, filenameRAM);
    } else {
      static const char fmt[] PROGMEM = "%s/%s";
      snprintf_P(buffer, bufferSize, fmt, _rootDir, filenameRAM);
    }
  }
  bool success = true;
  if (n < 0 || static_cast<size_t>(n) >= bufferSize) {
    // Truncated or error
    buffer[bufferSize - 1] = '\0';
    success = false;
  }
  free(filenameRAM);
  return success;
}

bool FileHelper::isValidFAT16Filename(const char* filename) {
  if (!filename) return false;
  if (strlen(filename) == 0) {
    return false;
  }

  // Check for more than one dot or invalid length
  const char* dot = strchr(filename, '.');
  size_t nameLen = 0;
  size_t extLen = 0;

  if (!dot) {
    // No extension
    nameLen = strlen(filename);
    if (nameLen == 0 || nameLen > 8) return false;
  } else {
    // With extension
    nameLen = dot - filename;
    extLen = strlen(dot + 1);
    if (nameLen == 0 || nameLen > 8) return false;
    if (extLen == 0 || extLen > 3) return false;
  }

  bool result = true;
  // Check each character
  size_t len = strlen(filename);
  for (size_t i = 0; i < len; i++) {
    char c = filename[i];
    if (!((c >= 'A' && c <= 'Z') ||  // Uppercase letters
          (c >= 'a' && c <= 'z') ||  // Lowercase letters
          (c >= '0' && c <= '9') ||  // Numbers
          (c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
           c == '\'' || c == '(' || c == ')' || c == '-' || c == '@' ||
           c == '^' || c == '_' || c == '`' || c == '{' || c == '}' ||
           c == '~' || (c == '.' && i != 0 && i != len - 1)))) {
      result = false;
      break;
    }
  }
  return result;
}

bool FileHelper::getPathFromFilename(const char* filename, char* buffer, size_t bufferSize) {
  if (!filename || !buffer || bufferSize == 0 || !verifyBufferSize(bufferSize)) return false;
  // Check for null, empty or all whitespace
  if (*filename == '\0' || strspn(filename, " \t\r\n") == strlen(filename)) {
    return false;
  }

  bool result = false;
  do {
    const char* lastSlash = strrchr(filename, '/');
    if (!lastSlash) break;
    size_t pathLen = lastSlash - filename;
    if (pathLen == 0) pathLen = 1; // Root case "/"
    if (pathLen + 1 >= MAX_FILENAME_LENGTH) {
#if (defined(DEBUG))
      Serial.print(F("pathLen exceeds allowed maximum of "));
      Serial.println(MAX_FILENAME_LENGTH);
#endif
      break;
    }
    strncpy(buffer, filename, pathLen);
    buffer[pathLen] = '\0';

    result = true;
  } while (false);
  return result;
}

bool FileHelper::getFilenameFromFullName(const char* filename, char* buffer, size_t bufferSize) {
  if (!filename || !buffer || bufferSize == 0 || !verifyBufferSize(bufferSize)) return false;
  // Check for null, empty or all whitespace
  if (*filename == '\0' || strspn(filename, " \t\r\n") == strlen(filename)) {
    return false;
  }

  bool result = false;
  do {
    const char* lastSlash = strrchr(filename, '/');
    const char* baseName = (lastSlash != nullptr) ? (lastSlash + 1) : filename;
    size_t baseNameLen = strlen(baseName);
    if (baseNameLen + 1 >= MAX_FILENAME_LENGTH) {
#if (defined(DEBUG))
      Serial.print(F("pathLen exceeds allowed maximum of "));
      Serial.println(MAX_FILENAME_LENGTH);
#endif
      break;
    }
    strncpy(buffer, baseName, baseNameLen);
    buffer[baseNameLen] = '\0';

    result = true;
  } while (false);
  return result;
}

bool FileHelper::indexFilename(sdstorage::Index idx, char* buffer, size_t bufferSize) {
  if (!idx.name || !buffer || bufferSize == 0 || !verifyBufferSize(bufferSize)) return false;

  char* extRAM = strdup_P(_SDSTORAGE_INDEX_EXTSN);
  char* idxNameRAM = idx.isPmem ? strdup_P(idx.name) : strdup(idx.name);
  char* idxDir = getIdxDir();
  static const char fmt[] PROGMEM = "%s/%s%s";
  int n = snprintf_P(buffer, bufferSize, fmt, idxDir, idxNameRAM, extRAM);
  bool success = true;
  if (n < 0 || static_cast<size_t>(n) >= bufferSize) {
    // Truncated or error
    buffer[bufferSize - 1] = '\0';
    success = false;
  }
  free(extRAM);
  free(idxNameRAM);
  return success;
}

bool FileHelper::verifyBufferSize(size_t bufferSize) {
  if (bufferSize > MAX_FILENAME_LENGTH) {
#if (defined(DEBUG))
    Serial.print(F("bufferSize exceeds allowed maximum of "));
    Serial.println(MAX_FILENAME_LENGTH);
#endif
    return false;
  }
  return true;
}
