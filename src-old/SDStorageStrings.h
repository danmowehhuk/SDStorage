#ifndef _SDStorage_SDStorageStrings_h
#define _SDStorage_SDStorageStrings_h


namespace SDStorageStrings {

  bool contains(const char* str, const char* needle);
  bool contains(const char* str, const __FlashStringHelper* needle);
  bool endsWith(const char* str, const char* suffix);
  bool endsWith(const char* str, const __FlashStringHelper* suffix);
  char* strdup_P(const char* progmemStr);
  char* strdup(const __FlashStringHelper* progmemStr);
  char* trim(const char* str);

}


#endif
