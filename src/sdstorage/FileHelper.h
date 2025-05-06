#ifndef _SDStorage_FileHelper_h
#define _SDStorage_FileHelper_h


#include "../Index.h"
#include <Arduino.h>

static const char _SDSTORAGE_WORK_DIR[]          PROGMEM = "~WORK";
static const char _SDSTORAGE_IDX_DIR[]           PROGMEM = "~IDX";
static const char _SDSTORAGE_INDEX_EXTSN[]       PROGMEM = ".idx";

class FileHelper {

  public:
    FileHelper(const char* rootDir, bool isRootDirPmem);
    ~FileHelper();

    // Disable moving and copying
    FileHelper(FileHelper&& other) = delete;
    FileHelper& operator=(FileHelper&& other) = delete;
    FileHelper(const FileHelper&) = delete;
    FileHelper& operator=(const FileHelper&) = delete;

    static const size_t MAX_FILENAME_LENGTH;

  private:
    char* _rootDir = nullptr;
    char* _workDir = nullptr;
    char* _idxDir = nullptr;

    const char* getRootDir() { return _rootDir; };
    const char* getWorkDir() { return _workDir; };
    const char* getIdxDir() { return _idxDir; };

    class Filename {
      public:
        const char* name;
        const bool isPmem;

        explicit Filename(const char* n, bool isPmem = false): 
            name(n), isPmem(isPmem) {};
        explicit Filename(const __FlashStringHelper* n):
            name(reinterpret_cast<const char*>(n)), isPmem(true) {};

        static Filename fromProgmem(const char* n) {
            return Filename(n, true);
        };
    };

    /*
     * Returns the fully qualified path to a file.
     */
    bool canonicalFilename(Filename filename, char* buffer, size_t bufferSize);

    /*
     * Returns the fully-qualified filename of the given index.
     */
    bool indexFilename(sdstorage::Index idx, char* buffer, size_t bufferSize);

    /*
     * Various filename helpers
     */
    static bool isValidFAT16Filename(const char* filename);
    static bool getPathFromFilename(const char* filename, char* buffer, size_t bufferSize);
    static bool getFilenameFromFullName(const char* filename, char* buffer, size_t bufferSize);

    static bool verifyBufferSize(size_t bufferSize);

    friend class IndexManager;
    friend class SDStorage;
    friend class SDStorageTestHelper;
    friend class Transaction;
    friend class TransactionManager;

};


#endif