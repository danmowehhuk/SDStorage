#include "FileHelper.h"
#include "Strings.h"


using namespace SDStorageStrings;

FileHelper::FileHelper(const char* rootDir, bool isRootDirPmem) {
  _rootDir = isRootDirPmem ? strdup_P(rootDir) : strdup(rootDir);
  trim(_rootDir);

  // Make sure rootDir starts with "/"
  if (_rootDir[0] != '/') {
    size_t rootLen = strlen(_rootDir);
    char* newRoot = new char[rootLen + 2];
    newRoot[0] = '/';
    strcpy(newRoot + 1, _rootDir);
    delete[] _rootDir;
    _rootDir = nullptr;
    newRoot[rootLen + 1] = '\0';
    _rootDir = newRoot;
  }
  _idxDir = canonicalFilename(_SDSTORAGE_IDX_DIR, true);
  _workDir = canonicalFilename(_SDSTORAGE_WORK_DIR, true);
};

FileHelper::~FileHelper() {
  if (_rootDir) delete[] _rootDir;
  if (_workDir) delete[] _workDir;
  if (_idxDir) delete[] _idxDir;
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
char* FileHelper::canonicalFilename(const char* filename, bool isFilenamePmem = false) {
  const char* filenameRAM = isFilenamePmem ? strdup_P(filename) : filename;
  size_t rootLen = strlen(_rootDir);
  size_t filenameLen = strlen(filenameRAM);
  char* resolvedName = nullptr;
  if (strncmp(filenameRAM, _rootDir, rootLen) == 0 && filenameRAM[rootLen] == '/') {
    // Already prefixed — just return a copy
    resolvedName = strdup(filenameRAM);
  } else {
    // Otherwise construct new filename
    size_t totalLen = rootLen + filenameLen + (filenameRAM[0] == '/' ? 1 : 2);
    resolvedName = new char[totalLen]();
    if (filenameRAM[0] == '/') {
      static const char fmt[] PROGMEM = "%s%s";
      snprintf_P(resolvedName, totalLen, fmt, _rootDir, filenameRAM);
    } else {
      static const char fmt[] PROGMEM = "%s/%s";
      snprintf_P(resolvedName, totalLen, fmt, _rootDir, filenameRAM);
    }
  }
  if (isFilenamePmem) delete[] filenameRAM;
  return resolvedName;
}

char* FileHelper::canonicalFilename(const __FlashStringHelper* filename) {
  return canonicalFilename(reinterpret_cast<const char*>(filename), true);
}

bool FileHelper::isValidFAT16Filename(const char* filename) {
  if (!filename || strlen(filename) == 0) return false;

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
      return false;
    }
  }
  return true;
}

bool FileHelper::isValidFAT16Filename(const __FlashStringHelper* filename) {
  if (!filename) return false;

  char* filenameRAM = strdup(filename);
  bool result = isValidFAT16Filename(filenameRAM);
  free(filenameRAM);
  return result;
}

char* FileHelper::getPathFromFilename(const char* filename) {
  // Check for null, empty or all whitespace
  if (!filename || *filename == '\0' 
        || strspn(filename, " \t\r\n") == strlen(filename)) return nullptr;

  const char* lastSlash = strrchr(filename, '/');
  if (!lastSlash) return nullptr;

  size_t pathLen = lastSlash - filename;
  if (pathLen == 0) pathLen = 1; // Root case "/"
  char* path = new char[pathLen + 1]();
  strncpy(path, filename, pathLen);
  path[pathLen] = '\0';
  return path;
}

char* FileHelper::getPathFromFilename(const __FlashStringHelper* filename) {
  if (!filename) return nullptr;

  char* filenameRAM = strdup(filename);
  char* path = getPathFromFilename(filenameRAM);
  delete[] filenameRAM;
  return path;
}

char* FileHelper::getFilenameFromFullName(const char* filename) {
  // Check for null, empty or all whitespace
  if (!filename || *filename == '\0' 
        || strspn(filename, " \t\r\n") == strlen(filename)) return nullptr;

  const char* lastSlash = strrchr(filename, '/');
  const char* baseName = (lastSlash != nullptr) ? (lastSlash + 1) : filename;
  size_t baseNameLen = strlen(baseName);
  char* result = new char[baseNameLen + 1]();
  strncpy(result, baseName, baseNameLen);
  result[baseNameLen] = '\0';
  return result;
}

char* FileHelper::getFilenameFromFullName(const __FlashStringHelper* filename) {
  if (!filename) return nullptr;

  char* filenameRAM = strdup(filename);
  char* baseName = getFilenameFromFullName(filenameRAM);
  delete[] filenameRAM;
  return baseName;
}


