#include <SDStorage.h>
#include <StreamableManager.h>
#include <TestTool.h>
#include <sdstorage/Strings.h>
#include "MockSdFat.h"
#include "SDStorageTestHelper.h"

SDStorageTestHelper helper;
StreamableManager _streams;
SDStorage* sdStorage = nullptr;
bool beginSuccess = false;

using namespace SDStorageStrings;

bool errThrown = false;
void errFunction() {
  errThrown = true;
}

void before() {
  if (!sdStorage) {
    sdStorage = new SDStorage(12, reinterpret_cast<const __FlashStringHelper *>(_MOCK_TESTROOT), errFunction);
    MockSdFat::TestState ts;
    ts.onExistsReturn[0] = false; // root exists?
    ts.onExistsReturn[1] = false; // workdir exists?
    ts.onExistsReturn[2] = false; // idx dir exists?
    beginSuccess = sdStorage->begin(&ts);
  }
}

void testBegin(TestInvocation* t) {
  t->setName(F("SDStorage initialization"));
  t->assert(beginSuccess, F("begin() failed"));
  t->assertEqual(helper.getRootDir(sdStorage), F("/TESTROOT"));
  t->assertEqual(helper.getWorkDir(sdStorage), F("/TESTROOT/~WORK"));
  t->assertEqual(helper.getIdxDir(sdStorage), F("/TESTROOT/~IDX"));
}

void testConstructor(TestInvocation* t) {
  t->setName(F("Constructor/destructor memory leaks"));
  auto errFunc = []() {};
  SDStorage* s0 = new SDStorage(12, "TESTROOT", errFunc);
  delete s0;
  SDStorage* s1 = new SDStorage(12, F("TESTROOT"), errFunc);
  delete s1;
  SDStorage* s2 = new SDStorage(12, _MOCK_TESTROOT, true, errFunc);
  delete s2;
}

void testCanonicalFilename(TestInvocation* t) {
  t->setName(F("Filename resolution"));
  char resolvedName[64];
  t->assert(helper.canonicalFilename(sdStorage, helper.toFilename(F("foo")), resolvedName, 64), 
        F("canonicalFilename call 1 failed"));
  t->assertEqual(resolvedName, F("/TESTROOT/foo"), F("Prepend root dir prefix failed"));
  t->assert(helper.canonicalFilename(sdStorage, helper.toFilename(F("/foo")), resolvedName, 64), 
        F("canonicalFilename call 2 failed"));
  t->assertEqual(resolvedName, F("/TESTROOT/foo"), F("Ignore leading slash failed"));
  t->assert(helper.canonicalFilename(sdStorage, helper.toFilename(F("/TESTROOT/foo")), resolvedName, 64),
        F("canonicalFilename call 3 failed"));
  t->assertEqual(resolvedName, F("/TESTROOT/foo"), F("Ignore already under rootDir"));
}

void testMakeDir(TestInvocation* t) {
  t->setName(F("mkdir prepends rootDir"));
  MockSdFat::TestState ts;
  t->assert(sdStorage->mkdir(F("foo"), &ts), F("mkdir call failed"));
  t->assertEqual(ts.mkdirCaptor, F("/TESTROOT/foo"));
}

