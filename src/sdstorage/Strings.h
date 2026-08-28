#ifndef _SDStorage_Strings_h
#define _SDStorage_Strings_h

#include "../hal/FlashStr.h"
#include <stdint.h>

namespace SDStorageStrings {

  char* strdup_P(const char* progmemStr);
  char* strdup(const FlashStr* progmemStr);
  char* trim(const char* str);
  bool contains(const char* str, const char* needle);
  bool contains(const char* str, const FlashStr* needle);
  bool startsWith(const char* str, const char* prefix);
  bool endsWith(const char* str, const char* suffix);
  bool endsWith(const char* str, const FlashStr* suffix);
  bool isEmpty(const char* str);
  bool isEmpty(const FlashStr* str);
  bool isEmpty_P(const char* str);

  /*
   * Converts a uint64_t to a decimal string representation.
   *
   * @param value The 64-bit number to convert
   * @param output Buffer of at least 21 bytes (20 digits + null terminator)
   * @param bufferSize Size of the output buffer
   * @return true if conversion succeeded, false if output is null or
   *         bufferSize is too small for the value
   */
  bool uint64ToString(uint64_t value, char* output, size_t bufferSize);

  /*
   * Converts a decimal string to a uint64_t. Stops parsing at the first
   * non-digit character. Returns 0 for a null or invalid input.
   */
  uint64_t stringToUint64(const char* input);

}


#endif
