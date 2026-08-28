/*

  Index.h - Part of SDStorage

  SD card storage manager for StreamableDTOs with index and transaction support

  Copyright (c) 2025, Dan Mowehhuk (danmowehhuk@gmail.com)
  All rights reserved.

*/

#ifndef _SDStorage_Index_h
#define _SDStorage_Index_h


#include "hal/FlashStr.h"
#include "sdstorage/Strings.h"

using namespace SDStorageStrings;

namespace sdstorage {

  class Index {

    public:
      const char* name;
      const bool isPmem;

      explicit Index(const char* n, bool isPmem = false): 
          name(n), isPmem(isPmem) {};
      explicit Index(const FlashStr* n):
          name(reinterpret_cast<const char*>(n)), isPmem(true) {};

      static Index fromProgmem(const char* n) {
          return Index(n, true);
      };

  };

  struct IndexEntry {
    const char* key;
    const char* value;
    IndexEntry(const char* k): key(strdup(k)), value(nullptr) {};
    IndexEntry(const FlashStr* k): key(strdup(k)), value(nullptr) {};
    IndexEntry(const char* k, const char* v):
        key(strdup(k)), value(strdup(v)) {};
    IndexEntry(const char* k, const FlashStr* v):
        key(strdup(k)), value(strdup(v)) {};
    IndexEntry(const FlashStr* k, const char* v):
        key(strdup(k)), value(strdup(v)) {};
    IndexEntry(const FlashStr* k, const FlashStr* v):
        key(strdup(k)), value(strdup(v)) {};

    IndexEntry(const IndexEntry& other)
      : key(other.key ? strdup(other.key) : nullptr),
        value(other.value ? strdup(other.value) : nullptr) {}

    IndexEntry& operator=(const IndexEntry& other) {
      if (this != &other) {
        if (key) free(key);
        if (value) free(value);
        key = other.key ? strdup(other.key) : nullptr;
        value = other.value ? strdup(other.value) : nullptr;
      }
      return *this;
    }

    ~IndexEntry() {
      if (key) free(key);
      if (value) free(value);
    };
  };

  struct KeyValue {
    char* key;
    char* value;
    KeyValue* next = nullptr;
    KeyValue(const char* key, const char* value): key(strdup(key)), value(strdup(value)) {};
    ~KeyValue() {
      if (key) free(key);
      if (value) free(value);
      key = nullptr;
      value = nullptr;
      KeyValue* current = this->next;
      while (current != nullptr) {
        KeyValue* toDelete = current;
        current = current->next;
        toDelete->next = nullptr;
        delete toDelete;
      }
    }
  };

  struct SearchResults {
    char* searchPrefix;
    bool trieMode = false;
    uint32_t trieBloom[3] = {0};
    uint8_t matchCount = 0;
    KeyValue* matchResult = nullptr;
    KeyValue* trieResult = nullptr;
    SearchResults(const char* searchPrefix): searchPrefix(strdup(searchPrefix)) {};
    ~SearchResults() {
      if (searchPrefix) free(searchPrefix);
      searchPrefix = nullptr;
      if (matchResult) {
        delete matchResult;
      }
      if (trieResult) {
        delete trieResult;
      }
    }
  };

};


#endif