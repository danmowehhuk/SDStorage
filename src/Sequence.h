/*

  Sequence.h - Part of SDStorage

  SD card storage manager for StreamableDTOs with index and transaction support

  Copyright (c) 2025, Dan Mowehhuk (danmowehhuk@gmail.com)
  All rights reserved.

*/

#ifndef _SDStorage_Sequence_h
#define _SDStorage_Sequence_h


#include "hal/FlashStr.h"
#include <stdint.h>

namespace sdstorage {

  /*
   * Converts a Sequence's current uint64_t value into a caller-supplied
   * buffer, returning true on success. The buffer must be sized by the
   * caller to whatever the specific conversion function requires (e.g.
   * RCEntities' idToFAT16() needs at least 9 bytes).
   */
  typedef bool (*SeqToString)(uint64_t value, char* out);

  class Sequence {

    public:
      const char* name;
      const bool isPmem;

      explicit Sequence(const char* n, bool isPmem = false):
          name(n), isPmem(isPmem) {};
      explicit Sequence(const FlashStr* n):
          name(reinterpret_cast<const char*>(n)), isPmem(true) {};

      static Sequence fromProgmem(const char* n) {
          return Sequence(n, true);
      };

  };

};


#endif
