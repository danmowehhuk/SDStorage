#include "Index.h"
#include "SDStorage.h"
#include <StreamableDTO.h>

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

  sdstorage::Index idx(F("my_idx3"));
  sdstorage::IndexEntry entry(F("abc"), F("def"));

  StreamableDTO newObj;
  newObj.put("abc", "def");
  newObj.put("123", "456");
  char* filename = "myfile3.dat";

  // Add the index and the file to a transaction (can have any number of indexes and files)
  // Currently, only one change per index or file is supported in a transaction.
  Transaction* txn = sdStorage.beginTxn(idx, filename);

  // upsert the index
  if (!sdStorage.idxUpsert(idx, &entry)) {
    Serial.println(F("Index upsert FAILED"));
    return;
  }

  // save the StreamableDTO
  if (!sdStorage.save(filename, &newObj)) {
    Serial.println(F("StreamableDTO save FAILED"));
    return;
  }

  // Notice that filename doesn't exist yet
  if (sdStorage.exists(filename)) {
    Serial.println(F("Wrote file before commit"));
    return;
  }

  // Commit the transaction
  if (!sdStorage.commitTxn(txn)) {
    Serial.println(F("Commit transaction FAILED"));
    return;
  }

  // Now the file exists and the index has been updated
  if (!sdStorage.exists(filename)) {
    Serial.println(F("File should have been written after commit"));
    return;
  }

}

void loop() {
}
