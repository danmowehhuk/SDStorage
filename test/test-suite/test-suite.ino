#include <avr/wdt.h>
#include <SDStorage.h>
#include "SDStorageTestHelper.h"
#include <StreamableDTO.h>
#include <TestTool.h>

#define SD_CS_PIN 53

bool didBegin = false;
bool beginSuccess = false;
bool errThrown = false;
void errFunction() {
  errThrown = true;
}

static const char TESTROOT[] PROGMEM = "TESTROOT";

SDStorageTestHelper helper;
SDStorage sdStorage(SD_CS_PIN, TESTROOT, true, errFunction);
SdFat* sdFat = nullptr;

void before() {
  if (!didBegin) {
    beginSuccess = sdStorage.begin();
    sdFat = helper.getSdFat(&sdStorage);
    didBegin = true;
  }
}

void testBegin(TestInvocation* t) {
  t->setName(F("SDStorage initialization"));
  t->verify(beginSuccess, F("begin() failed"));
  if (!t->passed()) return;
  t->verify(sdFat, F("SdFat* required for test suite"));
  t->verify(sdFat->exists("/TESTROOT"), F("/TESTROOT does not exist"));
  t->verify(sdFat->exists("/TESTROOT/~WORK"), F("/TESTROOT/~WORK does not exist"));
  t->verify(sdFat->exists("/TESTROOT/~IDX"), F("/TESTROOT/~IDX does not exist"));
  t->verify(sdFat->exists("/TESTROOT/~SEQ"), F("/TESTROOT/~SEQ does not exist"));

  File workDirFile = sdFat->open("/TESTROOT/~WORK");
  t->verify(!workDirFile.openNextFile(), F("/TESTROOT/~WORK not empty!"));
  if (!t->passed()) {
    beginSuccess = false;
  }
}

