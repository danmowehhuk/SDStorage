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

      // Copy and trim line
      size_t len = strlen(line);
      while (len && isspace(line[0])) { line++; len--; }
      while (len && isspace(line[len - 1])) len--;
      char* trimmed = new char[len + 1];
      strncpy(trimmed, line, len);
      trimmed[len] = '\0';

      // Split on '='
      char* eq = strchr(trimmed, '=');
      if (!eq) {
        IndexEntry entry(trimmed);
        delete[] trimmed;
        return entry;
      }

      *eq = '\0';
      char* key = trimmed;
      char* val = eq + 1;

      // Trim key
      while (isspace(*key)) key++;
      char* keyEnd = key + strlen(key) - 1;
      while (keyEnd > key && isspace(*keyEnd)) *keyEnd-- = '\0';

      // Trim value
      while (isspace(*val)) val++;
      char* valEnd = val + strlen(val) - 1;
      while (valEnd > val && isspace(*valEnd)) *valEnd-- = '\0';

      IndexEntry entry = (*val ? IndexEntry(key, val) : IndexEntry(key));
      delete[] trimmed;
      return entry;
    }

    friend class IndexManager;
    friend class IndexScanFilters;
    friend class SDStorageTestHelper;

};


#endif
