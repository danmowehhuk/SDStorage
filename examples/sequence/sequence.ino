#include "SDStorage.h"
#include "Sequence.h"

/*
 * Wire the SD card reader to the Arduino's hardware SPI and update
 * the SD_CS_PIN value below to whichever pin was used for the SD's
 * chip select
 */
#ifndef OVERRIDE_PINS
  #ifndef SD_CS_PIN
  #define SD_CS_PIN 53
  #endif
#endif

/*
 * Root directory where SDStorage will put all the files
 */
static const char SD_ROOT[] PROGMEM = "EXAMPLE";

/*
 * Pass this function to the SDStorage constructor. SDStorage will call
 * it if an error occurs that could allow your file system to be damaged.
 * Best practice is for this function to print an error and quickly halt
 * the system by entering a perpetual loop - while(true) {}
 */
void errFunction() {
  Serial.println(F("Something very bad just happened. Halting to protect the SD card."));
  while (true) {}
}

// chip select pin, file system root dir, root dir string is pmem, on error function
SDStorage sdStorage(SD_CS_PIN, SD_ROOT, true, errFunction);

// A simple stringify function matching the SeqToString shape:
// bool (*)(uint64_t value, char* out)
// NOTE: avr-libc's sprintf has no 64-bit conversion, so this truncates to
// 32 bits - fine for this demo, not for a sequence expected to run past
// ~4 billion. See SDStorageStrings::uint64ToString for the full range.
bool toDecimal(uint64_t value, char* out) {
  return sprintf(out, "%lu", (unsigned long)value) > 0;
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!sdStorage.begin()) {
    Serial.println(F("SDStorage init failed"));
    return;
  }

  sdstorage::Sequence mySeq(F("my_seq"));

  // A sequence must be advanced with seqNext() (or given an explicit
  // starting point with seqInit()) at least once before seqCurrent() can
  // be called on it - reading it first is a usage error and calls
  // errFunction. seqNext() itself has no such requirement: calling it on
  // a brand-new sequence is exactly how a sequence gets created.
  uint64_t id1 = sdStorage.seqNext(mySeq);
  uint64_t id2 = sdStorage.seqNext(mySeq);
  Serial.print(F("First two IDs: "));
  Serial.print((unsigned long)id1);
  Serial.print(F(", "));
  Serial.println((unsigned long)id2);

  Serial.print(F("Current value: "));
  Serial.println((unsigned long)sdStorage.seqCurrent(mySeq)); // valid now that seqNext() has run

  char idStr[21];
  if (sdStorage.seqNext(mySeq, idStr, toDecimal)) {
    Serial.print(F("Third ID as a string: "));
    Serial.println(idStr);
  }

  // seqInit() lets a sequence start somewhere other than 1 - useful when
  // importing existing data. initialValue must be >= 1.
  sdstorage::Sequence importedSeq(F("imported"));
  if (sdStorage.seqInit(importedSeq, 1000)) {
    Serial.print(F("Imported sequence starts at: "));
    Serial.println((unsigned long)sdStorage.seqCurrent(importedSeq));
  }
}

void loop() {
}
