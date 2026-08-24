#ifndef _SDStorage_Strings_h
#define _SDStorage_Strings_h

#include "../hal/FlashStr.h"

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

}


#endif
