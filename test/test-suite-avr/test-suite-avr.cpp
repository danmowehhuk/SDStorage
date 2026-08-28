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
#include "avr/fatfs/ff.h"

using namespace BareMetalHAL;

// This board's pins: hardware SPI SCK=PB1/MOSI=PB2/MISO=PB3 (verified
// against pins_arduino.h), SD module's own CS=PE7 (this board's real
// wiring, confirmed directly - not a generic Mega SD shield's usual
// SS=PB0/D53 convention). Card-detect is on PE6, not used by this test.
static const uint8_t SD_CS_PIN = pin(Port::E, 7);

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

void testIndexUpsertLookupRemove(TestInvocation* t) {
  t->setName(F("idxUpsert()/idxLookup()/idxRemove() round-trip"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  f_unlink("/TESTROOT/~IDX/idx1.idx");

  Index myIdx(F("idx1"));
  // First upsert creates the index file (_writeIndexLine); second
  // upsert appends a new key to an existing index (_updateIndex).
  IndexEntry entry1(F("abc"), F("def"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry1), F("first upsert failed"));
  if (!t->passed()) return;
  IndexEntry entry2(F("ghi"), F("jkl"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry2), F("second upsert failed"));
  if (!t->passed()) return;

  // idxHasKey()/idxLookup() both scan the index via _scanIndex.
  t->verify(sdStorage.idxHasKey(myIdx, "abc"), F("key 'abc' not found"));
  t->verify(sdStorage.idxHasKey(myIdx, "ghi"), F("key 'ghi' not found"));
  char buf[10];
  t->verify(sdStorage.idxLookup(myIdx, "abc", buf, 10), F("lookup failed"));
  t->verifyEqual(buf, F("def"));

  // Removal also goes through _updateIndex, rewriting the index
  // without the removed key.
  t->verify(sdStorage.idxRemove(myIdx, "abc"), F("remove failed"));
  if (!t->passed()) return;
  t->verify(!sdStorage.idxHasKey(myIdx, "abc"), F("key 'abc' still present after remove"));
  t->verify(sdStorage.idxHasKey(myIdx, "ghi"), F("key 'ghi' unexpectedly removed too"));

  // cleanup
  f_unlink("/TESTROOT/~IDX/idx1.idx");
}

void testSeqCurrentNext(TestInvocation* t) {
  t->setName(F("seqCurrent()/seqNext() round-trip"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  f_unlink("/TESTROOT/~SEQ/seq1.seq");

  // A sequence must be advanced with seqNext() at least once before
  // seqCurrent() can be called on it - reading it first is a usage error.
  Sequence mySeq(F("seq1"));
  t->verify(sdStorage.seqNext(mySeq) == 1, F("Expected 1"));
  t->verify(sdStorage.seqNext(mySeq) == 2, F("Expected 2"));
  t->verify(sdStorage.seqCurrent(mySeq) == 2, F("current() after two next() calls should be 2"));

  f_unlink("/TESTROOT/~SEQ/seq1.seq");
}

void testFsckRecoversStaleTransaction(TestInvocation* t) {
  t->setName(F("begin() (fsck) recovers a stale, never-committed transaction"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("orphan.dat"));

  // Simulate a power loss mid-transaction: beginTxn() writes a real
  // .txn marker file to ~WORK immediately, before any save() happens.
  // Deliberately never commit, abort, or delete this Transaction - a
  // real crash wouldn't either, and recovering from exactly this is
  // fsck()'s job on the next begin().
  Transaction* txn = sdStorage.beginTxn(F("orphan.dat"));
  t->verify(txn, F("beginTxn failed"));
  if (!t->passed()) return;

  // Re-running begin() re-runs fsck(), which must roll back the
  // abandoned transaction by deleting its leftover .txn file. This
  // exercises f_opendir/f_readdir/f_unlink deleting a directory entry
  // while that same directory's read cursor is still open - the
  // single highest-risk, previously-unexercised path in this port.
  t->verify(sdStorage.begin(), F("begin() (recovery) failed"));
  if (!t->passed()) return;
  t->verify(!sdStorage.exists(F("orphan.dat")), F("orphan.dat should not exist (txn was never committed)"));

  // ~WORK should be empty again - no leftover transaction artifacts.
  FATFS_DIR dir;
  FILINFO fno;
  t->verify(f_opendir(&dir, "/TESTROOT/~WORK") == FR_OK, F("could not open ~WORK"));
  if (t->passed()) {
    t->verify(f_readdir(&dir, &fno) == FR_OK && fno.fname[0] == 0, F("~WORK not empty after recovery"));
    f_closedir(&dir);
  }
}

int main() {
  Uart0::begin(9600);
  timingInit();
  // CS isn't wired to this chip's own hardware SS pin on this board, so
  // PB0 needs its own OUTPUT configuration - and it must happen BEFORE
  // spiBegin(), not after: spiBegin() writes MSTR=1 to SPCR, and if SS
  // is still an input reading LOW at that exact moment, the hardware
  // immediately clears MSTR back to 0 (auto-switch to Slave mode, per
  // the datasheet) - configuring PB0 OUTPUT afterward doesn't undo
  // that. Confirmed as a real hang on real hardware from getting this
  // ordering backwards.
  pinMode(pin(Port::B, 0), OUTPUT);
  spiBegin(pin(Port::B, 1), pin(Port::B, 2), pin(Port::B, 3));

  TestFunction tests[] = {
    testBegin,
    testExists_absentFile,
    testMkdir,
    testSaveLoadErase,
    testIndexUpsertLookupRemove,
    testSeqCurrentNext,
    testFsckRecoversStaleTransaction
  };

  runTestSuiteShowMem(tests, before);

  while (true) {}
  return 0;
}
