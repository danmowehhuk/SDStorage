#ifndef _SDStorage_IndexScanFilters_h
#define _SDStorage_IndexScanFilters_h


#include <StreamableManager.h>

class IndexScanFilters {

  public:
    IndexScanFilters() = delete;

  private:

    // For passing data in/out of index-related lambda functions
    struct IdxScanCapture {
      const char* key;           // in
      const char* newKey;        // in
      const char* valueIn;       // in
      const bool isUpsert;       // in
      char* value = nullptr;     // out
      char* prevKey = nullptr;   // out
      bool keyExists = false;    // out
      bool didUpsert = false;    // out
      bool didRemove = false;    // out
      bool didInsert = false;    // out
      IdxScanCapture(const char* key): 
          key(key), newKey(nullptr), valueIn(nullptr), isUpsert(false) {};
      IdxScanCapture(const char* key, const char* value): 
          key(key), newKey(nullptr), valueIn(value), isUpsert(true) {};
      IdxScanCapture(const char* oldKey, const char* newKey, bool):
          key(oldKey), newKey(newKey), valueIn(nullptr), isUpsert(false) {};
    };

    static bool idxUpsertFilter(const char* line, StreamableManager::DestinationStream* dest, 
          void* statePtr) {
      IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
      if (state->didUpsert) {
        // Upsert already happened. Pipe the rest in fast mode.
        return _pipeFast(line, dest);
      }
      char* key = parseIndexKey(line);
      if (strcmp(state->key, key) == 0) {
        // Found matching key - update the value
        char* updatedLine = toIndexLine(key, state->valueIn);
        dest->println(updatedLine);
        delete[] updatedLine;
        state->didUpsert = true;
      } else if (strcmp(state->key, key) < 0 &&                               // state->key is before key
              (!state->prevKey                                                // this is the first key OR
               || strcmp(state->key, state->prevKey) > 0)) {                  // state->key is after prevKey
          // insert new key/value before line
          char* newLine = toIndexLine(state->key, state->valueIn);
          dest->println(newLine);
          dest->println(line);
          delete[] newLine;
          state->didUpsert = true;
      } else {
        // state-key comes after this key, so keep going
        dest->println(line);
        state->prevKey = key;
      }
      delete[] key;
      return true;
    };

    // dynamic char* must be delete[]'ed
    static char* toIndexLine(const char* key, const char* value) {
      size_t len = strlen(key) + strlen(value) + 2;
      char* line = new char[len]();
      static const char fmt[] PROGMEM = "%s=%s";
      snprintf_P(line, len, fmt, key, value);
      return line;
    };

    static char* parseIndexKey(const char* line) {
      if (!line) return nullptr;

      size_t len = strcspn(line, "=");
      while (len > 0 && isspace(line[len - 1])) len--;  // trim trailing
      const char* start = line;
      while (isspace(*start) && start < line + len) start++;  // trim leading

      size_t trimmedLen = line + len - start;
      char* key = new char[trimmedLen + 1];
      strncpy(key, start, trimmedLen);
      key[trimmedLen] = '\0';
      return key;
    };

    static char* parseIndexValue(const char* line) {
      if (!line) return nullptr;

      const char* sep = strchr(line, '=');
      if (!sep) return nullptr;
      const char* start = sep + 1;
      while (isspace(*start)) start++;  // trim leading whitespace
      const char* end = start + strlen(start) - 1;
      while (end > start && isspace(*end)) end--;  // trim trailing

      size_t trimmedLen = end - start + 1;
      char* value = new char[trimmedLen + 1];
      strncpy(value, start, trimmedLen);
      value[trimmedLen] = '\0';
      return value;
    };

    static bool _pipeFast(const char* line, StreamableManager::DestinationStream* dest) {
      dest->println(line);
      return true;
    };

    friend class IndexManager;

};


#endif
