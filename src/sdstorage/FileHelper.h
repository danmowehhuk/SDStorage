#ifndef _SDStorage_FileHelper_h
#define _SDStorage_FileHelper_h


#include <Arduino.h>

static const char _SDSTORAGE_WORK_DIR[]          PROGMEM = "~WORK";
static const char _SDSTORAGE_IDX_DIR[]           PROGMEM = "~IDX";

class FileHelper {

  public:
    FileHelper(const char* rootDir, bool isRootDirPmem);
    ~FileHelper();

    const char* getRootDir() { return _rootDir; };
    const char* getWorkDir() { return _workDir; };
    const char* getIdxDir() { return _idxDir; };

    /*
     * Returns the fully qualified path to a file. These methods return
     * dynamically allocated char[]'s that the caller needs to release.
     */
    char* canonicalFilename(const char* filename, bool isFilenamePmem = false);
    char* canonicalFilename(const __FlashStringHelper* filename);

    // Disable moving and copying
    FileHelper(FileHelper&& other) = delete;
    FileHelper& operator=(FileHelper&& other) = delete;
    FileHelper(const FileHelper&) = delete;
    FileHelper& operator=(const FileHelper&) = delete;

  private:
    char* _rootDir = nullptr;
    char* _workDir = nullptr;
    char* _idxDir = nullptr;

    /*
     * Various filename helpers
     */
    static bool isValidFAT16Filename(const char* filename);
    static bool isValidFAT16Filename(const __FlashStringHelper* filename);
    static char* getPathFromFilename(const char* filename);
    static char* getPathFromFilename(const __FlashStringHelper* filename);
    static char* getFilenameFromFullName(const char* filename);
    static char* getFilenameFromFullName(const __FlashStringHelper* filename);

    friend class SDStorage;
    friend class TransactionManager;
    friend class SDStorageTestHelper;

};


#endif