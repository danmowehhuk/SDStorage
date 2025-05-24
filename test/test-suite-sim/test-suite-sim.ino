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

void testParseIndexEntry(TestInvocation *t) {
  t->setName(F("Convert line to IndexEntry"));
  IndexEntry entry1 = helper.parseIndexEntry(F(""));
  t->assertEqual(entry1.key, F(""));
  t->assert(!entry1.value, F("Expected value to be nullptr"));

  IndexEntry entry2 = helper.parseIndexEntry(F("myKey"));
  t->assertEqual(entry2.key, F("myKey"), F("Expected key = 'myKey'"));
  t->assert(!entry2.value, F("Expected value to be nullptr"));

  IndexEntry entry3 = helper.parseIndexEntry(F("myKey=myValue"));
  t->assertEqual(entry3.key, F("myKey"));
  t->assertEqual(entry3.value, F("myValue"));

  IndexEntry entry4 = helper.parseIndexEntry(F("  myKey  =   myValue  "));
  t->assertEqual(entry4.key, F("myKey"), F("Expected trimmed key"));
  t->assertEqual(entry4.value, F("myValue"), F("Expected trimmed value"));

  IndexEntry entry5 = helper.parseIndexEntry(F("  = value  "));
  t->assertEqual(entry5.key, F(""), F("Expected empty key"));
  t->assertEqual(entry5.value, F("value"), F("Expected value = 'value'"));
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

void testIdxUpsert_firstEntryWithTxn(TestInvocation *t) {
  t->setName(F("Index upsert - firstEntryWithTxn"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // myIndex.idx doesn't exist yet (new index)
  ts.onExistsReturn[1] = true; // /TESTROOT/~IDX dir exists
  ts.onIsDirectoryReturn = true; // /TESTROOT/~IDX is a directory
  ts.onExistsReturn[2] = false; // myIndex's tmp file doesn't exist yet
  ts.onExistsReturn[3] = false; // myIndex.idx doesn't exist (write first line)

  Index myIdx(F("myIndex"));
  Transaction* txn = sdStorage->beginTxn(&ts, myIdx);
  t->assert(txn, F("Create transaction failed"));

  IndexEntry entry(F("fan"), F("1"));
  t->assert(sdStorage->idxUpsert(&ts, myIdx, &entry, txn), F("First entry insert failed"));
  t->assertEqual(ts.writeIdxDataCaptor.get(), F("fan=1\n"), F("Unexpected first index entry written"));

  ts.onExistsAlways = true; // simplify rest of the test
  ts.onExistsAlwaysReturn = true; // all tmp files exist
  ts.onRenameReturn = true; // commit txn
  ts.onRemoveReturn = true; // transaction cleanup

  t->assert(sdStorage->commitTxn(txn, &ts), F("commitTxn failed"));
  t->assert(endsWith(ts.removeCaptor, F(".cmt")), F("Last file removed should have been .cmt file"));
}

void testIdxUpsert_firstLine(TestInvocation* t) {
  t->setName(F("Index upsert - first line"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists for txn
  ts.onExistsReturn[1] = false; // myIndex's tmp file doesn't exist yet
  ts.onExistsReturn[2] = true; // myIndex.idx exists for index upsert

  Index myIdx(F("myIndex"));
  Transaction* txn = sdStorage->beginTxn(&ts, myIdx);
  t->assert(txn, F("Create transaction failed"));

  ts.onReadIdxData = strdup(F("fan=1\n"));
  IndexEntry entry1(F("ear"), F("6"));
  t->assert(sdStorage->idxUpsert(&ts, myIdx, &entry1, txn), F("Insert first line failed"));
  t->assertEqual(ts.writeIdxDataCaptor.get(), F("ear=6\nfan=1\n"), F("Inserted first line in wrong position"));

  ts.onRemoveReturn = true;
  sdStorage->abortTxn(txn, &ts);
}

void testIdxUpsert_betweenLines(TestInvocation* t) {
  t->setName(F("Index upsert - between lines"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists for txn
  ts.onExistsReturn[1] = false; // myIndex's tmp file doesn't exist yet
  ts.onExistsReturn[2] = true; // myIndex.idx exists for index upsert

  Index myIdx(F("myIndex"));
  Transaction* txn = sdStorage->beginTxn(&ts, myIdx);
  t->assert(txn, F("Create transaction failed"));

  ts.onReadIdxData = strdup(F("ear=6\nfan=1\n"));
  IndexEntry entry1(F("egg"), F("12"));
  t->assert(sdStorage->idxUpsert(&ts, myIdx, &entry1, txn), F("Insert between lines failed"));
  t->assertEqual(ts.writeIdxDataCaptor.get(), F("ear=6\negg=12\nfan=1\n"), F("Inserted between in wrong position"));

  ts.onRemoveReturn = true;
  sdStorage->abortTxn(txn, &ts);
}

void testIdxUpsert_lastLine(TestInvocation* t) {
  t->setName(F("Index upsert - last line"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists for txn
  ts.onExistsReturn[1] = false; // myIndex's tmp file doesn't exist yet
  ts.onExistsReturn[2] = true; // myIndex.idx exists for index upsert

  Index myIdx(F("myIndex"));
  Transaction* txn = sdStorage->beginTxn(&ts, myIdx);
  t->assert(txn, F("Create transaction failed"));

  ts.onReadIdxData = strdup(F("ear=6\nfan=1\n"));
  IndexEntry entry1(F("gum"), F("3"));
  t->assert(sdStorage->idxUpsert(&ts, myIdx, &entry1, txn), F("Insert last line failed"));
  t->assertEqual(ts.writeIdxDataCaptor.get(), F("ear=6\nfan=1\ngum=3\n"), F("Inserted after last in wrong position"));

  ts.onRemoveReturn = true;
  sdStorage->abortTxn(txn, &ts);
}

void testIdxUpsert_updateLine(TestInvocation* t) {
  t->setName(F("Index upsert - update line"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists for txn
  ts.onExistsReturn[1] = false; // myIndex's tmp file doesn't exist yet
  ts.onExistsReturn[2] = true; // myIndex.idx exists for index upsert

  Index myIdx(F("myIndex"));
  Transaction* txn = sdStorage->beginTxn(&ts, myIdx);
  t->assert(txn, F("Create transaction failed"));

  ts.onReadIdxData = strdup(F("ear=6\nfan=1\n"));
  IndexEntry entry1(F("fan"), F("3"));
  t->assert(sdStorage->idxUpsert(&ts, myIdx, &entry1, txn), F("Update index entry failed"));
  t->assertEqual(ts.writeIdxDataCaptor.get(), F("ear=6\nfan=3\n"), F("Unexpected index data after update entry"));

  ts.onRemoveReturn = true;
  sdStorage->abortTxn(txn, &ts);
}

void testIdxRemove(TestInvocation *t) {
  t->setName(F("Index remove"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists for txn
  ts.onExistsReturn[1] = false; // myIndex's tmp file doesn't exist yet
  ts.onExistsReturn[2] = true; // myIndex.idx exists for index upsert

  Index myIdx(F("myIndex"));
  Transaction* txn = sdStorage->beginTxn(&ts, myIdx);
  t->assert(txn, F("Create transaction failed"));

  ts.onReadIdxData = strdup(F("ear=3\negg=45\nfan=1\n"));
  t->assert(sdStorage->idxRemove(&ts, myIdx, F("ear"), txn), F("Remove key failed"));
  t->assertEqual(ts.writeIdxDataCaptor.get(), F("egg=45\nfan=1\n"), F("Unexpected index data after remove key"));

  ts.onRemoveReturn = true;
  sdStorage->abortTxn(txn, &ts);
}

void testIdxRenameKey_happyPath(TestInvocation *t) {
  t->setName(F("Rename index key - happy path"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists
  ts.onExistsReturn[1] = false; // temp file does not exist yet

  Index myIdx(F("myIndex"));
  Transaction* txn = sdStorage->beginTxn(&ts, myIdx);
  t->assert(txn, F("Create transaction failed"));

  ts.onExistsAlways = true;
  ts.onExistsAlwaysReturn = true; // simplify the rest of the test
  ts.onReadIdxData = strdup(F("ear=3\negg=45\nfan=1\n"));
  t->assert(sdStorage->idxRename(&ts, myIdx, F("egg"), F("bag"), txn), F("Rename key failed"));
  t->assertEqual(ts.writeIdxDataCaptor.get(), F("bag=45\near=3\nfan=1\n"), F("Unexpected index data after rename key"));

  ts.onRemoveReturn = true;
  t->assert(txn, F("txn is null"));
  t->assert(sdStorage->abortTxn(txn, &ts), F("abortTxn failed"));
}

void testIdxRenameKey_keyDoesntExist(TestInvocation *t) {
  delay(100);
  t->setName(F("Rename index key - key doesn't exist"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists
  ts.onExistsReturn[1] = false; // temp file does not exist yet

  Index myIdx(F("myIndex"));
  Transaction* txn = sdStorage->beginTxn(&ts, myIdx);
  t->assert(txn, F("Create transaction failed"));

  ts.onExistsAlways = true;
  ts.onExistsAlwaysReturn = true; // simplify the rest of the test
  ts.onReadIdxData = strdup(F("ear=3\negg=45\nfan=1\n"));
  t->assert(!sdStorage->idxRename(&ts, myIdx, F("foo"), F("bag"), txn), F("Rename key should have failed"));

  ts.onRemoveReturn = true;
  t->assert(txn, F("txn is null"));
  t->assert(sdStorage->abortTxn(txn, &ts), F("abortTxn failed"));
}

void testIdxLookup(TestInvocation *t) {
  t->setName(F("Index lookup"));
  MockSdFat::TestState ts;
  ts.onExistsAlways = true;
  ts.onExistsAlwaysReturn = true;
  ts.onReadIdxData = strdup(F("ear=3\negg=45\nfan=1\nbar=\n"));

  Index myIdx(F("myIndex"));
  char buffer[10] = { '\0' };
  t->assert(sdStorage->idxLookup(myIdx, F("egg"), buffer, 64, &ts), F("Scan failed"));
  t->assertEqual(buffer, F("45"));
  char buffer1[10] = { '\0' };
  t->assert(!sdStorage->idxLookup(myIdx, F("foo"), buffer, 10, &ts), F("Expected failure"));
  char buffer2[10] = { '\0' };
  t->assert(sdStorage->idxLookup(myIdx, F("bar"), buffer, 10, &ts), F("Scan failed"));
  t->assert(isEmpty(buffer2), F("Expected empty buffer"));
}

void testIdxHasKey(TestInvocation *t) {
  t->setName(F("Index key exists"));
  MockSdFat::TestState ts;
  ts.onExistsAlways = true;
  ts.onExistsAlwaysReturn = true;

  Index myIdx(F("myIndex"));
  ts.onReadIdxData = strdup(F("ear=3\negg=45\nfan=1\n"));
  t->assert(sdStorage->idxHasKey(myIdx, F("ear"), &ts), F("Key should have existed"));
  t->assert(!sdStorage->idxHasKey(myIdx, F("lap"), &ts), F("Key should not have existed"));
}

void testIdxSearchResults(TestInvocation *t) {
  t->setName(F("SearchResults struct"));
  sdstorage::SearchResults* sr = new sdstorage::SearchResults("a");
  t->assertEqual(sr->searchPrefix, F("a"));
  delete sr;
}

void testIdxPrefixSearch_noResults(TestInvocation *t) {
  t->setName(F("Index prefix search with no results"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // index file exists

  Index myIdx(F("myIndex"));
  sdstorage::SearchResults sr("a");
  t->assert(sdStorage->idxPrefixSearch(myIdx, &sr, &ts), F("idxPrefixSearch failed"));
  t->assert(!sr.matchResult, F("Should have found no matches"));
  t->assert(!sr.trieResult, F("Trie results should be empty"));
  t->assert(!sr.trieMode, F("Should not have switched to trie mode"));
}

void testIdxPrefixSearch_emptySearchString(TestInvocation *t) {
  t->setName(F("Index prefix search with empty search string"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // index file exists
  ts.onReadIdxData = strdup(F("ear=3\negg=45\nera=12\nerf=20\nfan=1\nglob=\n"));

  Index myIdx(F("myIndex"));
  sdstorage::SearchResults sr("");
  sdStorage->idxPrefixSearch(myIdx, &sr, &ts);
  t->assert(sr.matchResult, F("matchResult should be populated"));

  char* keys[] = { "ear", "egg", "era", "erf", "fan", "glob" };
  char* values[] = { "3", "45", "12", "20", "1", "" };
  sdstorage::KeyValue* kv = sr.matchResult;

  for (uint8_t i = 0; i < 6; i++) {
    t->assert(kv, F("Result should not be nullptr"));
    t->assertEqual(kv->key, keys[i], F("Incorrect key result"));
    t->assertEqual(kv->value, values[i], F("Incorrect value result"));
    kv = kv->next;
  }
  t->assert(!kv, F("Unexpected extra results"));
}

void testIdxPrefixSearch_under10Matches(TestInvocation *t) {
  t->setName(F("Index prefix search with <10 matches"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // index file exists
  ts.onReadIdxData = strdup(F("ear=3\negg=45\nera=12\nerf=20\nfan=1\nglob=\n"));

  Index myIdx(F("myIndex"));
  sdstorage::SearchResults sr("e");
  t->assertEqual(sr.searchPrefix, F("e"), F("Wrong search prefix passed"));
  sdStorage->idxPrefixSearch(myIdx, &sr, &ts);
  t->assertEqual(sr.searchPrefix, F("e"), F("Search prefix changed!"));
  t->assert(sr.matchResult, F("matchResult should be populated"));
  t->assert(!sr.trieResult, F("trieResult should not be populated"));
  t->assert(!sr.trieMode, F("Should not have switched to trie mode"));

  char* keys[] = { "ear", "egg", "era", "erf" };
  char* values[] = { "3", "45", "12", "20" };
  sdstorage::KeyValue* kv = sr.matchResult;

  for (uint8_t i = 0; i < 4; i++) {
    t->assert(kv, F("Result should not be nullptr"));
    t->assertEqual(kv->key, keys[i], F("Incorrect key result"));
    t->assertEqual(kv->value, values[i], F("Incorrect value result"));
    kv = kv->next;
  }
  t->assert(!kv, F("Unexpected extra results"));
}

void testIdxPrefixSearch_over10Matches(TestInvocation *t) {
  t->setName(F("Index prefix search with >10 matches"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // index file exists
  ts.onReadIdxData = strdup(F("are=1\near=3\neast=23\ned=209\negg=45\nent=65\nera=12\nerf=20\neta=2\netre=98\neva=4\nexit=4\nfan=1\nglob=\n"));

  Index myIdx(F("myIndex"));
  sdstorage::SearchResults sr("e");
  sdStorage->idxPrefixSearch(myIdx, &sr, &ts);
  t->assert(!sr.matchResult, F("matchResult should not be populated"));
  t->assert(sr.trieResult, F("trieResult should be populated"));
  t->assert(sr.trieMode, F("Should have switched to trie mode"));

  char* keys[] = { "a", "d", "g", "n", "r", "t", "v", "x" };
  char* values[] = { "", "209", "", "", "", "", "", "" };
  sdstorage::KeyValue* kv = sr.trieResult;

  for (uint8_t i = 0; i < 8; i++) {
    t->assert(kv, F("Result should not be nullptr"));
    t->assertEqual(kv->key, keys[i], F("Incorrect key result"));
    t->assertEqual(kv->value, values[i], F("Incorrect value result"));
    kv = kv->next;
  }
  t->assert(!kv, F("Unexpected extra results"));
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
    testSaveFile_noTxn,
    testIdxFilename,
    testParseIndexEntry,
    testToIndexLine,
    testIdxUpsert_firstEntryNoTxn,
    testIdxUpsert_firstEntryWithTxn,
    testIdxUpsert_firstLine,
    testIdxUpsert_betweenLines,
    testIdxUpsert_lastLine,
    testIdxUpsert_updateLine,
    testIdxRemove,
    testIdxRenameKey_happyPath,
    testIdxRenameKey_keyDoesntExist,
    testIdxLookup,
    testIdxHasKey,
    testIdxSearchResults,
    testIdxPrefixSearch_noResults,
    testIdxPrefixSearch_emptySearchString,
    testIdxPrefixSearch_under10Matches,
    testIdxPrefixSearch_over10Matches
  };

  runTestSuiteShowMem(tests, before, nullptr);

}

void loop() {}