void testCreateFile_empty(TestInvocation* t) {
  t->setName(F("Create an empty file"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("file1.dat"));

  StreamableDTO dto;
  t->verify(!sdStorage.exists(F("file1.dat")), F("File already exists"));
  t->verify(sdStorage.save(F("file1.dat"), &dto), F("Save failed"));
  t->verify(sdFat->exists("/TESTROOT/file1.dat"), F("Saved file not found by SdFat"));
  t->verify(sdStorage.exists(F("file1.dat")), F("Saved file not found by SDStorage"));
  
  // cleanup
  t->verify(sdStorage.erase(F("file1.dat")), F("Erase failed"));
  t->verify(!sdStorage.exists(F("file1.dat")), F("file1.dat not erased"));
}

void testCreateFile_readAfterWrite(TestInvocation* t) {
  t->setName(F("Create and read new file"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("file2.dat"));

  StreamableDTO dtoIn;
  dtoIn.put(F("abc"),F("def"));
  t->verify(!sdStorage.exists(F("file2.dat")), F("file2.dat already exists"));
  t->verify(sdStorage.save(F("file2.dat"), &dtoIn), F("Save failed"));
  StreamableDTO dtoOut;
  t->verify(sdStorage.load(F("file2.dat"), &dtoOut), F("Load failed"));
  t->verifyEqual(dtoOut.get(F("abc")), F("def"));
  
  // cleanup
  t->verify(sdStorage.erase(F("file2.dat")), F("Erase failed"));
  t->verify(!sdStorage.exists(F("file2.dat")), F("file2.dat not erased"));
}

void testCreateFile_deleteAfterWrite(TestInvocation* t) {
  t->setName(F("Create and erase new file"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("file3.dat"));

  StreamableDTO dtoIn;
  t->verify(!sdStorage.exists(F("file3.dat")), F("file3.dat already exists"));
  t->verify(sdStorage.save(F("file3.dat"), &dtoIn), F("Save failed"));
  t->verify(sdStorage.exists(F("file3.dat")), F("file3.dat not found"));
  t->verify(sdStorage.erase(F("file3.dat")), F("Erase failed"));
  t->verify(!sdStorage.exists(F("file3.dat")), F("file3.dat not erased"));
  // self-cleaning
}

void testCreateDirectory(TestInvocation* t) {
  t->setName(F("Create directory"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("myDir"));

  t->verify(!sdStorage.exists(F("myDir")), F("myDir already exists"));
  t->verify(sdStorage.mkdir(F("myDir")), F("mkdir failed"));
  t->verify(sdFat->exists("/TESTROOT/myDir"), F("myDir not found by SdFat"));
  t->verify(sdStorage.exists(F("/myDir")), F("myDir not found by SDStorage"));
  
  // cleanup
  t->verify(sdFat->rmdir("/TESTROOT/myDir"), F("Erase failed"));
  t->verify(!sdStorage.exists(F("myDir")), F("myDir not erased"));
}

void testCreateFile_inDirectory(TestInvocation* t) {
  t->setName(F("Create a file in a directory"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("/TESTROOT/dir1/myDTO.dat"));
  sdStorage.erase(F("/TESTROOT/dir1"));

  StreamableDTO dto;
  t->verify(!sdStorage.exists(F("dir1")), F("dir1 already exists"));
  t->verify(sdStorage.mkdir(F("dir1")), F("mkdir failed"));
  t->verify(sdStorage.save(F("dir1/myDTO.dat"), &dto), F("Save failed"));
  t->verify(sdFat->exists("/TESTROOT/dir1/myDTO.dat"), F("Saved file not found by SdFat"));
  t->verify(sdStorage.exists(F("dir1/myDTO.dat")), F("Saved file not found by SDStorage"));

  // cleanup
  t->verify(sdStorage.erase(F("dir1/myDTO.dat")), F("Erase failed"));
  t->verify(!sdStorage.exists(F("dir1/myDTO.dat")), F("dir1/myDTO.dat not erased"));
  t->verify(sdFat->rmdir("/TESTROOT/dir1"), F("Erase failed"));
  t->verify(!sdStorage.exists(F("dir1")), F("dir1 not erased"));
}

void testCreateIndex(TestInvocation* t) {
  t->setName(F("Create an index"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdFat->remove("/TESTROOT/~IDX/idx1.idx");

  t->verify(!sdFat->exists("/TESTROOT/~IDX/idx1.idx"), F("Index file already exists"));

  Index myIdx(F("idx1"));
  IndexEntry entry(F("abc"),F("def"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry), F("Upsert failed"));
  t->verify(sdFat->exists("/TESTROOT/~IDX/idx1.idx"), F("Index file not found"));

  // cleanup
  t->verify(sdFat->remove(F("/TESTROOT/~IDX/idx1.idx")), F("Erase failed"));
  t->verify(!sdFat->exists(F("/TESTROOT/~IDX/idx1.idx")), F("/TESTROOT/~IDX/idx1.idx not erased"));
}

void testIndexUpsert(TestInvocation* t) {
  t->setName(F("Upsert an index"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdFat->remove("/TESTROOT/~IDX/idx2.idx");

  t->verify(!sdFat->exists("/TESTROOT/~IDX/idx2.idx"), F("Index file already exists"));
  Index myIdx(F("idx2"));
  IndexEntry entry1(F("abc"),F("def"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry1), F("First upsert failed"));
  t->verify(sdStorage.idxHasKey(myIdx, "abc"), F("New key 'abc' not found"));
  t->verify(!sdStorage.idxHasKey(myIdx, "ghi"), F("Key 'ghi' already exists"));

  IndexEntry entry2(F("ghi"),F("jkl"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry2), F("Second upsert failed"));
  t->verify(sdStorage.idxHasKey(myIdx, "abc"), F("Existing key 'abc' not found"));
  t->verify(sdStorage.idxHasKey(myIdx, "ghi"), F("New key 'ghi' not found"));

  char buf[10];
  t->verify(sdStorage.idxLookup(myIdx, "abc", buf, 10), F("First lookup failed"));
  t->verifyEqual(buf, F("def"));
  t->verify(sdStorage.idxLookup(myIdx, "ghi", buf, 10), F("Second lookup failed"));
  t->verifyEqual(buf, F("jkl"));

  // cleanup
  t->verify(sdFat->remove(F("/TESTROOT/~IDX/idx2.idx")), F("Erase failed"));
  t->verify(!sdFat->exists(F("/TESTROOT/~IDX/idx2.idx")), F("/TESTROOT/~IDX/idx2.idx not erased"));
}

void testIndexRemoveKey(TestInvocation* t) {
  t->setName(F("Remove a key from an index"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdFat->remove("/TESTROOT/~IDX/idx3.idx");

  t->verify(!sdFat->exists("/TESTROOT/~IDX/idx3.idx"), F("Index file already exists"));
  Index myIdx(F("idx3"));
  IndexEntry entry1(F("abc"),F("def"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry1), F("Upsert failed"));
  t->verify(sdStorage.idxHasKey(myIdx, "abc"), F("Key 'abc' not found"));
  t->verify(sdStorage.idxRemove(myIdx, "abc"), F("Remove key failed"));
  t->verify(!sdStorage.idxHasKey(myIdx, "abc"), F("Key 'abc' not removed"));

  // cleanup
  t->verify(sdFat->remove(F("/TESTROOT/~IDX/idx3.idx")), F("Erase failed"));
  t->verify(!sdFat->exists(F("/TESTROOT/~IDX/idx3.idx")), F("/TESTROOT/~IDX/idx3.idx not erased"));
}

void testTransaction_success(TestInvocation* t) {
  t->setName(F("Perform successful transaction"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdFat->remove("/TESTROOT/~IDX/idx4.idx");
  sdStorage.erase(F("file4.dat"));

  t->verify(!sdFat->exists("/TESTROOT/~IDX/idx4.idx"), F("Index file already exists"));
  t->verify(!sdStorage.exists(F("/TESTROOT/file4.dat")), F("File already exists"));

  StreamableDTO dto;
  Index myIdx(F("idx4"));
  IndexEntry entry(F("abc"),F("def"));
  Transaction* txn = sdStorage.beginTxn(myIdx, F("file4.dat"));
  t->verify(txn, F("beginTxn failed"));
  t->verify(sdStorage.save(F("file4.dat"), &dto, txn), F("Save failed"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry, txn), F("Index upsert failed"));

  t->verify(!sdFat->exists("/TESTROOT/~IDX/idx4.idx"), F("Index file exists before commit"));
  t->verify(!sdFat->exists("/TESTROOT/file4.dat"), F("File exists before commit"));
  t->verify(sdStorage.commitTxn(txn), F("commitTxn failed"));
  t->verify(sdFat->exists("/TESTROOT/~IDX/idx4.idx"), F("Index file not created"));
  t->verify(sdFat->exists("/TESTROOT/file4.dat"), F("File not created"));

  // cleanup
  t->verify(sdFat->remove(F("/TESTROOT/~IDX/idx4.idx")), F("Erase failed"));
  t->verify(!sdFat->exists(F("/TESTROOT/~IDX/idx4.idx")), F("/TESTROOT/~IDX/idx4.idx not erased"));
  t->verify(sdStorage.erase(F("file4.dat")), F("Erase failed"));
  t->verify(!sdStorage.exists(F("file4.dat")), F("file4.dat not erased"));
}

void testTransaction_abort(TestInvocation* t) {
  t->setName(F("Abort a transaction"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdFat->remove("/TESTROOT/~IDX/idx5.idx");
  sdStorage.erase(F("file5.dat"));

  t->verify(!sdFat->exists("/TESTROOT/~IDX/idx5.idx"), F("Index file already exists"));
  t->verify(!sdFat->exists("/TESTROOT/file5.dat"), F("File already exists"));
  StreamableDTO dto;
  Index myIdx(F("idx5"));
  IndexEntry entry(F("abc"),F("def"));
  Transaction* txn = sdStorage.beginTxn(myIdx, F("file5.dat"));
  t->verify(txn, F("beginTxn failed"));
  t->verify(sdStorage.save(F("file5.dat"), &dto, txn), F("Save failed"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry, txn), F("Index upsert failed"));

  t->verify(!sdFat->exists("/TESTROOT/~IDX/idx5.idx"), F("Index file exists before commit"));
  t->verify(!sdFat->exists("/TESTROOT/file5.dat"), F("File exists before commit"));
  t->verify(sdStorage.abortTxn(txn), F("abortTxn failed"));
  t->verify(!sdFat->exists("/TESTROOT/~IDX/idx5.idx"), F("Aborted index created anyway"));
  t->verify(!sdFat->exists("/TESTROOT/file5.dat"), F("Aborted file created anyway"));
}

void testTransaction_repeatFileEdit(TestInvocation* t) {
  t->setName(F("Edit the same file twice in one transaction, interleaved with another file"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("file6.dat"));
  sdStorage.erase(F("file7.dat"));

  Transaction* txn = sdStorage.beginTxn(F("file6.dat"), F("file7.dat"));
  t->verify(txn, F("beginTxn failed"));

  StreamableDTO dtoA1;
  dtoA1.put(F("step"), F("1"));
  t->verify(sdStorage.save(F("file6.dat"), &dtoA1, txn), F("First save of file6 failed"));

  StreamableDTO dtoB;
  dtoB.put(F("b"), F("1"));
  t->verify(sdStorage.save(F("file7.dat"), &dtoB, txn), F("Save of file7 failed"));

  StreamableDTO dtoA2;
  t->verify(sdStorage.load(F("file6.dat"), &dtoA2, nullptr, txn), F("Reload of file6 within the transaction failed"));
  t->verifyEqual(dtoA2.get(F("step")), F("1"), F("Reload within the transaction lost the first edit"));
  dtoA2.put(F("step"), F("2"));
  t->verify(sdStorage.save(F("file6.dat"), &dtoA2, txn), F("Second save of file6 failed"));

  t->verify(sdStorage.commitTxn(txn), F("commitTxn failed"));

  StreamableDTO finalA;
  t->verify(sdStorage.load(F("file6.dat"), &finalA), F("Final load of file6 failed"));
  t->verifyEqual(finalA.get(F("step")), F("2"), F("Committed file6 does not reflect both edits"));
  StreamableDTO finalB;
  t->verify(sdStorage.load(F("file7.dat"), &finalB), F("Final load of file7 failed"));
  t->verifyEqual(finalB.get(F("b")), F("1"), F("Committed file7 is incorrect"));

  // cleanup
  t->verify(sdStorage.erase(F("file6.dat")), F("Erase failed"));
  t->verify(sdStorage.erase(F("file7.dat")), F("Erase failed"));
}

void testTransaction_repeatFileAndIndexEdits(TestInvocation* t) {
  t->setName(F("Repeated file and index edits, interleaved, in one transaction"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdFat->remove("/TESTROOT/~IDX/idx6.idx");
  sdStorage.erase(F("file8.dat"));
  sdStorage.erase(F("file9.dat"));

  Index myIdx(F("idx6"));
  Transaction* txn = sdStorage.beginTxn(myIdx, F("file8.dat"), F("file9.dat"));
  t->verify(txn, F("beginTxn failed"));

  StreamableDTO dtoA;
  dtoA.put(F("n"), F("1"));
  t->verify(sdStorage.save(F("file8.dat"), &dtoA, txn), F("Save of file8 (edit 1) failed"));
  IndexEntry entryA(F("file8"), F("1"));
  t->verify(sdStorage.idxUpsert(myIdx, &entryA, txn), F("First index upsert failed"));

  StreamableDTO dtoB;
  dtoB.put(F("n"), F("1"));
  t->verify(sdStorage.save(F("file9.dat"), &dtoB, txn), F("Save of file9 failed"));

  StreamableDTO dtoA2;
  t->verify(sdStorage.load(F("file8.dat"), &dtoA2, nullptr, txn), F("Reload of file8 within the transaction failed"));
  t->verifyEqual(dtoA2.get(F("n")), F("1"), F("Reload of file8 within the transaction lost the first edit"));
  dtoA2.put(F("n"), F("2"));
  t->verify(sdStorage.save(F("file8.dat"), &dtoA2, txn), F("Save of file8 (edit 2) failed"));
  IndexEntry entryA2(F("file8"), F("2"));
  t->verify(sdStorage.idxUpsert(myIdx, &entryA2, txn), F("Second index upsert (same key) failed"));

  t->verify(sdStorage.commitTxn(txn), F("commitTxn failed"));

  StreamableDTO finalA;
  t->verify(sdStorage.load(F("file8.dat"), &finalA), F("Final load of file8 failed"));
  t->verifyEqual(finalA.get(F("n")), F("2"), F("Committed file8 does not reflect both edits"));
  StreamableDTO finalB;
  t->verify(sdStorage.load(F("file9.dat"), &finalB), F("Final load of file9 failed"));
  t->verifyEqual(finalB.get(F("n")), F("1"), F("Committed file9 is incorrect"));
  char buf[10];
  t->verify(sdStorage.idxLookup(myIdx, "file8", buf, 10), F("Index lookup failed"));
  t->verifyEqual(buf, F("2"), F("Index does not reflect the second upsert"));

  // cleanup
  t->verify(sdFat->remove(F("/TESTROOT/~IDX/idx6.idx")), F("Erase failed"));
  t->verify(sdStorage.erase(F("file8.dat")), F("Erase failed"));
  t->verify(sdStorage.erase(F("file9.dat")), F("Erase failed"));
}

void testFsck(TestInvocation* t) {
  t->setName(F("Filesystem check and repair (fsck)"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  const char* fname1 = "/TESTROOT/fsck1.dat";
  const char* fname2 = "/TESTROOT/fsck2.dat";
  const char* fname3 = "/TESTROOT/~WORK/orphan.tmp";
  sdFat->remove(fname1);
  sdFat->remove(fname2);
  sdFat->remove(fname3);

  File workDirFile = sdFat->open("/TESTROOT/~WORK");
  t->verify(!workDirFile.openNextFile(), F("/TESTROOT/~WORK not empty!"));
  if (!t->passed()) return;
  workDirFile.close();

  // Create some files in the ~WORK dir simulating a power loss during 
  // SD read/write activity. One uncommitted transaction, one that
  // has been marked ready to apply, and a file that isn't referenced 
  // in any transaction
  Transaction* uncommitted = helper.newTransaction(&sdStorage);
  helper.addToTxn(uncommitted, fname1);
  t->verify(helper.writeTxn(sdFat, uncommitted, fname1), F("writeTxn failed for fname1"));
  if (!t->passed()) return;
  char* tmpFilename1 = helper.getTmpFilename(uncommitted, fname1);
  t->verify(helper.createFile(sdFat, tmpFilename1), F("create fname1 tmpfile failed"));
  if (!t->passed()) return;
  t->verify(!sdFat->exists(fname1), F("fname1 should NOT have been written"));
  if (!t->passed()) return;
  t->verify(sdFat->exists(tmpFilename1), F("fname1 tmpfile should have been written"));
  if (!t->passed()) return;

  Transaction* committed = helper.newTransaction(&sdStorage);
  helper.commit(committed);
  helper.addToTxn(committed, fname2);
  t->verify(helper.writeTxn(sdFat, committed, fname2), F("writeTxn failed for fname2"));
  if (!t->passed()) return;
  char* tmpFilename2 = helper.getTmpFilename(committed, fname2);
  t->verify(helper.createFile(sdFat, tmpFilename2), F("create fname2 tmpfile failed"));
  if (!t->passed()) return;
  t->verify(!sdFat->exists(fname2), F("fname2 should NOT have been written"));
  if (!t->passed()) return;
  t->verify(sdFat->exists(tmpFilename2), F("fname2 tmpfile should have been written"));
  if (!t->passed()) return;

  t->verify(helper.createFile(sdFat, fname3), F("create fname3 failed"));
  if (!t->passed()) return;
  errThrown = false;
  delete uncommitted;
  delete committed;

  // Perform the fsck
  t->verify((helper.doFsck(&sdStorage) && !errThrown), F("fsck failed"));

  // uncommitted transaction should have been cleaned up, file should have been written for committed
  // transaction, orphan file should have been cleaned up
  t->verify(!sdFat->exists(fname1), F("fname1 should NOT have been written"));
  t->verify(sdFat->exists(fname2), F("fname2 should have been written"));
  File workDirFileAfter = sdFat->open("/TESTROOT/~WORK");
  t->verify(!workDirFileAfter.openNextFile(), F("/TESTROOT/~WORK not empty after fsck"));
  workDirFileAfter.close();
}

void testSeqCurrentNext(TestInvocation* t) {
  t->setName(F("seqCurrent()/seqNext() round-trip"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdFat->remove("/TESTROOT/~SEQ/seq1.seq");

  // A sequence must be advanced with seqNext() at least once before
  // seqCurrent() can be called on it - reading it first is a usage error.
  Sequence mySeq(F("seq1"));
  t->verify(sdStorage.seqNext(mySeq) == 1, F("Expected 1"));
  t->verify(sdStorage.seqNext(mySeq) == 2, F("Expected 2"));
  t->verify(sdStorage.seqCurrent(mySeq) == 2, F("current() after two next() calls should be 2"));

  // cleanup
  t->verify(sdFat->remove(F("/TESTROOT/~SEQ/seq1.seq")), F("Erase failed"));
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  wdt_disable(); // disable watchdog timer

  // Disable anything else connected to the SPI bus
  const int usbChipSelect = 3;
  pinMode(usbChipSelect, OUTPUT);
  digitalWrite(usbChipSelect, HIGH);

  TestFunction tests[] = {
    testBegin,
    testCreateFile_empty,
    testCreateFile_readAfterWrite,
    testCreateFile_deleteAfterWrite,
    testCreateDirectory,
    testCreateFile_inDirectory,
    testCreateIndex,
    testIndexUpsert,
    testIndexRemoveKey,
    testTransaction_success,
    testTransaction_abort,
    testFsck,
    testSeqCurrentNext
  };

  runTestSuiteShowMem(tests, before, nullptr);
}


void loop() {}

