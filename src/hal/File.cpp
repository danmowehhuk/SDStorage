#ifdef NO_ARDUINO

#include "File.h"
#include <limits.h>

File::File() {}

File::~File() {
  if (_open) close();
}

bool File::open(const char* path, BYTE mode) {
  // Any write/create mode against a directory path is not a valid
  // open - only take the directory shortcut for a read-only open,
  // matching the SdFat/MockSdFat branches' own genuine failure here.
  static const BYTE writeModeMask = FA_WRITE | FA_CREATE_NEW | FA_CREATE_ALWAYS | FA_OPEN_ALWAYS;
  FILINFO info;
  if (f_stat(path, &info) == FR_OK && (info.fattrib & AM_DIR)) {
    if (mode & writeModeMask) return false;
    _isDir = true;
    _open = true;
    return true;
  }
  FRESULT res = f_open(&_fil, path, mode);
  _open = (res == FR_OK);
  return _open;
}

void File::close() {
  if (_open && !_isDir) f_close(&_fil);
  _open = false;
  _isDir = false;
}

bool File::isOpen() const { return _open; }
bool File::isDirectory() const { return _isDir; }

bool File::seek(uint32_t position) {
  // f_lseek() extends a file opened for writing when position is past
  // its end; a read never needs that.
  return _open && !_isDir && position <= f_size(&_fil) && f_lseek(&_fil, position) == FR_OK;
}

uint32_t File::position() {
  return _open && !_isDir ? f_tell(&_fil) : 0;
}

uint32_t File::size() {
  return _open && !_isDir ? f_size(&_fil) : 0;
}

int File::available() {
  if (!_open || _isDir) return 0;
  // Clamped, as SdFat's is: on AVR an int wraps past 32 KB left
  FSIZE_t remaining = f_size(&_fil) - f_tell(&_fil);
  return remaining > static_cast<FSIZE_t>(INT_MAX) ? INT_MAX : static_cast<int>(remaining);
}

int File::read() {
  if (!_open || _isDir) return -1;
  uint8_t b;
  UINT actuallyRead;
  if (f_read(&_fil, &b, 1, &actuallyRead) != FR_OK || actuallyRead == 0) return -1;
  return b;
}

size_t File::write(uint8_t byte) {
  if (!_open || _isDir) return 0;
  UINT written;
  if (f_write(&_fil, &byte, 1, &written) != FR_OK) return 0;
  return written;
}

int File::availableForWrite() {
  return _open && !_isDir ? 1 : 0;
}

#endif
