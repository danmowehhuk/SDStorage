#include <SDStorage.h>
#include "MockSdFat.h"
#include "SDStorageTestHelper.h"

#define STR(s) String(F(s))

struct TestInvocation {
  String name = String();
  bool success = false;
  String message = String();
};

extern char __heap_start, *__brkval;

int freeMemory() {
    char top;
    if (__brkval == 0) {
        return &top - &__heap_start;
    } else {
        return &top - __brkval;
    }
}

SDStorage sdStorage(12, reinterpret_cast<const __FlashStringHelper *>(_MOCK_TESTROOT));
SDStorageTestHelper helper;
StreamableManager strManager;

void testBegin(TestInvocation* t) {
  t->name = STR("SDStorage initialization");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // root exists?
  ts.onExistsReturn[1] = false; // workdir exists?
  ts.onExistsReturn[2] = false; // idx dir exists?
  do {
    if (!sdStorage.begin(&ts)) {
      t->message = STR("begin() failed");
      break;
    }
    if (!ts.mkdirCaptor.equals(STR("/TESTROOT/~WORK"))) {
      t->message = STR("Last mkdir didn't match");
      break;
    }
    t->success = true;
  } while (false);
}

void testRealFilename(TestInvocation* t) {
  t->name = STR("Filename resolution");
  do {
    if (!helper.realFilename(&sdStorage, STR("foo")).equals(STR("/TESTROOT/foo"))) {
      t->message = STR("Prepend root dir prefix failed");
      break;
    }
    if (!helper.realFilename(&sdStorage, STR("/foo")).equals(STR("/TESTROOT/foo"))) {
      t->message = STR("Ignore leading slash failed");
      break;
    }
    if (!helper.realFilename(&sdStorage, STR("/TESTROOT/foo")).equals(STR("/TESTROOT/foo"))) {
      t->message = STR("Ignore already under rootDir");
      break;
    }
    t->success = true;
  } while (false);
}

void testLoadFile(TestInvocation* t) {
  t->name = STR("Load a mock file from a stream");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myFile.dat exists?
  ts.onLoadData = String("foo=bar\n");
  StreamableDTO dto;
  do {
    if (!sdStorage.load(STR("myFile.dat"), &dto, &ts)) {
      t->message = STR("Load failed");
      break;
    }
    if (strcmp(dto.get(STR("foo").c_str()),"bar") != 0) {
      t->message = STR("Wrong value for key");
      break;
    }
    t->success = true;
  } while (false);
}

void testSaveIndividualFile(TestInvocation* t) {
  t->name = STR("Save an individual file");
  MockSdFat::TestState ts;
  ts.onRenameReturn = true;
  ts.onRemoveReturn = true;
  ts.onExistsReturn[0] = false; // /TESTROOT/writeMe.dat exists?
  ts.onExistsReturn[1] = true;  // /TESTROOT exists?
  ts.onIsDirectoryReturn = true; // /TESTROOT is a dir

  StreamableDTO dto;
  dto.put("def", "ghi");
  do {
    if (!sdStorage.save(&ts, STR("writeMe.dat"), &dto)) {
      t->message = STR("Save failed");
      break;
    }
    if (!ts.writeDataCaptor.getString().equals("def=ghi\n")) {
      t->message = STR("Unexpected data written");
      break;
    }
    if (!ts.removeCaptor.endsWith(".cmt")) {
      t->message = STR("Last file removed should have been .cmt file");
      break;
    }
    t->success = true;
  } while (false);
}

