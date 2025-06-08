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

  // Populate an index
  sdstorage::Index idx(F("my_idx2"));
  sdstorage::IndexEntry entry1(F("are"), F("1"));
  sdstorage::IndexEntry entry2(F("ear"), F("3"));
  sdstorage::IndexEntry entry3(F("east"), F("23"));
  sdstorage::IndexEntry entry4(F("ed"), F("209"));
  sdstorage::IndexEntry entry5(F("egg"), F("45"));
  sdstorage::IndexEntry entry6(F("ent"), F("65"));
  sdstorage::IndexEntry entry7(F("era"), F("12"));
  sdstorage::IndexEntry entry8(F("erf"), F("20"));
  sdstorage::IndexEntry entry9(F("eta"), F("2"));
  sdstorage::IndexEntry entry10(F("etre"), F("98"));
  sdstorage::IndexEntry entry11(F("eva"), F("4"));
  sdstorage::IndexEntry entry12(F("exit"), F("4"));
  sdstorage::IndexEntry entry13(F("fan"), F("1"));
  sdstorage::IndexEntry entry14(F("glob"));
  sdStorage.idxUpsert(idx, &entry1);
  sdStorage.idxUpsert(idx, &entry2);
  sdStorage.idxUpsert(idx, &entry3);
  sdStorage.idxUpsert(idx, &entry4);
  sdStorage.idxUpsert(idx, &entry5);
  sdStorage.idxUpsert(idx, &entry6);
  sdStorage.idxUpsert(idx, &entry7);
  sdStorage.idxUpsert(idx, &entry8);
  sdStorage.idxUpsert(idx, &entry9);
  sdStorage.idxUpsert(idx, &entry10);
  sdStorage.idxUpsert(idx, &entry11);
  sdStorage.idxUpsert(idx, &entry12);
  sdStorage.idxUpsert(idx, &entry13);
  sdStorage.idxUpsert(idx, &entry14);

  // If the prefix search returns 10 or fewer results, trieMode is false and the full set
  // of matching keys and their values is returned
  char* searchPrefix1 = "et";
  sdstorage::SearchResults searchResult1(searchPrefix1);
  if (sdStorage.idxPrefixSearch(idx, &searchResult1)) {
    Serial.print(F("Searching for 'et' returned "));
    Serial.print(searchResult1.matchCount);
    Serial.println(" match(es):");
    sdstorage::KeyValue* kv = &searchResult1.matchResult[0];
    while (kv) {
      Serial.print(F("  "));
      Serial.print(kv->key);
      Serial.print(": ");
      if (kv->value) {
        Serial.println(kv->value);
      } else {
        Serial.println();
      }
      kv = kv->next;
    }
  } else {
    Serial.println(F("Prefix search for 'et' FAILED"));
  }

  // If the prefix search returns more than 10 results, trieMode is true and the result keys
  // only contain the next possible characters. If one of those characters happens to complete
  // a key, then its value is also populated
  char* searchPrefix2 = "e";
  sdstorage::SearchResults searchResult2(searchPrefix2);
  if (sdStorage.idxPrefixSearch(idx, &searchResult2)) {
    Serial.print(F("Searching for 'e' returned "));
    Serial.print(searchResult2.matchCount);
    Serial.println(" potential next characters:");
    sdstorage::KeyValue* kv = &searchResult2.matchResult[0];
    while (kv) {
      Serial.print(F("  "));
      Serial.print(kv->key);
      Serial.print(": ");
      if (kv->value) {
        Serial.println(kv->value);
      } else {
        Serial.println();
      }
      kv = kv->next;
    }
  } else {
    Serial.println(F("Prefix search for 'e' FAILED"));
  }

}

void loop() {
}
