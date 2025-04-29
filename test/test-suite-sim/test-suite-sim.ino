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
  char* resolvedName = helper.canonicalFilename(sdStorage, F("foo"));
  t->assertEqual(resolvedName, F("/TESTROOT/foo"), F("Prepend root dir prefix failed"));
  delete[] resolvedName;
  resolvedName = nullptr;
  resolvedName = helper.canonicalFilename(sdStorage, F("/foo"));
  t->assertEqual(resolvedName, F("/TESTROOT/foo"), F("Ignore leading slash failed"));
  delete[] resolvedName;
  resolvedName = nullptr;
  resolvedName = helper.canonicalFilename(sdStorage, F("/TESTROOT/foo"));
  t->assertEqual(resolvedName, F("/TESTROOT/foo"), F("Ignore already under rootDir"));
  delete[] resolvedName;
  resolvedName = nullptr;
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
  char* path = helper.getPathFromFilename(sdStorage, F(""));
  t->assert(!path, F("Empty filename"));
  if (path) delete[] path;
  path = nullptr;
  path = helper.getPathFromFilename(sdStorage, F("  "));
  t->assert(!path, F("Filename only spaces"));
  if (path) delete[] path;
  path = nullptr;
  path = helper.getPathFromFilename(sdStorage, F("bar"));
  t->assert(!path, F("No path component"));
  if (path) delete[] path;
  path = nullptr;
  path = helper.getPathFromFilename(sdStorage, F("/bar"));
  t->assertEqual(path, F("/"));
  if (path) delete[] path;
  path = nullptr;
  path = helper.getPathFromFilename(sdStorage, F("foo/bar"));
  t->assertEqual(path, F("foo"));
  if (path) delete[] path;
  path = nullptr;
  path = helper.getPathFromFilename(sdStorage, F("/foo/bar"));
  t->assertEqual(path, F("/foo"));
  if (path) delete[] path;
  path = nullptr;
  path = helper.getPathFromFilename(sdStorage, F("/foo/bar/baz.txt"));
  t->assertEqual(path, F("/foo/bar"));
  if (path) delete[] path;
  path = nullptr;
}

void testGetFilenameFromFullName(TestInvocation* t) {
  t->setName(F("Extract short file name from filename"));
  char* filename = helper.getFilenameFromFullName(sdStorage, F(""));
  t->assert(!filename, F("Empty filename"));
  if (filename) delete[] filename;
  filename = nullptr;
  filename = helper.getFilenameFromFullName(sdStorage, F("  "));
  t->assert(!filename, F("Filename only spaces"));
  if (filename) delete[] filename;
  filename = nullptr;
  filename = helper.getFilenameFromFullName(sdStorage, F("bar"));
  t->assertEqual(filename, F("bar"));
  if (filename) delete[] filename;
  filename = nullptr;
  filename = helper.getFilenameFromFullName(sdStorage, F("/bar"));
  t->assertEqual(filename, F("bar"));
  if (filename) delete[] filename;
  filename = nullptr;
  filename = helper.getFilenameFromFullName(sdStorage, F("foo/bar"));
  t->assertEqual(filename, F("bar"));
  if (filename) delete[] filename;
  filename = nullptr;
  filename = helper.getFilenameFromFullName(sdStorage, F("/foo/bar"));
  t->assertEqual(filename, F("bar"));
  if (filename) delete[] filename;
  filename = nullptr;
  filename = helper.getFilenameFromFullName(sdStorage, F("/foo/bar/baz.txt"));
  t->assertEqual(filename, F("baz.txt"));
  if (filename) delete[] filename;
  filename = nullptr;
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
  ts.onExistsReturn[2] = false; // newFile's tmp file does not exist yet
  ts.onExistsReturn[3] = true; // newFile's tmp file does exist now
  ts.onExistsReturn[4] = true; // newFile.dat exists (to remove before replace)
  ts.onIsDirectoryReturn = true; // /TESTROOT is a directory

  Transaction* txn = sdStorage->beginTxn(&ts, "newFile.dat");
  StreamableDTO newDto;
  t->assert(txn, F("beginTxn failed"));

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
    testSaveFile_noTxn
  };

  runTestSuiteShowMem(tests, before, nullptr);

}

void loop() {}