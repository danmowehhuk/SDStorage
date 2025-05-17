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
      ~IdxScanCapture() {
        if (prevKey) free(prevKey);
        if (value) free(value);
        prevKey = nullptr;
        value = nullptr;
      }
    };

    static bool idxUpsertFilter(const char* line, StreamableManager::DestinationStream* dest, 
          void* statePtr) {

      IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
      if (state->didUpsert) {
        // Upsert already happened. Pipe the rest in fast mode.
        return _pipeFast(line, dest);
      }

      char* l = strdup(line);
      IndexEntry currEntry = IndexHelpers::parseIndexEntry(l);
      free(l);

      if (isEmpty(currEntry.key)) {
#if (defined(DEBUG))
        Serial.println(F("idxUpsert aborting - possible index corruption"));
#endif
        state->didUpsert = false; // signal txn rollback
        return false; // stop piping index lines
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
        if (state->prevKey) free(state->prevKey);
        state->prevKey = nullptr;
        state->prevKey = strdup(currEntry.key);
      }
      return true;
    };

    static bool idxRemoveFilter(const char* line, StreamableManager::DestinationStream* dest, 
          void* statePtr) {
      IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
      if (state->didRemove) {
        // Remove already happened. Pipe the rest in fast mode.
        return _pipeFast(line, dest);
      }
      char* l = strdup(line);
      IndexEntry currEntry = IndexHelpers::parseIndexEntry(l);
      free(l);
      if (strcmp(state->key, currEntry.key) == 0) {
        /* skip it */ 
        state->didRemove = true;
      } else { dest->println(line); }
      return true;
    }

    static bool idxRenameFilter(const char* line, StreamableManager::DestinationStream* dest, 
          void* statePtr) {
      IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
      if (state->didRemove && state->didInsert) {
        // Rename already happened. Pipe the rest in fast mode.
        return _pipeFast(line, dest);
      }
      char* l = strdup(line);
      IndexEntry currEntry = IndexHelpers::parseIndexEntry(l);
      free(l);

      if (strcmp(state->key, currEntry.key) == 0) {
        /* skip the old key */
        state->didRemove = true;
      } else if (strcmp(state->newKey, currEntry.key) == 0) {
        // new key already exists - abort
        return false;
      } else if (strcmp(state->newKey, currEntry.key) < 0 &&     // state->newKey is before key
              (!state->prevKey                                   // this is the first key OR
               || strcmp(state->newKey, state->prevKey) > 0)) {  // state->newKey is after prevKey
          // insert new newKey/value before line
          IndexEntry newEntry(state->newKey, state->value);
          char newLine[state->bufferSize];
          IndexHelpers::toIndexLine(&newEntry, newLine, state->bufferSize);
          dest->println(newLine);
          dest->println(line);
          state->didInsert = true;
      } else {
        // state-key comes after this key, so keep going
        dest->println(line);
        if (state->prevKey) free(state->prevKey);
        state->prevKey = nullptr;
        state->prevKey = strdup(currEntry.key);
      }
      return true;
    }

    static bool idxLookupFilter(const char* line, StreamableManager::DestinationStream* dest, 
          void* statePtr) {
      IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
      char* l = strdup(line);
      IndexEntry currEntry = IndexHelpers::parseIndexEntry(l);
      free(l);
      if (strcmp(state->key, currEntry.key) == 0) {
        state->keyExists = true;
        state->value = strdup(currEntry.value);
        return false; // stop scanning
      }
      return true; // keep going
    }

    static bool idxPrefixSearchFilter(const char* line, StreamableManager::DestinationStream* dest, 
          void* statePtr) {
      SearchResults* results = static_cast<SearchResults*>(statePtr);
      char* l = strdup(line);
      IndexEntry currEntry = IndexHelpers::parseIndexEntry(l);
      free(l);

      char* prefix = results->searchPrefix;
      if (strlen(prefix) > 0 && !startsWith(currEntry.key, prefix) && strcmp(currEntry.key, prefix) > 0) {
        // Indexes are sorted, so if key comes after prefix, there's no
        // point continuing to scan the index
        return false;
      }
      if (strlen(prefix) == 0 || startsWith(currEntry.key, prefix)) {
        // Handle up to 10 matches, then switch to trie mode
        if (results->matchCount < 10) {
          KeyValue* match = new KeyValue(currEntry.key, currEntry.value);
          appendMatchResult(results, match);
          results->matchCount++;
        } else {
          results->trieMode = true;
        }

        // Populate the trieResult and bloom filter
        uint8_t pLen = strlen(prefix);
        uint8_t pos = pLen;
        if (strlen(currEntry.key) > pLen) {
          char c = currEntry.key[pos];
          if (c >= 32 && c <= 122) { // Ensure it's an acceptable ASCII character
            uint8_t index = c - 32;  // Map ASCII to 0-90
            uint8_t wordIndex = index / 32; // Determine which 32-bit word to use
            uint8_t bitIndex = index % 32; // Determine the bit within the word
            if ((results->trieBloom[wordIndex] & (1UL << bitIndex)) == 0) { // Check if this char is new
              results->trieBloom[wordIndex] |= (1UL << bitIndex);  // Mark this char as seen
              // Add it to the trieResult
              KeyValue* kv;
              char cmpStr[pLen+2];
              strncpy(cmpStr, prefix, pLen);
              cmpStr[pLen] = c;
              cmpStr[pLen+1] = '\0';
              char key[2] = { c, '\0' };
              if (strcmp(currEntry.key, cmpStr) == 0) {
                kv = new KeyValue(key, currEntry.value);
              } else {
                kv = new KeyValue(key, "");
              }
              appendTrieResult(results, kv);
            }
          }
        }
      }
      return true;
    }

    static void _appendKeyValue(KeyValue* head, KeyValue* result) {
      while (head->next != nullptr) head = head->next;
      head->next = result;
    }

    static void appendMatchResult(SearchResults* sr, KeyValue* result) {
      if (sr->matchResult == nullptr) {
        sr->matchResult = result;
      } else {
        _appendKeyValue(sr->matchResult, result);
      }
    }

    static void appendTrieResult(SearchResults* sr, KeyValue* result) {
      if (sr->trieResult == nullptr) {
        sr->trieResult = result;
      } else {
        _appendKeyValue(sr->trieResult, result);
      }
    }


    static bool _pipeFast(const char* line, StreamableManager::DestinationStream* dest) {
      dest->println(line);
      return true;
    };

    friend class IndexManager;

};


#endif
