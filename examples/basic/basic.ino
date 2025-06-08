#include "SDStorage.h"
#include <StreamableDTO.h>
#include <StreamableManager.h>


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

  char* filename = "myfile1.dat";

  // Create a new file of simple key-value pairs
  StreamableDTO newObj;
  newObj.put("abc", "def");
  newObj.put("123", "456");
  if (sdStorage.save(filename, &newObj)) {
    Serial.println(F("Saved newObj"));
  } else {
    Serial.println(F("Save newObj FAILED"));
    return;
  }
  delete &newObj; // clear it from memory

  // Confirm that the file has been created
  if (sdStorage.exists(filename)) {
    Serial.print(F("Confirmed "));
    Serial.print(filename);
    Serial.println(F(" exists"));
  } else {
    Serial.println(F("Existance check FAILED"));
    return;
  }

  // For printing StreamableDTO contents to Serial
  StreamableManager stream;

  // Load the saved file back into memory
  StreamableDTO savedObj;
  if (sdStorage.load(filename, &savedObj)) {
    Serial.print(F("Reloaded "));
    Serial.print(filename);
    Serial.println(F(" into memory:"));
    // Print the key-value pairs to Serial
    stream.send(&Serial, &savedObj);
  } else {
    Serial.println(F("Load savedObj FAILED"));
    return;
  }

  // Update one of the keys and save
  savedObj.put("abc", "ghi");
  if (sdStorage.save(filename, &savedObj)) {
    Serial.println(F("Saved updated savedObj"));
  } else {
    Serial.println(F("Save updated savedObj FAILED"));
    return;
  }
  delete &savedObj; // clear it from memory

  // Load the saved file back into memory
  StreamableDTO savedObj2;
  if (sdStorage.load(filename, &savedObj2)) {
    Serial.print(F("Reloaded "));
    Serial.print(filename);
    Serial.println(F(" into memory:"));
    // Print the key-value pairs to Serial
    stream.send(&Serial, &savedObj2);
  } else {
    Serial.println(F("Load savedObj2 FAILED"));
    return;
  }
  delete &savedObj2;

  // Delete the file
  if (sdStorage.erase(filename)) {
    Serial.print(F("Erased "));
    Serial.println(filename);
  } else {
    Serial.print(F("Erase "));
    Serial.print(filename);
    Serial.println(F(" FAILED"));
    return;
  }

  // Confirm that the file was deleted
  if (!sdStorage.exists(filename)) {
    Serial.print(F("Confirmed "));
    Serial.print(filename);
    Serial.println(F(" does not exist"));
  } else {
    Serial.println(F("Delete check FAILED"));
  }
}

void loop() {
}