void testCreateTransaction(TestInvocation* t) {
  t->name = STR("Initialize a transaction");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // file1.dat exists (overwriting)
  ts.onExistsReturn[1] = false; // file1's temp file does not exist yet
  ts.onExistsReturn[2] = false; // file2.dat does not exist (new file)
  ts.onExistsReturn[3] = true; // /TESTROOT exists
  ts.onIsDirectoryReturn = true; // /TESTROOT is a directory
  ts.onExistsReturn[4] = false; // file2's temp file does not exist yet
  ts.onExistsReturn[5] = true; // file1.dat exists to be erased
  ts.onExistsReturn[6] = true; // file3.dat exists to be erased
  ts.onExistsReturn[7] = false; // file2's temp file never written

  Transaction* txn = sdStorage.beginTxn(&ts, "file1.dat", "file2.dat");
  do {
    if (!txn) {
      t->message = STR("beginTxn failed");
      break;
    }
    if (ts.writeTxnDataCaptor.getString().indexOf(STR("{TOMBSTONE}")) != -1) {
      t->message = STR("unexpected tombstone in transaction");
      break;
    }
    ts.onExistsReturn[0] = true; // checking file to erase
    ts.writeTxnDataCaptor.reset();    
    if (!sdStorage.erase(&ts, "file1.dat", txn)) {
      t->message = STR("erase call failed");
      break;
    }
    if (sdStorage.erase(&ts, "file3.dat", txn)) {
      t->message = STR("Should have failed with file not part of transaction");
      break;
    }
    if (ts.writeTxnDataCaptor.getString().indexOf(STR("{TOMBSTONE}")) == -1) {
      t->message = STR("missing tombstone in transaction");
      break;
    }
    ts.onExistsReturn[0] = true;
    t->success = true;
  } while (false);
  ts.onRemoveReturn = true;
  if (txn && !sdStorage.abortTxn(txn, &ts)) {
    t->success = false;
    t->message = STR("abortTxn failed");
  }
}

void testCreateTransactionCleanupFails(TestInvocation* t) {
  t->name = STR("Transaction with pre-existing tmp file");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // file1.dat exists (overwrite)
  ts.onExistsReturn[1] = true; // tmpfile already exists
  Transaction* txn = sdStorage.beginTxn(&ts, "file1.dat");
  do {
    if (txn) {
      t->message = STR("beginTxn should have returned nullptr");
      break;
    }
    t->success = true;
  } while (false);
}

void testCommitTxNewFile(TestInvocation* t) {
  t->name = STR("Commit transaction");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // newFile.dat does not exist (new file)
  ts.onExistsReturn[1] = true; // /TESTROOT exists
  ts.onExistsReturn[2] = false; // newFile's tmp file does not exist yet
  ts.onExistsReturn[3] = true; // newFile's tmp file does exist now
  ts.onExistsReturn[4] = true; // newFile.dat exists (to remove before replace)
  ts.onIsDirectoryReturn = true; // /TESTROOT is a directory
  Transaction* txn = sdStorage.beginTxn(&ts, "newFile.dat");
  StreamableDTO newDto;
  do {
    if (!txn) {
      t->message = STR("beginTxn failed");
      break;
    }
    ts.onRenameReturn = true; // .txn file renamed to .cmt, tmpFile renamed to newFile.dat
    ts.onRemoveReturn = true; // .cmt file removed
    if (!sdStorage.commitTxn(txn, &ts)) {
      t->message = STR("commitTxn failed");
      break;
    }
    if (!ts.renameNewCaptor.equals(STR("/TESTROOT/newFile.dat"))) {
      t->message = STR("newFile.dat not written");
      break;
    }
    if (ts.removeCaptor.indexOf(STR(".cmt")) == -1) {
      t->message = STR(".cmt file should have been last to remove");
      break;
    }
    t->success = true;
  } while (false);
}

void testAbortTx(TestInvocation* t) {
  t->name = STR("Abort transaction");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myFile.dat does not exist (new file)
  ts.onExistsReturn[1] = false; // newFile's tmp file does not exist yet
  Transaction* txn = sdStorage.beginTxn(&ts, "myFile.dat");
  StreamableDTO newDto;
  do {
    if (!txn) {
      t->message = STR("beginTxn failed");
      break;
    }
    ts.onRenameReturn = false; // no renames should happen
    ts.onRemoveReturn = true; // .txn file removed last
    if (!sdStorage.abortTxn(txn, &ts)) {
      t->message = STR("abortTxn failed");
      break;
    }
    if (ts.removeCaptor.indexOf(STR(".txn")) == -1) {
      t->message = STR(".txn file should have been last to remove");
      break;
    }
    t->success = true;
  } while (false);
}

