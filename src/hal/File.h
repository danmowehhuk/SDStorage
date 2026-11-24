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

    // Moves to a byte offset in the file, as SdFat's File::seek() does.
    // Params:
    //   position - the offset from the start of the file
    // Returns true if it moved, false if the file isn't open or position
    // is past its end.
    bool seek(uint32_t position);

    uint32_t position();
    uint32_t size();

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
