#ifndef _SDStorage_IndexScanFilters_h
#define _SDStorage_IndexScanFilters_h


#include "../Index.h"
#include "IndexHelpers.h"
#include <StreamableManager.h>
#include "Strings.h"


using namespace SDStorageStrings;

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
      size_t bufferSize = 64;    // in
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

      IndexEntry currEntry = IndexHelpers::parseIndexEntry(line);
      if (isEmpty(currEntry.key)) {
#if (defined(DEBUG))
        Serial.println(F("idxUpsert aborting - possible index corruption"));
#endif
        return false;
      }

      if (strcmp(state->key, currEntry.key) == 0) {

        // Found matching key - update the value
        IndexEntry newEntry(state->key, state->valueIn);
        char newLine[state->bufferSize];
        IndexHelpers::toIndexLine(&newEntry, newLine, state->bufferSize);
        dest->println(newLine);
        state->didUpsert = true;

      } else if (strcmp(state->key, currEntry.key) < 0 &&       // state->key is before key
              (!state->prevKey                                  // this is the first key OR
               || strcmp(state->key, state->prevKey) > 0)) {    // state->key is after prevKey

          // insert new entry before current
          IndexEntry newEntry(state->key, state->valueIn);
          char newLine[state->bufferSize];
          IndexHelpers::toIndexLine(&newEntry, newLine, state->bufferSize);
          dest->println(newLine);
          dest->println(line);
          state->didUpsert = true;

      } else {
        // state-key comes after this key, so keep going
        dest->println(line);
        state->prevKey = state->key;
      }
      return true;
    };

    static bool _pipeFast(const char* line, StreamableManager::DestinationStream* dest) {
      dest->println(line);
      return true;
    };

    friend class IndexManager;

};


#endif
