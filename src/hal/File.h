#ifndef SDSTORAGE_HAL_FILE_H
#define SDSTORAGE_HAL_FILE_H

#ifdef NO_ARDUINO

#include <StreamableManager.h>
#include "avr/fatfs/ff.h"

// Wraps a FatFs file handle, implementing StreamableDTO's Stream
// interface so StreamableManager's send()/load() work against a real
// SD-card file exactly as they do against a StringStream.
class File : public Stream {
  public:
    File();
    ~File();

    bool open(const char* path, BYTE mode);
    void close();
    bool isOpen() const;
    bool isDirectory() const;

    int available() override;
    int read() override;
    size_t write(uint8_t byte) override;
    int availableForWrite() override;

  private:
    FIL _fil;
    bool _open = false;
    bool _isDir = false;
};

#endif
#endif
