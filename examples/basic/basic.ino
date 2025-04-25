#include "SDStorage.h"
#include <StreamableDTO.h>
#include <StreamableManager.h>


/*
 * Wire the SD card reader to the Arduino's hardware SPI and update
 * the SD_CS_PIN value below to whichever pin was used for the SD's
 * chip select
 */
#define SD_CS_PIN 12

void setup() {
  Serial.begin(9600);
  while (!Serial);

  String rootDir(F("DATA"));
  SDStorage sdStorage(SD_CS_PIN, rootDir);
  if (!sdStorage.begin()) {
    Serial.println(F("SDStorage init failed"));
  }

  String filename(F("/DATA/myFile.dat"));

  // Create a new file of simple key-value pairs
  StreamableDTO newObj;
  newObj.put("foo", "bar");
  newObj.put("abc", "def");
  if (sdStorage.save(filename, &newObj)) {
    Serial.println(F("Saved newObj"));
  } else {
    Serial.println(F("Save newObj FAILED"));
  }
  delete &newObj; // clear it from memory

  // Confirm that the file has been created
  if (sdStorage.exists(filename)) {
    Serial.println(String(F("Confirmed ")) + filename + String(F(" exists")));
  } else {
    Serial.println(F("Existance check FAILED"));
  }

  // For printing StreamableDTO contents to Serial
  StreamableManager stream;

  // Load the saved file back into memory
  StreamableDTO savedObj;
  if (sdStorage.load(filename, &savedObj)) {
    Serial.println(String(F("Reloaded ")) + filename + String(F(" into memory:")));
    // Print the key-value pairs to Serial
    stream.send(&Serial, &savedObj);
  } else {
    Serial.println(F("Load savedObj FAILED"));
  }

  // Update one of the keys and save
  savedObj.put("abc", "ghi");
  if (sdStorage.save(filename, &savedObj)) {
    Serial.println(F("Saved updated savedObj"));
  } else {
    Serial.println(F("Save updated savedObj FAILED"));
  }
  delete &savedObj; // clear it from memory

  // Load the saved file back into memory
  StreamableDTO savedObj2;
  if (sdStorage.load(filename, &savedObj2)) {
    Serial.println(String(F("Reloaded ")) + filename + String(F(" into memory:")));
    // Print the key-value pairs to Serial
    stream.send(&Serial, &savedObj2);
  } else {
    Serial.println(F("Load savedObj2 FAILED"));
  }
  delete &savedObj2;

  // Delete the file
  if (sdStorage.erase(filename)) {
    Serial.println(String(F("Erased ")) + filename);
  } else {
    Serial.println(String(F("Erase ")) + filename + String(F(" FAILED")));
  }

  // Confirm that the file was deleted
  if (!sdStorage.exists(filename)) {
    Serial.println(String(F("Confirmed ")) + filename + String(F(" does not exist")));
  } else {
    Serial.println(F("Delete check FAILED"));
  }

}

void loop() {
}
