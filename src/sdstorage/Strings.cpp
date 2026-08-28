#include "../hal/FlashStr.h"
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

  char* strdup(const FlashStr* progmemStr) {
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

  bool contains(const char* str, const FlashStr* needle) {
    if (!str || !needle) return false;
    char* needleRAM = strdup(needle);
    bool result = contains(str, needleRAM);
    free(needleRAM);
    return result;    
  }

  bool startsWith(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    while (*prefix) {
      if (*str++ != *prefix++) return false;
    }
    return true;
  }

  bool endsWith(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t strLen = strlen(str);
    size_t suffixLen = strlen(suffix);
    if (suffixLen > strLen) return false;
    return strcmp(str + (strLen - suffixLen), suffix) == 0;
  }

  bool endsWith(const char* str, const FlashStr* suffix) {
    if (!str || !suffix) return false;
    char* suffixRAM = strdup(suffix);
    bool result = endsWith(str, suffixRAM);
    free(suffixRAM);
    return result;
  }

  bool isEmpty(const char* str) {
    return (!str || str[0] == '\0');
  };

  bool isEmpty(const FlashStr* str) {
    return isEmpty_P(reinterpret_cast<const char*>(str));
  };

  bool isEmpty_P(const char* str) {
    return (!str || strlen_P(str) == 0);
  };

  bool uint64ToString(uint64_t value, char* output, size_t bufferSize) {
    if (!output || bufferSize < 2) return false;

    if (value == 0) {
      output[0] = '0';
      output[1] = '\0';
      return true;
    }

    // Count digits first so a too-small buffer is caught before writing
    uint64_t counter = value;
    int digits = 0;
    while (counter > 0) {
      digits++;
      counter /= 10;
    }
    if (bufferSize < (size_t)(digits + 1)) return false;

    char temp[20]; // max digits for uint64_t (no null terminator needed here)
    int index = 0;
    counter = value;
    while (counter > 0) {
      temp[index++] = '0' + (counter % 10);
      counter /= 10;
    }
    for (int i = 0; i < index; i++) {
      output[i] = temp[index - 1 - i];
    }
    output[index] = '\0';
    return true;
  }

  uint64_t stringToUint64(const char* input) {
    if (!input) return 0;
    uint64_t result = 0;
    while (*input) {
      if (*input >= '0' && *input <= '9') {
        result = result * 10 + (*input - '0');
      } else {
        break;
      }
      input++;
    }
    return result;
  }

}