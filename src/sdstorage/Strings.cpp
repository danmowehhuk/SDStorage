#include <Arduino.h>
#include "Strings.h"

namespace SDStorageStrings {

  char* strdup_P(const char* progmemStr) {
    if (!progmemStr) return nullptr;
    size_t len = strlen_P(progmemStr);

    // use malloc so cleanup uses free, not delete, just like strdup
    char* ramStr = (char*)malloc(len + 1);
    if (ramStr) {
      strncpy_P(ramStr, progmemStr, len);
      ramStr[len] = '\0';
    }
    return ramStr;
  }

  char* strdup(const __FlashStringHelper* progmemStr) {
    return strdup_P(reinterpret_cast<const char*>(progmemStr));
  }

  char* trim(const char* str) {
    char* s = const_cast<char*>(str);
    while (isspace(*s)) s++;
    for (char* end = s + strlen(s) - 1; end >= s && isspace(*end); --end) {
      *end = '\0';
    }
    return s;
  }

  bool contains(const char* str, const char* needle) {
    if (!str || !needle) return false;
    return strstr(str, needle) != nullptr;
  }

  bool contains(const char* str, const __FlashStringHelper* needle) {
    if (!str || !needle) return false;
    char* needleRAM = strdup(needle);
    bool result = contains(str, needleRAM);
    free(needleRAM);
    return result;    
  }

  bool endsWith(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t strLen = strlen(str);
    size_t suffixLen = strlen(suffix);
    if (suffixLen > strLen) return false;
    return strcmp(str + (strLen - suffixLen), suffix) == 0;
  }

  bool endsWith(const char* str, const __FlashStringHelper* suffix) {
    if (!str || !suffix) return false;
    char* suffixRAM = strdup(suffix);
    bool result = endsWith(str, suffixRAM);
    free(suffixRAM);
    return result;
  }

  bool isEmpty(const char* str) {
    return (!str || str[0] == '\0');
  };

}