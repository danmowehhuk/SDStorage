#ifndef _SDStorage_IndexHelpers_h
#define _SDStorage_IndexHelpers_h


#include "../Index.h"
#include "Strings.h"

using namespace sdstorage;
using namespace SDStorageStrings;

class IndexHelpers {

  public:
    IndexHelpers() = delete;

  private:
    static bool toIndexLine(IndexEntry* entry, char* buffer, size_t bufferSize) {
      if (!entry || !entry->key || isEmpty(entry->key) || !buffer || bufferSize == 0) return false;

      static const char fmt[] PROGMEM = "%s=%s";
      const char* val = entry->value ? entry->value : "";
      int n = snprintf_P(buffer, bufferSize, fmt, entry->key, val);
      if (n < 0 || static_cast<size_t>(n) >= bufferSize) {
        // Truncated or error
        buffer[bufferSize - 1] = '\0';
        return false;
      }
      return true;
    };

    static IndexEntry parseIndexEntry(const char* line) {
      if (!line || !*line) return IndexEntry("");

      // Trim leading and trailing spaces in-place
      char* start = line;
      while (isspace(*start)) ++start;

      char* end = start + strlen(start) - 1;
      while (end > start && isspace(*end)) *end-- = '\0';

      char* eq = strchr(start, '=');
      if (!eq) {
        return IndexEntry(start);  // strdup’d inside constructor
      }

      *eq = '\0';
      char* key = start;
      char* value = eq + 1;

      while (isspace(*key)) ++key;
      char* keyEnd = key + strlen(key) - 1;
      while (keyEnd > key && isspace(*keyEnd)) *keyEnd-- = '\0';

      while (isspace(*value)) ++value;
      char* valEnd = value + strlen(value) - 1;
      while (valEnd > value && isspace(*valEnd)) *valEnd-- = '\0';

      return *value ? IndexEntry(key, value) : IndexEntry(key);
    };

    friend class IndexManager;
    friend class IndexScanFilters;
    friend class SDStorageTestHelper;

};


#endif