void testIdxFilename(TestInvocation* t) {
  t->name = STR("Index filename");
  String idxFilename = sdStorage.indexFilename(STR("foo"));
  do {
    if (!idxFilename.equals(STR("/TESTROOT/~IDX/foo.idx"))) {
      t->message = STR("Incorrect index filename");
      break;
    }
    t->success = true;
  } while (false);
}

void testIdxUpsert(TestInvocation *t) {
  t->name = STR("Index upsert");
  String idxFilename = sdStorage.indexFilename(STR("myIndex"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // myIndex.idx doesn't exist yet (new index)
  ts.onExistsReturn[1] = true; // /TESTROOT/~IDX dir exists
  ts.onIsDirectoryReturn = true; // /TESTROOT/~IDX is a directory
  ts.onExistsReturn[2] = false; // myIndex's tmp file doesn't exist yet
  Transaction* txn = sdStorage.beginTxn(&ts, idxFilename);
  do {
    if (!txn) {
      t->message = STR("Create transaction failed");
      break;
    }
    if (!sdStorage.idxUpsert(&ts, STR("myIndex"), STR("fan"), STR("1"), txn)) {
      t->message = STR("First entry insert failed");
      break;
    }
    if (!ts.writeIdxDataCaptor.getString().equals(STR("fan=1\n"))) {
      t->message = STR("Unexpected first index entry written");
      break;
    }
    ts.onExistsAlways = true;
    ts.onExistsAlwaysReturn = true; // simplify remainder of test
    ts.writeIdxDataCaptor.reset();
    ts.onReadIdxData = STR("fan=1\n");
    if (!sdStorage.idxUpsert(&ts, STR("myIndex"), STR("ear"), STR("6"), txn)) {
      t->message = STR("Insert first line failed");
      break;
    }
    if (!ts.writeIdxDataCaptor.getString().equals(STR("ear=6\nfan=1\n"))) {
      t->message = STR("Inserted first line in wrong position");
      break;
    }
    ts.writeIdxDataCaptor.reset();
    ts.onReadIdxData = STR("ear=6\nfan=1\n");
    if (!sdStorage.idxUpsert(&ts, STR("myIndex"), STR("egg"), STR("12"), txn)) {
      t->message = STR("Insert between failed");
      break;
    }
    if (!ts.writeIdxDataCaptor.getString().equals(STR("ear=6\negg=12\nfan=1\n"))) {
      t->message = STR("Inserted between in wrong position");
      break;
    }
    ts.writeIdxDataCaptor.reset();
    ts.onReadIdxData = STR("ear=6\nfan=1\n");
    if (!sdStorage.idxUpsert(&ts, STR("myIndex"), STR("gum"), STR("3"), txn)) {
      t->message = STR("Insert after last failed");
      break;
    }
    if (!ts.writeIdxDataCaptor.getString().equals(STR("ear=6\nfan=1\ngum=3\n"))) {
      t->message = STR("Inserted after last in wrong position");
      break;
    }
    ts.writeIdxDataCaptor.reset();
    ts.onReadIdxData = STR("ear=6\nfan=1\n");
    if (!sdStorage.idxUpsert(&ts, STR("myIndex"), STR("fan"), STR("3"), txn)) {
      t->message = STR("Update index entry failed");
      break;
    }
    if (!ts.writeIdxDataCaptor.getString().equals(STR("ear=6\nfan=3\n"))) {
      t->message = STR("Unexpected index data after update entry");
      break;
    }
    t->success = true;
  } while (false);
  ts.onRemoveReturn = true;
  if (txn && !sdStorage.abortTxn(txn, &ts)) {
    t->success = false;
    t->message = STR("Abort transaction failed");
  }
}

void testIdxRemove(TestInvocation *t) {
  t->name = STR("Index remove");
  String idxFilename = sdStorage.indexFilename(STR("myIndex"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists
  ts.onExistsReturn[1] = false; // temp file does not exist yet
  ts.onExistsReturn[2] = true; // myIndex.idx still exists
  ts.onExistsReturn[3] = true; // temp file exists now
  Transaction* txn = sdStorage.beginTxn(&ts, idxFilename);
  do {
    if (!txn) {
      t->message = STR("Create transaction failed");
      break;
    }
    ts.onReadIdxData = STR("ear=3\negg=45\nfan=1\n");
    if (!sdStorage.idxRemove(&ts, STR("myIndex"), STR("ear"), txn)) {
      t->message = STR("Remove key failed");
      break;
    }
    if (!ts.writeIdxDataCaptor.getString().equals(STR("egg=45\nfan=1\n"))) {
      t->message = STR("Unexpected index data after remove key");
      break;
    }
    t->success = true;
  } while (false);
  ts.onRemoveReturn = true;
  if (txn && !sdStorage.abortTxn(txn, &ts)) {
    t->success = false;
    t->message = STR("Abort transaction failed");
  }
}

void testIdxLookup(TestInvocation *t) {
  t->name = STR("Index lookup");
  MockSdFat::TestState ts;
  ts.onExistsAlways = true;
  ts.onExistsAlwaysReturn = true;
  do {
    ts.onReadIdxData = STR("ear=3\negg=45\nfan=1\n");
    String value = sdStorage.idxLookup(STR("myIndex"), STR("egg"), &ts);
    if (value.length() == 0) {
      t->message = STR("Lookup failed");
      break;
    }
    if (!value.equals(STR("45"))) {
      t->message = STR("Incorrect value from index lookup");
      break;
    }
    value = sdStorage.idxLookup(STR("myIndex"), STR("foo"), &ts);
    if (value.length() != 0) {
      t->message = STR("Expected empty string");
      break;
    }
    t->success = true;
  } while (false);
}

void testIdxHasKey(TestInvocation *t) {
  t->name = STR("Index key exists");
  MockSdFat::TestState ts;
  ts.onExistsAlways = true;
  ts.onExistsAlwaysReturn = true;
  do {
    ts.onReadIdxData = STR("ear=3\negg=45\nfan=1\n");
    if (!sdStorage.idxHasKey(STR("myIndex"), STR("ear"), &ts)) {
      t->message = STR("Key should have existed");
      break;
    }
    if (sdStorage.idxHasKey(STR("myIndex"), STR("lap"), &ts)) {
      t->message = STR("Key should not have existed");
      break;
    }
    t->success = true;
  } while (false);
}

void testIdxRenameKey(TestInvocation *t) {
  t->name = STR("Rename index key");
  String idxFilename = sdStorage.indexFilename(STR("myIndex"));
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // myIndex.idx exists
  ts.onExistsReturn[1] = false; // temp file does not exist yet
  Transaction* txn = sdStorage.beginTxn(&ts, idxFilename);
  do {
    if (!txn) {
      t->message = STR("Create transaction failed");
      break;
    }
    ts.onExistsAlways = true;
    ts.onExistsAlwaysReturn = true; // simplify the rest of the test
    ts.onReadIdxData = STR("ear=3\negg=45\nfan=1\n");
    if (!sdStorage.idxRename(&ts, STR("myIndex"), STR("egg"), STR("bag"), txn)) {
      t->message = STR("Rename key failed");
      break;
    }
    if (!ts.writeIdxDataCaptor.getString().equals(STR("bag=45\near=3\nfan=1\n"))) {
      t->message = STR("Unexpected index data after rename key");
      break;
    }
    t->success = true;
  } while (false);
  ts.onRemoveReturn = true;
  if (txn && !sdStorage.abortTxn(txn, &ts)) {
    t->success = false;
    t->message = STR("Abort transaction failed");
  }
}

void testIdxSearchResults(TestInvocation *t) {
  t->name = STR("SearchResults struct");
  SDStorage::SearchResults* sr = new SDStorage::SearchResults(STR("a"));
  do {
    if (!sr->searchPrefix.equals("a")) {
      t->message = STR("Expected 'a' but got '") + sr->searchPrefix + STR("'");
      break;
    }
    t->success = true;
  } while (false);
  delete sr;
}

void testIdxPrefixSearchNoResults(TestInvocation *t) {
  t->name = STR("Index prefix search with no results");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = false; // index file exists
  SDStorage::SearchResults sr(STR("a"));
  do {
    sdStorage.idxPrefixSearch(STR("myIndex"), &sr, &ts);
    if (sr.matchResult) {
      t->message = STR("Should have found no matches");
      break;
    }
    if (sr.trieResult) {
      t->message = STR("Trie results should be empty");
      break;
    }
    if (sr.trieMode) {
      t->message = STR("Should not have switched to trie mode");
      break;
    }
    t->success = true;
  } while (false);
}

void testIdxPrefixSearchEmptySearchString(TestInvocation *t) {
  t->name = STR("Index prefix search with empty search string");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // index file exists
  ts.onReadIdxData = STR("ear=3\negg=45\nera=12\nerf=20\nfan=1\nglob=\n");
  SDStorage::SearchResults sr(STR(""));
  do {
    sdStorage.idxPrefixSearch(STR("myIndex"), &sr, &ts);
    if (!sr.matchResult) {
      t->message = STR("matchResult should be populated");
      break;
    }
    String keys[] = { "ear", "egg", "era", "erf", "fan", "glob" };
    String values[] = { "3", "45", "12", "20", "1", "" };
    SDStorage::KeyValue* kv = sr.matchResult;

    for (uint8_t i = 0; i < 6; i++) {
      if (!kv) {
        t->message = STR("Result should not be nullptr at index ") + String(i);
        break;
      }
      if (!kv->key.equals(keys[i])) {
        t->message = STR("Incorrect key result at index ") + String(i);
        break;
      }
      if (!kv->value.equals(values[i])) {
        t->message = STR("Incorrect value result at index ") + String(i);
        break;
      }
      kv = kv->next;
    }
    if (kv) {
      t->message = STR("Unexpected extra results");
      break;
    }
    t->success = true;
  } while (false);
}


void testIdxPrefixSearchUnder10Matches(TestInvocation *t) {
  t->name = STR("Index prefix search with <10 matches");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // index file exists
  ts.onReadIdxData = STR("ear=3\negg=45\nera=12\nerf=20\nfan=1\nglob=\n");
  SDStorage::SearchResults sr(STR("e"));
  do {
    if (!sr.searchPrefix.equals("e")) {
      t->message = STR("Wrong search prefix passed");
      break;
    }
    sdStorage.idxPrefixSearch(STR("myIndex"), &sr, &ts);
    if (!sr.searchPrefix.equals("e")) {
      t->message = STR("Search prefix changed!");
      break;
    }
    if (sr.trieMode) {
      t->message = STR("Should not have switched to trie mode");
      break;
    }
    if (!sr.matchResult) {
      t->message = STR("matchResult should be populated");
      break;
    }
    if (sr.trieResult != nullptr) {
      t->message = STR("trieResult should not be populated");
      break;
    }
    String keys[] = { "ear", "egg", "era", "erf" };
    String values[] = { "3", "45", "12", "20" };
    SDStorage::KeyValue* kv = sr.matchResult;

    for (uint8_t i = 0; i < 4; i++) {
      if (!kv) {
        t->message = STR("Result should not be nullptr at index ") + String(i);
        break;
      }
      if (!kv->key.equals(keys[i])) {
        t->message = STR("Incorrect key result at index ") + String(i);
        break;
      }
      if (!kv->value.equals(values[i])) {
        t->message = STR("Incorrect value result at index ") + String(i);
        break;
      }
      kv = kv->next;
    }
    if (kv) {
      t->message = STR("Unexpected extra results");
      break;
    }
    t->success = true;
  } while (false);
}

void testIdxPrefixSearchOver10Matches(TestInvocation *t) {
  t->name = STR("Index prefix search with >10 matches");
  MockSdFat::TestState ts;
  ts.onExistsReturn[0] = true; // index file exists
  ts.onReadIdxData = STR("are=1\near=3\neast=23\ned=209\negg=45\nent=65\nera=12\nerf=20\neta=2\netre=98\neva=4\nexit=4\nfan=1\nglob=\n");
  SDStorage::SearchResults sr(STR("e"));
  do {
    sdStorage.idxPrefixSearch(STR("myIndex"), &sr, &ts);
    if (!sr.trieMode) {
      t->message = STR("Should have switched to trie mode");
      break;
    }
    if (sr.matchResult) {
      t->message = STR("matchResult should not be populated");
      break;
    }
    if (!sr.trieResult) {
      t->message = STR("trieResult should be populated");
      break;
    }
    String keys[] = { "a", "d", "g", "n", "r", "t", "v", "x" };
    String values[] = { "", "209", "", "", "", "", "", "" };
    SDStorage::KeyValue* kv = sr.trieResult;

    for (uint8_t i = 0; i < 8; i++) {
      if (!kv) {
        t->message = STR("Result should not be nullptr at index ") + String(i);
        break;
      }
      if (!kv->key.equals(keys[i])) {
        t->message = STR("Incorrect key result at index ") + String(i);
        break;
      }
      if (!kv->value.equals(values[i])) {
        t->message = STR("Incorrect value result at index ") + String(i);
        break;
      }
      kv = kv->next;
    }
    if (kv) {
      t->message = STR("Unexpected extra results");
      break;
    }
    t->success = true;
  } while (false);
}


bool printAndCheckResult(TestInvocation* t) {
  uint8_t nameWidth = 48;
  String name;
  name.reserve(nameWidth);
  name = STR("  ") + t->name + STR("...");
  if (name.length() > nameWidth) {
    name = name.substring(0, nameWidth);
  } else if (name.length() < nameWidth) {
    uint8_t padding = nameWidth - name.length();
    for (uint8_t i = 0; i < padding; i++) {
      name += " ";
    }
  }
  Serial.print(name + " ");
  if (t->success) {
    Serial.println(F("PASSED"));
  } else {
    Serial.println(F("FAILED"));
    if (t->message.length() > 0) {
      Serial.print(F("    "));
      Serial.println(t->message);
    }
    return false;
  }
  return true;
}

bool invokeTest(void (*testFunction)(TestInvocation* t)) {
  TestInvocation t;
  testFunction(&t);
  return printAndCheckResult(&t);
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println(F("Starting SDStorage tests..."));

  void (*tests[])(TestInvocation*) = {

    testBegin,
    testRealFilename,
    testLoadFile,
    testSaveIndividualFile,
    testCreateTransaction,
    testCreateTransactionCleanupFails,
    testCommitTxNewFile,
    testAbortTx,
    testIdxFilename,
    testIdxUpsert,
    testIdxRemove,
    testIdxLookup,
    testIdxHasKey,
    testIdxRenameKey,
    testIdxSearchResults,
    testIdxPrefixSearchNoResults,
    testIdxPrefixSearchEmptySearchString,
    testIdxPrefixSearchUnder10Matches,
    testIdxPrefixSearchOver10Matches

  };

  bool success = true;
  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    // for (uint8_t j = 0; j < 20; j++) {
    // Serial.println("Memory before test: "+String(freeMemory()));
      success &= invokeTest(tests[i]);
    // Serial.println("Memory after test: "+String(freeMemory()));
    // }
  }
  if (success) {
    Serial.println(F("All tests passed!"));
  } else {
    Serial.println(F("Test suite failed!"));
  }
}

void loop() {}
