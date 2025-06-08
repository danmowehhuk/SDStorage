#include "SDStorage.h"
#include "Index.h"

/*
 * Wire the SD card reader to the Arduino's hardware SPI and update
 * the SD_CS_PIN value below to whichever pin was used for the SD's
 * chip select
 */
#define SD_CS_PIN 53

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


void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!sdStorage.begin()) {
    Serial.println(F("SDStorage init failed"));
    return;
  }

  sdstorage::Index idx(F("my_idx1"));

  sdstorage::IndexEntry entry1(F("abc"), F("def"));
  sdstorage::IndexEntry entry2(F("123"), F("456"));

  // Add two entries to an index
  if (sdStorage.idxUpsert(idx, &entry1)
        && sdStorage.idxUpsert(idx, &entry2)) {
    Serial.println(F("Added 2 entries to index"));
  } else {
    Serial.println(F("Adding 2 entries to index FAILED"));
    return;
  }

  // Check that the index contains a key
  if (sdStorage.idxHasKey(idx, "123")) {
    Serial.println(F("Confirmed key '123' exists in index"));
  } else {
    Serial.println(F("Key '123' not found after inserting"));
    return;
  }

  // Look up a value from the index
  char valueBuffer[10];
  if (sdStorage.idxLookup(idx, "abc", valueBuffer, 10)) {
    Serial.print(F("Retrieved value '"));
    Serial.print(valueBuffer);
    Serial.println(F("' for key 'abc'"));
  } else {
    Serial.println("Key lookup FAILED");
    return;
  }

  // Remove a key from the index
  if (!sdStorage.idxRemove(idx, "123")) {
    Serial.println(F("Remove key '123' from index FAILED"));
    return;
  }
  if (sdStorage.idxHasKey(idx, "123")) {
    Serial.println(F("Index should not have contained remove key '123'"));
    return;
  }

  // Rename a key in the index
  if (!sdStorage.idxRename(idx, "abc", "ABC")) {
    Serial.println(F("Renaming key from 'abc' to 'ABC' FAILED"));
    return;
  }
  if (!sdStorage.idxHasKey(idx, "ABC")) {
    Serial.println(F("Key 'ABC' not found after renaming"));
    return;
  }
  if (sdStorage.idxHasKey(idx, "abc")) {
    Serial.println(F("Index should not have contained renamed key 'abc'"));
    return;
  }

  // Update the value for an existing key
  sdstorage::IndexEntry entry3(F("ABC"), F("DEF"));
  if (!sdStorage.idxUpsert(idx, &entry3)) {
    Serial.println("Updating entry 'ABC' to value 'DEF' FAILED");
    return;
  }
}

void loop() {
}
