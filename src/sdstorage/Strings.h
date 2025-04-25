#ifndef _SDStorage_Strings_h
#define _SDStorage_Strings_h


namespace SDStorageStrings {

  char* strdup_P(const char* progmemStr);
  char* strdup(const __FlashStringHelper* progmemStr);
  char* trim(const char* str);
  bool contains(const char* str, const char* needle);
  bool contains(const char* str, const __FlashStringHelper* needle);

}


#endif
