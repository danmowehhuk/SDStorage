#ifndef _SDStorage_IndexScanFilters_h
#define _SDStorage_IndexScanFilters_h


#include "../Index.h"
#include "../hal/SDStorageHal.h"
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
        SDStorageHal::println(F("idxUpsert aborting - possible index corruption"));
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
      } else if (!state->didInsert &&                            // not inserted yet AND
              strcmp(state->newKey, currEntry.key) < 0 &&        // state->newKey is before key
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
        // newKey goes after this key, or is already in: keep the line
        dest->println(line);
        if (state->prevKey) free(state->prevKey);
        state->prevKey = nullptr;
        state->prevKey = strdup(currEntry.key);
      }
      return true;
    }

    // For passing a rewrite's filter and its state through idxDedupeFilter
    struct DedupeCapture {
      StreamableManager::FilterFunction filter;  // in
      void* filterState;                         // in
      char* prevKey;                             // the previous line's key, in a buffer that holds a line
      bool stopped = false;                      // out: filter stopped the rewrite
      DedupeCapture(StreamableManager::FilterFunction filter, void* filterState, char* prevKey):
          filter(filter), filterState(filterState), prevKey(prevKey) { prevKey[0] = '\0'; };
    };

    // Drops a line whose key repeats the previous line's, keeping the first,
    // and passes every other line to state->filter. Every rewrite of an index
    // goes through this, so it also removes duplicates an earlier write left.
    static bool idxDedupeFilter(const char* line, StreamableManager::DestinationStream* dest,
          void* statePtr) {
      DedupeCapture* state = static_cast<DedupeCapture*>(statePtr);
      const char* eq = strchr(line, '=');
      size_t keyLength = eq ? static_cast<size_t>(eq - line) : strlen(line);
      // The key's trailing spaces don't count, as in IndexHelpers::parseIndexEntry
      while (keyLength > 0 && isspace(line[keyLength - 1])) keyLength--;
      if (keyLength > 0 && strlen(state->prevKey) == keyLength
          && strncmp(line, state->prevKey, keyLength) == 0) {
        return true;
      }
      memcpy(state->prevKey, line, keyLength);
      state->prevKey[keyLength] = '\0';
      state->stopped = !state->filter(line, dest, state->filterState);
      return !state->stopped;
    }

    static bool idxLookupFilter(const char* line, StreamableManager::DestinationStream* /*dest*/, 
          void* statePtr) {
      IdxScanCapture* state = static_cast<IdxScanCapture*>(statePtr);
      char* l = strdup(line);
      IndexEntry currEntry = IndexHelpers::parseIndexEntry(l);
      free(l);
      int order = strcmp(state->key, currEntry.key);
      if (order == 0) {
        state->keyExists = true;
        state->value = strdup(currEntry.value);
      }
      // Indexes are sorted, so once a line's key sorts after state->key,
      // state->key can't be further on
      return order > 0;
    }

    // For comparing a key with an index line's
    struct KeyOrder {
      const char* key;  // in
      // out: strcmp(key, the line's key). With no line read it stays
      // negative, which only narrows a search's end: always safe.
      int order = -1;
      KeyOrder(const char* key): key(key) {};
    };

    // Compares the first line's key with state->key (a KeyOrder), then stops.
    static bool idxCompareKeyFilter(const char* line, StreamableManager::DestinationStream* /*dest*/,
          void* statePtr) {
      KeyOrder* state = static_cast<KeyOrder*>(statePtr);
      char* l = strdup(line);
      IndexEntry entry = IndexHelpers::parseIndexEntry(l);
      free(l);
      state->order = strcmp(state->key, entry.key);
      return false;
    }

    static bool idxPrefixSearchFilter(const char* line, StreamableManager::DestinationStream* /*dest*/, 
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
    friend class StorageProvider;

};


#endif
