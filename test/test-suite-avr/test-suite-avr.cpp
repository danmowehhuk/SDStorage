// test/test-suite-avr/test-suite-avr.cpp
//
// Real-hardware AVR port of ../test-suite/test-suite.ino - exercises
// StorageProvider's real methods (through SDStorage's public API)
// against an actual SD card over BareMetalHAL's SPI/FatFs driver.
// SimulIDE cannot simulate an SD card, so this is not SimulIDE-
// verifiable (same constraint as BareMetalHAL's own sd-basic-avr
// example). The card must already carry a FAT12/16/32 filesystem -
// this build has f_mkfs() disabled and exFAT unsupported.
#include <BareMetalHAL.h>
#include <TestTool.h>
#include <SDStorage.h>
#include <StreamableDTO.h>

using namespace BareMetalHAL;

// This board's pins (verified against pins_arduino.h): SCK=PB1,
// MOSI=PB2, MISO=PB3, SD_CS=PB0/D53 (also this chip's hardware SS).
static const uint8_t SD_CS_PIN = pin(Port::B, 0);

static const char TESTROOT[] PROGMEM = "TESTROOT";

SDStorage sdStorage(SD_CS_PIN, TESTROOT, true);

bool didBegin = false;
bool beginSuccess = false;

void before() {
  if (!didBegin) {
    beginSuccess = sdStorage.begin();
    didBegin = true;
  }
}

void testBegin(TestInvocation* t) {
  t->setName(F("SDStorage initialization"));
  t->verify(beginSuccess, F("begin() failed"));
}

void testExists_absentFile(TestInvocation* t) {
  t->setName(F("exists() reports false for an absent file"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  t->verify(!sdStorage.exists(F("nope.dat")), F("nope.dat should not exist"));
}

void testMkdir(TestInvocation* t) {
  t->setName(F("mkdir() creates a directory"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("mydir"));

  t->verify(!sdStorage.exists(F("mydir")), F("mydir already exists"));
  if (!t->passed()) return;
  t->verify(sdStorage.mkdir(F("mydir")), F("mkdir failed"));
  if (!t->passed()) return;
  t->verify(sdStorage.exists(F("mydir")), F("mydir not found after mkdir"));
  if (!t->passed()) return;

  // cleanup
  t->verify(sdStorage.erase(F("mydir")), F("erase (rmdir) failed"));
  if (!t->passed()) return;
  t->verify(!sdStorage.exists(F("mydir")), F("mydir not erased"));
}

void testSaveLoadErase(TestInvocation* t) {
  t->setName(F("save()/load()/erase() round-trip a StreamableDTO"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("file1.dat"));

  StreamableDTO dtoIn;
  dtoIn.put(F("abc"), F("def"));
  t->verify(!sdStorage.exists(F("file1.dat")), F("file1.dat already exists"));
  if (!t->passed()) return;
  // save() writes to a tmp file then renames it into place on commit,
  // exercising StorageProvider's _rename as well as _writeToStream.
  t->verify(sdStorage.save(F("file1.dat"), &dtoIn), F("save failed"));
  if (!t->passed()) return;
  t->verify(sdStorage.exists(F("file1.dat")), F("saved file not found"));
  if (!t->passed()) return;

  StreamableDTO dtoOut;
  t->verify(sdStorage.load(F("file1.dat"), &dtoOut), F("load failed"));
  if (!t->passed()) return;
  t->verifyEqual(dtoOut.get(F("abc")), F("def"));

  t->verify(sdStorage.erase(F("file1.dat")), F("erase failed"));
  if (!t->passed()) return;
  t->verify(!sdStorage.exists(F("file1.dat")), F("file1.dat not erased"));
}

int main() {
  Uart0::begin(9600);
  timingInit();
  spiBegin(pin(Port::B, 1), pin(Port::B, 2), pin(Port::B, 3));

  TestFunction tests[] = {
    testBegin,
    testExists_absentFile,
    testMkdir,
    testSaveLoadErase
  };

  runTestSuiteShowMem(tests, before);

  while (true) {}
  return 0;
}
