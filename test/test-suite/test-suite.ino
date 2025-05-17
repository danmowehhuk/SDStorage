#include <SDStorage.h>
#include "SDStorageTestHelper.h"
#include <StreamableDTO.h>
#include <TestTool.h>

#define SD_CS_PIN 12

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
  if (!t->assert(beginSuccess, F("begin() failed"))) return;
  t->assert(sdFat, F("SdFat* required for test suite"));
  t->assert(sdFat->exists("/TESTROOT"), F("/TESTROOT does not exist"));
  t->assert(sdFat->exists("/TESTROOT/~WORK"), F("/TESTROOT/~WORK does not exist"));
  t->assert(sdFat->exists("/TESTROOT/~IDX"), F("/TESTROOT/~IDX does not exist"));

  File workDirFile = sdFat->open("/TESTROOT/~WORK");
  t->assert(!workDirFile.openNextFile(), F("/TESTROOT/~WORK not empty!"));
  beginSuccess = false;
}

void testCreateFile_empty(TestInvocation* t) {
  t->setName(F("Create an empty file"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  StreamableDTO dto;
  t->assert(!sdStorage.exists(F("file1.dat")), F("File already exists"));
  t->assert(sdStorage.save(F("file1.dat"), &dto), F("Save failed"));
  t->assert(sdFat->exists("/TESTROOT/file1.dat"), F("Saved file not found by SdFat"));
  t->assert(sdStorage.exists(F("file1.dat")), F("Saved file not found by SDStorage"));
}

void testCreateFile_readAfterWrite(TestInvocation* t) {
  t->setName(F("Create and read new file"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  StreamableDTO dtoIn;
  dtoIn.put(F("abc"),F("def"));
  t->assert(!sdStorage.exists(F("file2.dat")), F("file2.dat already exists"));
  t->assert(sdStorage.save(F("file2.dat"), &dtoIn), F("Save failed"));
  StreamableDTO dtoOut;
  t->assert(sdStorage.load(F("file2.dat"), &dtoOut), F("Load failed"));
  t->assertEqual(dtoOut.get(F("abc")), F("def"));
}

void testCreateFile_deleteAfterWrite(TestInvocation* t) {
  t->setName(F("Create and erase new file"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  StreamableDTO dtoIn;
  t->assert(!sdStorage.exists(F("file3.dat")), F("file3.dat already exists"));
  t->assert(sdStorage.save(F("file3.dat"), &dtoIn), F("Save failed"));
  t->assert(sdStorage.exists(F("file3.dat")), F("file3.dat not found"));
  t->assert(sdStorage.erase(F("file3.dat")), F("Erase failed"));
  t->assert(!sdStorage.exists(F("file3.dat")), F("file3.dat not erased"));
}

void testCreateDirectory(TestInvocation* t) {
  t->setName(F("Create directory"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  t->assert(!sdStorage.exists(F("myDir")), F("myDir already exists"));
  t->assert(sdStorage.mkdir(F("myDir")), F("mkdir failed"));
  t->assert(sdFat->exists("/TESTROOT/myDir"), F("myDir not found by SdFat"));
  t->assert(sdStorage.exists(F("/myDir")), F("myDir not found by SDStorage"));
}

void testCreateFile_inDirectory(TestInvocation* t) {
  t->setName(F("Create a file in a directory"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  StreamableDTO dto;
  t->assert(!sdStorage.exists(F("dir1")), F("dir1 already exists"));
  t->assert(sdStorage.mkdir(F("dir1")), F("mkdir failed"));
  t->assert(sdStorage.save(F("dir1/myDTO.dat"), &dto), F("Save failed"));
  t->assert(sdFat->exists("/TESTROOT/dir1/myDTO.dat"), F("Saved file not found by SdFat"));
  t->assert(sdStorage.exists(F("dir1/myDTO.dat")), F("Saved file not found by SDStorage"));
}

void testCreateIndex(TestInvocation* t) {
  t->setName(F("Create an index"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  t->assert(!sdFat->exists("/TESTROOT/~IDX/idx1.idx"), F("Index file already exists"));

  Index myIdx(F("idx1"));
  IndexEntry entry(F("abc"),F("def"));
  t->assert(sdStorage.idxUpsert(myIdx, &entry), F("Upsert failed"));
  t->assert(sdFat->exists("/TESTROOT/~IDX/idx1.idx"), F("Index file not found"));
}

void testIndexUpsert(TestInvocation* t) {
  t->setName(F("Upsert an index"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  t->assert(!sdFat->exists("/TESTROOT/~IDX/idx2.idx"), F("Index file already exists"));

  Index myIdx(F("idx2"));
  IndexEntry entry1(F("abc"),F("def"));
  t->assert(sdStorage.idxUpsert(myIdx, &entry1), F("First upsert failed"));
  t->assert(sdStorage.idxHasKey(myIdx, "abc"), F("Key 'abc' not found"));
  t->assert(!sdStorage.idxHasKey(myIdx, "ghi"), F("Key 'ghi' already exists"));

  IndexEntry entry2(F("ghi"),F("jkl"));
  t->assert(sdStorage.idxUpsert(myIdx, &entry2), F("Second upsert failed"));
  t->assert(sdStorage.idxHasKey(myIdx, "abc"), F("Key 'abc' not found"));
  t->assert(sdStorage.idxHasKey(myIdx, "ghi"), F("Key 'ghi' not found"));

  char buf[10];
  t->assert(sdStorage.idxLookup(myIdx, "abc", buf, 10), F("First lookup failed"));
  t->assertEqual(buf, F("def"));
  t->assert(sdStorage.idxLookup(myIdx, "ghi", buf, 10), F("Second lookup failed"));
  t->assertEqual(buf, F("jkl"));
}

void testIndexRemoveKey(TestInvocation* t) {
  t->setName(F("Remove a key from an index"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  t->assert(!sdFat->exists("/TESTROOT/~IDX/idx3.idx"), F("Index file already exists"));

  Index myIdx(F("idx3"));
  IndexEntry entry1(F("abc"),F("def"));
  t->assert(sdStorage.idxUpsert(myIdx, &entry1), F("Upsert failed"));
  t->assert(sdStorage.idxHasKey(myIdx, "abc"), F("Key 'abc' not found"));
  t->assert(sdStorage.idxRemove(myIdx, "abc"), F("Remove key failed"));
  t->assert(!sdStorage.idxHasKey(myIdx, "abc"), F("Key 'abc' not removed"));
}

void testTransaction_success(TestInvocation* t) {
  t->setName(F("Perform successful transaction"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  t->assert(!sdFat->exists("/TESTROOT/~IDX/idx4.idx"), F("Index file already exists"));
  t->assert(!sdFat->exists("/TESTROOT/file4.dat"), F("File already exists"));

  StreamableDTO dto;
  Index myIdx(F("idx4"));
  IndexEntry entry(F("abc"),F("def"));
  Transaction* txn = sdStorage.beginTxn(myIdx, F("file4.dat"));
  t->assert(txn, F("beginTxn failed"));
  t->assert(sdStorage.save(F("file4.dat"), &dto, txn), F("Save failed"));
  t->assert(sdStorage.idxUpsert(myIdx, &entry, txn), F("Index upsert failed"));

  t->assert(!sdFat->exists("/TESTROOT/~IDX/idx4.idx"), F("Index file exists before commit"));
  t->assert(!sdFat->exists("/TESTROOT/file4.dat"), F("File exists before commit"));
  t->assert(sdStorage.commitTxn(txn), F("commitTxn failed"));
  t->assert(sdFat->exists("/TESTROOT/~IDX/idx4.idx"), F("Index file not created"));
  t->assert(sdFat->exists("/TESTROOT/file4.dat"), F("File not created"));
}

void testTransaction_abort(TestInvocation* t) {
  t->setName(F("Abort a transaction"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;
  t->assert(!sdFat->exists("/TESTROOT/~IDX/idx5.idx"), F("Index file already exists"));
  t->assert(!sdFat->exists("/TESTROOT/file5.dat"), F("File already exists"));

  StreamableDTO dto;
  Index myIdx(F("idx5"));
  IndexEntry entry(F("abc"),F("def"));
  Transaction* txn = sdStorage.beginTxn(myIdx, F("file5.dat"));
  t->assert(txn, F("beginTxn failed"));
  t->assert(sdStorage.save(F("file5.dat"), &dto, txn), F("Save failed"));
  t->assert(sdStorage.idxUpsert(myIdx, &entry, txn), F("Index upsert failed"));

  t->assert(!sdFat->exists("/TESTROOT/~IDX/idx5.idx"), F("Index file exists before commit"));
  t->assert(!sdFat->exists("/TESTROOT/file5.dat"), F("File exists before commit"));
  t->assert(sdStorage.abortTxn(txn), F("abortTxn failed"));
  t->assert(!sdFat->exists("/TESTROOT/~IDX/idx5.idx"), F("Aborted index created anyway"));
  t->assert(!sdFat->exists("/TESTROOT/file5.dat"), F("Aborted file created anyway"));
}

void testFsck(TestInvocation* t) {
  t->setName(F("Filesystem check and repair (fsck)"));
  if (!t->assert(beginSuccess, F("SKIPPED"))) return;

  File workDirFile = sdFat->open("/TESTROOT/~WORK");
  if (!t->assert(!workDirFile.openNextFile(), F("/TESTROOT/~WORK not empty!"))) return;
  workDirFile.close();

  // Create some files in the ~WORK dir simulating a power loss during 
  // SD read/write activity. One uncommitted transaction, one that
  // has been marked ready to apply, and a file that isn't referenced 
  // in any transaction
  const char* fname1 = "/TESTROOT/fsck1.dat";
  const char* fname2 = "/TESTROOT/fsck2.dat";
  const char* fname3 = "/TESTROOT/~WORK/orphan.tmp";
  Transaction* uncommitted = helper.newTransaction(&sdStorage);
  helper.addToTxn(uncommitted, fname1);
  if (!t->assert(helper.writeTxn(sdFat, uncommitted, fname1), F("writeTxn failed for fname1"))) return;
  if (!t->assert(helper.createFile(sdFat, helper.getTmpFilename(uncommitted, fname1)), F("create fname1 failed"))) return;
  Transaction* committed = helper.newTransaction(&sdStorage);
  helper.commit(committed);
  helper.addToTxn(committed, fname2);
  if (!t->assert(helper.writeTxn(sdFat, committed, fname2), F("writeTxn failed for fname2"))) return;
  if (!t->assert(helper.createFile(sdFat, helper.getTmpFilename(committed, fname2)), F("create fname2 failed"))) return;
  if (!t->assert(helper.createFile(sdFat, fname3), F("create fname3 failed"))) return;
  if (!t->assert(sdFat->exists(fname1), F("fname1 already exists")));
  if (!t->assert(sdFat->exists(fname2), F("fname2 already exists")));
  errThrown = false;

  // Perform the fsck
  t->assert((helper.doFsck(&sdStorage) && !errThrown), F("fsck failed"));

  // uncommitted transaction should have been cleaned up, file should have been written for committed
  // transaction, orphan file should have been cleaned up
  t->assert(!sdFat->exists(fname1), F("fname1 should NOT have been written"));
  t->assert(sdFat->exists(fname2), F("fname2 should have been written"));
  File workDirFileAfter = sdFat->open("/TESTROOT/~WORK");
  t->assert(!workDirFileAfter.openNextFile(), F("/TESTROOT/~WORK not empty after fsck"));
  workDirFileAfter.close();
}


void setup() {
  Serial.begin(9600);
  while (!Serial);

  TestFunction tests[] = {
    testBegin,
    testCreateFile_empty,
    testCreateFile_readAfterWrite,
    testCreateDirectory,
    testCreateFile_inDirectory,
    testCreateIndex,
    testIndexUpsert,
    testIndexRemoveKey,
    testTransaction_success,
    testTransaction_abort,
    testFsck
  };

  runTestSuiteShowMem(tests, before, nullptr);
}


void loop() {}

