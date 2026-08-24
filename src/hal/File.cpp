#ifdef NO_ARDUINO

#include "File.h"

File::File() {}

File::~File() {
  if (_open) close();
}

bool File::open(const char* path, BYTE mode) {
  FILINFO info;
  if (f_stat(path, &info) == FR_OK && (info.fattrib & AM_DIR)) {
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

int File::available() {
  if (!_open || _isDir) return 0;
  return f_tell(&_fil) < f_size(&_fil);
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