void testFileExists(TestInvocation* t) {
  t->setName(F("file exists mock"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false;
  ts.onExistsReturn[1] = true;
  ts.onExistsReturn[2] = false;
  t->assert(!sdStorage->exists(F("foo"), &ts), F("expected false"));
  t->assert(sdStorage->exists(F("foo"), &ts), F("expected true"));
  t->assert(!sdStorage->exists(F("foo"), &ts), F("expected false"));
}

void testIsValidFAT16Filename(TestInvocation* t) {
  t->setName(F("FAT16 filename validation"));
  t->assert(!helper.isValidFAT16Filename(sdStorage, F("")), F("Empty filename"));
  t->assert(!helper.isValidFAT16Filename(sdStorage, F("  ")), F("Filename only spaces"));
  t->assert(!helper.isValidFAT16Filename(sdStorage, F("foo.bar.baz")), F("Multiple dots in filename"));
  t->assert(!helper.isValidFAT16Filename(sdStorage, F("filenametoolong.txt")), F("Filename too long"));
  t->assert(!helper.isValidFAT16Filename(sdStorage, F("foo.")), F("Zero-length filename extension"));
  t->assert(!helper.isValidFAT16Filename(sdStorage, F("foo.text")), F("Filename extension too long"));
  t->assert(!helper.isValidFAT16Filename(sdStorage, F("f£o.txt")), F("Invalid filename character"));
  t->assert(helper.isValidFAT16Filename(sdStorage, F("foo.txt")), F("Should have been valid filename"));
  t->assert(helper.isValidFAT16Filename(sdStorage, F("foo")), F("No filename extension should be valid"));
}

void testGetPathFromFilename(TestInvocation* t) {
  t->setName(F("Extract path from filename"));
  char path[64];
  t->assert(!helper.getPathFromFilename(sdStorage, F(""), path, 64), 
        F("should have failed (empty)"));
  t->assert(!helper.getPathFromFilename(sdStorage, F("  "), path, 64),
        F("should have failed (whitespace)"));
  t->assert(!helper.getPathFromFilename(sdStorage, F("bar"), path, 64),
        F("should have failed (no slash)"));
  t->assert(helper.getPathFromFilename(sdStorage, F("/bar"), path, 64),
        F("getPathFromFilename call 1 failed"));
  t->assertEqual(path, F("/"));
  t->assert(helper.getPathFromFilename(sdStorage, F("foo/bar"), path, 64),
        F("getPathFromFilename call 2 failed"));
  t->assertEqual(path, F("foo"));
  t->assert(helper.getPathFromFilename(sdStorage, F("/foo/bar"), path, 64),
        F("getPathFromFilename call 3 failed"));
  t->assertEqual(path, F("/foo"));
  t->assert(helper.getPathFromFilename(sdStorage, F("/foo/bar/baz.txt"), path, 64),
        F("getPathFromFilename call 4 failed"));
  t->assertEqual(path, F("/foo/bar"));
}

void testGetFilenameFromFullName(TestInvocation* t) {
  t->setName(F("Extract short file name from filename"));
  char filename[64];
  t->assert(!helper.getFilenameFromFullName(sdStorage, F(""), filename, 64), 
        F("should have failed (empty)"));
  t->assert(!helper.getFilenameFromFullName(sdStorage, F("  "), filename, 64), 
        F("should have failed (whitespace)"));
  t->assert(helper.getFilenameFromFullName(sdStorage, F("bar"), filename, 64), 
        F("getFilenameFromFullName call 1 failed"));
  t->assertEqual(filename, F("bar"));
  t->assert(helper.getFilenameFromFullName(sdStorage, F("/bar"), filename, 64), 
        F("getFilenameFromFullName call 2 failed"));
  t->assertEqual(filename, F("bar"));
  t->assert(helper.getFilenameFromFullName(sdStorage, F("foo/bar"), filename, 64), 
        F("getFilenameFromFullName call 3 failed"));
  t->assertEqual(filename, F("bar"));
  t->assert(helper.getFilenameFromFullName(sdStorage, F("/foo/bar"), filename, 64), 
        F("getFilenameFromFullName call 4 failed"));
  t->assertEqual(filename, F("bar"));
  t->assert(helper.getFilenameFromFullName(sdStorage, F("/foo/bar/baz.txt"), filename, 64), 
        F("getFilenameFromFullName call 5 failed"));
  t->assertEqual(filename, F("baz.txt"));
}

void testCreateTransaction_happyPath(TestInvocation* t) {
  t->setName(F("Initialize a transaction - happy path"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // file1.dat exists (overwriting)
  ts.onExistsReturn[1] = false; // file1's temp file does not exist yet

  Transaction* txn = sdStorage->beginTxn(&ts, F("file1.dat"));
  t->assert(txn, F("beginTxn failed"));
  if (txn) delete txn;
}

void testCreateTransaction_newFileInvalidName(TestInvocation* t) {
  t->setName(F("Initialize a transaction - bad file name"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // new file

  Transaction* txn = sdStorage->beginTxn(&ts, F("???"));
  t->assert(!txn, F("beginTxn should have failed"));
  if (txn) delete txn;
}

void testCreateTransaction_newFileNoSuchDir(TestInvocation* t) {
  t->setName(F("Initialize a transaction - no such path"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // new file
  ts.onExistsReturn[1] = false; // path does not exist

  Transaction* txn = sdStorage->beginTxn(&ts, F("foo/file.dat"));
  t->assert(!txn, F("beginTxn should have failed"));
  if (txn) delete txn;
}

void testCreateTransaction_newFileInvalidPath(TestInvocation* t) {
  t->setName(F("Initialize a transaction - path not a dir"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // new file
  ts.onExistsReturn[1] = true; // path does not exist
  ts.onIsDirectoryReturn = false;

  Transaction* txn = sdStorage->beginTxn(&ts, F("foo/file.dat"));
  t->assert(!txn, F("beginTxn should have failed"));
  if (txn) delete txn;
}

void testCreateTransaction_existingTmpFile(TestInvocation* t) {
  t->setName(F("Initialize a transaction - existing tmp file"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // file1.dat exists (overwriting)
  ts.onExistsReturn[1] = true; // file1's temp file already exists

  Transaction* txn = sdStorage->beginTxn(&ts, F("file1.dat"));
  t->assert(!txn, F("beginTxn should have failed"));
  if (txn) delete txn;
}

void testTransactionalEraseFile_happyPath(TestInvocation* t) {
  t->setName(F("Transactional erase file - happy path"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // file1.dat exists for txn
  ts.onExistsReturn[1] = false; // file1's temp file does not exist yet
  ts.onExistsReturn[2] = true; // file1.dat exists for erase

  Transaction* txn = sdStorage->beginTxn(&ts, F("file1.dat"));
  t->assert(txn, F("beginTxn failed"));
  t->assert(contains(ts.writeTxnDataCaptor.get(), F("file1.dat")), F("expected file1.dat in pre-erase transaction"));
  t->assert(!contains(ts.writeTxnDataCaptor.get(), F("{TOMBSTONE}")), F("unexpected tombstone in transaction"));
  ts.writeTxnDataCaptor.reset();

  t->assert(sdStorage->erase(&ts, F("file1.dat"), txn), F("erase call failed"));
  t->assert(contains(ts.writeTxnDataCaptor.get(), F("file1.dat")), F("expected file1.dat in post-erase transaction"));
  t->assert(contains(ts.writeTxnDataCaptor.get(), F("{TOMBSTONE}")), F("missing tombstone in transaction"));
  if (txn) delete txn;
}

void testTransactionalEraseFile_notInTransaction(TestInvocation* t) {
  t->setName(F("Transactional erase file - not in transaction"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // file1.dat exists for txn
  ts.onExistsReturn[1] = false; // file1's temp file does not exist yet
  ts.onExistsReturn[2] = true; // file3.dat exists for erase

  Transaction* txn = sdStorage->beginTxn(&ts, F("file1.dat"));
  t->assert(txn, F("beginTxn failed"));
  t->assert(!sdStorage->erase(&ts, F("file3.dat"), txn), F("Should have failed as file not part of transaction"));
  if (txn) delete txn;
}

void testAbortTransaction_happyPath(TestInvocation* t) {
  t->setName(F("Abort transaction - happy path"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // file1.dat exists (overwriting)
  ts.onExistsReturn[1] = false; // file1's temp file does not exist yet

  Transaction* txn = sdStorage->beginTxn(&ts, F("file1.dat"));
  t->assert(txn, F("beginTxn failed"));

  ts.onExistsAlways = true;
  ts.onRemoveReturn = true;

  t->assert(sdStorage->abortTxn(txn, &ts), F("abortTxn failed"));
}

void testAbortTransaction_abortFails(TestInvocation* t) {
  t->setName(F("Abort transaction - cannot cleanup tmp file"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // file1.dat exists (overwriting)
  ts.onExistsReturn[1] = false; // file1's temp file does not exist yet

  Transaction* txn = sdStorage->beginTxn(&ts, F("file1.dat"));
  t->assert(txn, F("beginTxn failed"));

  ts.onExistsAlways = true;
  ts.onExistsAlwaysReturn = true; // all tmp files exist
  ts.onRemoveReturn = false; // can't remove tmp file
  t->assert(!sdStorage->abortTxn(txn, &ts), F("abortTxn should have failed"));

}

void testCommitTransaction_happyPath(TestInvocation* t) {
  t->setName(F("Commit transaction - happy path"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // newFile.dat does not exist (new file)
  ts.onExistsReturn[1] = true; // /TESTROOT exists
  ts.onIsDirectoryReturn = true; // /TESTROOT is a directory
  ts.onExistsReturn[2] = false; // newFile's tmp file does not exist yet

  Transaction* txn = sdStorage->beginTxn(&ts, "newFile.dat");
  StreamableDTO newDto;
  t->assert(txn, F("beginTxn failed"));

  ts.onExistsAlways = true;
  ts.onExistsAlwaysReturn = true; // all tmp files exist
  ts.onRenameReturn = true; // .txn file renamed to .cmt, tmpFile renamed to newFile.dat
  ts.onRemoveReturn = true; // .cmt file removed
  t->assert(sdStorage->commitTxn(txn, &ts), F("commitTxn failed"));
  t->assertEqual(ts.renameNewCaptor, F("/TESTROOT/newFile.dat"), F("newFile.dat not written"));
  t->assert(contains(ts.removeCaptor, F(".cmt")) != -1, F(".cmt file should have been last to remove"));
}

void testCommitTransaction_failure(TestInvocation* t) {
  t->setName(F("Commit transaction - failure"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // newFile.dat does not exist (new file)
  ts.onExistsReturn[1] = true; // /TESTROOT exists
  ts.onExistsReturn[2] = false; // newFile's tmp file does not exist yet
  ts.onExistsReturn[3] = true; // newFile's tmp file does exist now
  ts.onExistsReturn[4] = true; // newFile.dat exists (to remove before replace)
  ts.onIsDirectoryReturn = true; // /TESTROOT is a directory

  Transaction* txn = sdStorage->beginTxn(&ts, "newFile.dat");
  StreamableDTO newDto;
  t->assert(txn, F("beginTxn failed"));

  ts.onRenameReturn = false; // cause a failure
  ts.onRemoveReturn = true; // .cmt file removed
  t->assert(!sdStorage->commitTxn(txn, &ts), F("commitTxn failed"));
  t->assert(errThrown, F("Expected errFunction to be invoked"));
}

void testLoadFile(TestInvocation* t) {
  t->setName(F("Load a mock file from a stream"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myFile.dat exists?

  ts.onLoadData = strdup(F("foo=bar\n"));
  StreamableDTO dto;
  t->assert(sdStorage->load(F("myFile.dat"), &dto, &ts), F("Load failed"));
  t->assertEqual(dto.get(F("foo")), F("bar"));
}

void testSaveFile_noTxn(TestInvocation* t) {
  t->setName(F("Save a file without a transaction"));
  MockSdFat::TestState ts;
  ts.onRenameReturn = true;
  ts.onRemoveReturn = true;
  ts.onExistsReturn[0] = false; // /TESTROOT/writeMe.dat exists?
  ts.onExistsReturn[1] = true;  // /TESTROOT exists?
  ts.onIsDirectoryReturn = true; // /TESTROOT is a dir

  StreamableDTO dto;
  dto.put("def", "ghi");
  t->assert(sdStorage->save(&ts, F("writeMe.dat"), &dto), F("Save failed"));
  t->assertEqual(ts.writeDataCaptor.get(), F("def=ghi\n"), F("Unexpected data written"));
  t->assert(endsWith(ts.removeCaptor, F(".cmt")), F("Last file removed should have been .cmt file"));
}

void testIdxFilename(TestInvocation* t) {
  t->setName(F("Index filename"));
  char idxFilename[64];
  t->assert(helper.getIndexFilename(sdStorage, Index(F("foo")), idxFilename, 64), 
        F("getIndexFilename returned false"));
  t->assertEqual(idxFilename, F("/TESTROOT/~IDX/foo.idx"), F("Incorrect index filename"));
}

void testToIndexLine(TestInvocation* t) {
  t->setName(F("Convert IndexEntry to chars"));
  char line[64];
  IndexEntry e0(F("key"), F("value"));
  t->assert(helper.toIndexLine(&e0, line, 64), F("toIndexLine 1 failed"));
  t->assertEqual(line, F("key=value"));
  IndexEntry e1(F("key"));
  t->assert(helper.toIndexLine(&e1, line, 64), F("toIndexLine 2 failed"));
  t->assertEqual(line, F("key="));
  IndexEntry e2(F(""));
  t->assert(!helper.toIndexLine(&e2, line, 64), F("Should have failed"));
}

void testIdxUpsert_firstEntryNoTxn(TestInvocation *t) {
  t->setName(F("Index upsert - firstEntryImplicitTxn"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // myIndex.idx doesn't exist yet (new index)
  ts.onExistsReturn[1] = true; // /TESTROOT/~IDX dir exists
  ts.onIsDirectoryReturn = true; // /TESTROOT/~IDX is a directory
  ts.onExistsReturn[2] = false; // myIndex's tmp file doesn't exist yet
  ts.onRenameReturn = true; // commit txn
  ts.onRemoveReturn = true; // transaction cleanup

  IndexEntry entry(F("fan"), F("1"));
  t->assert(sdStorage->idxUpsert(&ts, Index(F("myIndex")), &entry), F("First entry insert failed"));
  t->assertEqual(ts.writeIdxDataCaptor.get(), F("fan=1\n"), F("Unexpected first index entry written"));
  t->assert(endsWith(ts.removeCaptor, F(".cmt")), F("Last file removed should have been .cmt file"));
}

// void testIdxUpsert(TestInvocation *t) {
//   t->setName(F("Index upsert"));
//   char* idxFilename = sdStorage.indexFilename(F("myIndex"));
//   MockSdFat::TestState ts;
//   ts.onExistsReturn[0] = false; // myIndex.idx doesn't exist yet (new index)
//   ts.onExistsReturn[1] = true; // /TESTROOT/~IDX dir exists
//   ts.onIsDirectoryReturn = true; // /TESTROOT/~IDX is a directory
//   ts.onExistsReturn[2] = false; // myIndex's tmp file doesn't exist yet


//   Transaction* txn = sdStorage.beginTxn(&ts, idxFilename);
//   t->assert(txn, F("Create transaction failed"));
//   t->assert(sdStorage.idxUpsert(&ts, String("myIndex"), String("fan"), String("1"), txn), F("First entry insert failed"));
//   t->assertEqual(ts.writeIdxDataCaptor.getString().c_str(), F("fan=1\n"), F("Unexpected first index entry written"));

//   ts.onExistsAlways = true;
//   ts.onExistsAlwaysReturn = true; // simplify remainder of test
//   ts.writeIdxDataCaptor.reset();
//   ts.onReadIdxData = String("fan=1\n");
//   t->assert(sdStorage.idxUpsert(&ts, String("myIndex"), String("ear"), String("6"), txn), F("Insert first line failed"));
//   t->assertEqual(ts.writeIdxDataCaptor.getString().c_str(), F("ear=6\nfan=1\n"), F("Inserted first line in wrong position"));

//   ts.writeIdxDataCaptor.reset();
//   ts.onReadIdxData = String("ear=6\nfan=1\n");
//   t->assert(sdStorage.idxUpsert(&ts, String("myIndex"), String("egg"), String("12"), txn), F("Insert between failed"));
//   t->assertEqual(ts.writeIdxDataCaptor.getString().c_str(), F("ear=6\negg=12\nfan=1\n"), F("Inserted between in wrong position"));

//   ts.writeIdxDataCaptor.reset();
//   ts.onReadIdxData = String("ear=6\nfan=1\n");
//   t->assert(sdStorage.idxUpsert(&ts, String("myIndex"), String("gum"), String("3"), txn), F("Insert after last failed"));
//   t->assertEqual(ts.writeIdxDataCaptor.getString().c_str(), F("ear=6\nfan=1\ngum=3\n"), F("Inserted after last in wrong position"));


//   ts.writeIdxDataCaptor.reset();
//   ts.onReadIdxData = String("ear=6\nfan=1\n");
//   t->assert(sdStorage.idxUpsert(&ts, String("myIndex"), String("fan"), String("3"), txn), F("Update index entry failed"));
//   t->assertEqual(ts.writeIdxDataCaptor.getString().c_str(), F("ear=6\nfan=3\n"), F("Unexpected index data after update entry"));

//   ts.onRemoveReturn = true;
//   t->assert(txn, F("txn is null"));
//   t->assert(sdStorage.abortTxn(txn, &ts), F("abortTxn failed"));
//   delete[] idxFilename;
// }

void setup() {
  Serial.begin(9600);
  while (!Serial);

  TestFunction tests[] = {
    testBegin,
    testConstructor,
    testCanonicalFilename,
    testMakeDir,
    testFileExists,
    testIsValidFAT16Filename,
    testGetPathFromFilename,
    testGetFilenameFromFullName,
    testCreateTransaction_happyPath,
    testCreateTransaction_newFileInvalidName,
    testCreateTransaction_newFileNoSuchDir,
    testCreateTransaction_newFileInvalidPath,
    testCreateTransaction_existingTmpFile,
    testTransactionalEraseFile_happyPath,
    testTransactionalEraseFile_notInTransaction,
    testAbortTransaction_happyPath,
    testAbortTransaction_abortFails,
    testCommitTransaction_happyPath,
    testCommitTransaction_failure,
    testLoadFile,
    testSaveFile_noTxn,
    testIdxFilename,
    testToIndexLine,
    testIdxUpsert_firstEntryNoTxn
  };

  runTestSuiteShowMem(tests, before, nullptr);

}

void loop() {}