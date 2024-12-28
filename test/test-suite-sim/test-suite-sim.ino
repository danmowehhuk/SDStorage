#include <SDStorage.h>
#include "MockSdFat.h"
#include "SDStorageTestHelper.h"

#define STR(s) String(F(s))


struct TestInvocation {
  String name = String();
  bool success = false;
  String message = String();
};

SDStorage sdStorage(12, reinterpret_cast<const __FlashStringHelper *>(_MOCK_TESTROOT));
SDStorageTestHelper helper;

void testBegin(TestInvocation* t) {
  t->name = STR("SDStorage initialization");
  MockSdFat::TestState ts;
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
  ts.onExistsReturn = true;
  ts.onLoadData = "foo=bar\n";
  do {
    StreamableDTO dto;
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
  StreamableDTO dto;
  dto.put("def", "ghi");
  do {
    if (!sdStorage.save(STR("writeMe.dat"), &dto, nullptr, &ts)) {
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
    ts.onExistsReturn = true; // checking file to erase
    ts.writeTxnDataCaptor.reset();    
    if (!sdStorage.erase(txn, "file1.dat", &ts)) {
      t->message = STR("erase call failed");
      break;
    }
    if (sdStorage.erase(txn, "file3.dat", &ts)) {
      t->message = STR("Should have failed with file not part of transaction");
      break;
    }
    if (ts.writeTxnDataCaptor.getString().indexOf(STR("{TOMBSTONE}")) == -1) {
      t->message = STR("missing tombstone in transaction");
      break;
    }
    ts.onExistsReturn = true;
    ts.onRemoveReturn = true;
    if (!sdStorage.abortTxn(txn, &ts)) {
      t->message = STR("abortTxn failed");
      break;
    }
    t->success = true;
  } while (false);
}

void testCreateTransactionCleanupFails(TestInvocation* t) {
  t->name = STR("Transaction with pre-existing tmp file");
  MockSdFat::TestState ts;
  ts.onExistsReturn = true; // tmpfile already exists
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
  Transaction* txn = sdStorage.beginTxn(&ts, "newFile.dat");
  StreamableDTO newDto;
  do {
    if (!txn) {
      t->message = STR("beginTxn failed");
      break;
    }
    ts.onExistsReturn = true; // tmpFile was written
    ts.onRenameReturn = true; // tmpFile renamed to newFile.dat
    ts.onRemoveReturn = true; // tx file removed
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
   testIdxFilename

  };

  bool success = true;
  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    success &= invokeTest(tests[i]);
  }
  if (success) {
    Serial.println(F("All tests passed!"));
  } else {
    Serial.println(F("Test suite failed!"));
  }
}

void loop() {}
