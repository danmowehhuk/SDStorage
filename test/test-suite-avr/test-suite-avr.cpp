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

// Hardware SPI is SCK=PB1/MOSI=PB2/MISO=PB3 (verified against
// pins_arduino.h). The SD card's chip-select pin depends on the board,
// so build.sh's -c option passes it in as SD_CS_PORT and SD_CS_BIT.
#if !defined(SD_CS_PORT) || !defined(SD_CS_BIT)
#error "Build with ./build.sh -c <port><bit> (e.g. -c E6) for the SD card's chip-select pin"
#endif
static const uint8_t SD_CS_PIN = pin(Port::SD_CS_PORT, SD_CS_BIT);

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

void testPurge(TestInvocation* t) {
  t->setName(F("purge() empties the root and keeps working dirs"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  StreamableDTO dto;
  dto.put(F("abc"), F("def"));
  t->verify(sdStorage.save(F("purge1.dat"), &dto), F("save failed"));
  t->verify(sdStorage.mkdir(F("purgedir")), F("mkdir failed"));
  t->verify(sdStorage.mkdir(F("purgedir/sub")), F("nested mkdir failed"));
  t->verify(sdStorage.save(F("purgedir/sub/purge2.dat"), &dto), F("nested save failed"));
  if (!t->passed()) return;

  t->verify(sdStorage.purge(), F("purge failed"));
  if (!t->passed()) return;
  t->verify(!sdStorage.exists(F("purge1.dat")), F("purge1.dat not purged"));
  t->verify(!sdStorage.exists(F("purgedir")), F("purgedir not purged"));
  t->verify(sdStorage.exists(F("~IDX")), F("~IDX not recreated"));
  t->verify(sdStorage.exists(F("~WORK")), F("~WORK not recreated"));
  t->verify(sdStorage.exists(F("~SEQ")), F("~SEQ not recreated"));
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

  // idxHasKey()/idxLookup() both scan the index via _scanIndexFrom.
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

// Writes content to path, replacing the file, straight to the card
bool writeCardFile(const char* path, const FlashStr* content) {
  FIL fil;
  if (f_open(&fil, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return false;
  char text[48];
  strncpy_P(text, reinterpret_cast<const char*>(content), sizeof(text) - 1);
  text[sizeof(text) - 1] = '\0';
  UINT written;
  bool success = f_write(&fil, text, strlen(text), &written) == FR_OK && written == strlen(text);
  return f_close(&fil) == FR_OK && success;
}

// Reads path into buffer straight from the card, null-terminated
bool readCardFile(const char* path, char* buffer, size_t bufferSize) {
  FIL fil;
  if (f_open(&fil, path, FA_READ) != FR_OK) return false;
  UINT read;
  bool success = f_read(&fil, buffer, bufferSize - 1, &read) == FR_OK;
  buffer[success ? read : 0] = '\0';
  return f_close(&fil) == FR_OK && success;
}

static const char IDX6_PATH[] = "/TESTROOT/~IDX/idx6.idx";

// A write to idx6 that must fail and leave its lines exactly as they were
void verifyIdx6WriteFails(TestInvocation* t, const FlashStr* lines, bool (*write)(Index)) {
  t->verify(writeCardFile(IDX6_PATH, lines), F("writing the index failed"));
  if (!t->passed()) return;
  t->verify(!write(Index(F("idx6"))), F("the write should have failed"));
  char after[48];
  t->verify(readCardFile(IDX6_PATH, after, sizeof(after)), F("reading the index failed"));
  t->verifyEqual(after, lines, F("the index changed"));
  f_unlink(IDX6_PATH);
}

void testIdxRename_toExistingLaterKey(TestInvocation* t) {
  t->setName(F("idxRename() onto an existing later key fails, keeping the index"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  verifyIdx6WriteFails(t, F("ear=3\negg=45\nfan=1\ngum=7\n"),
      [](Index idx) { return sdStorage.idxRename(idx, "ear", "fan"); });
}

void testIdxRename_toExistingEarlierKey(TestInvocation* t) {
  t->setName(F("idxRename() onto an existing earlier key fails, keeping the index"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  verifyIdx6WriteFails(t, F("ear=3\negg=45\nfan=1\ngum=7\n"),
      [](Index idx) { return sdStorage.idxRename(idx, "fan", "ear"); });
}

void testIdxUpsert_stopsAtEmptyKey(TestInvocation* t) {
  t->setName(F("idxUpsert() past a line with no key fails, keeping the index"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  verifyIdx6WriteFails(t, F("ear=3\n=x\nfan=1\n"), [](Index idx) {
    IndexEntry entry(F("gum"), F("2"));
    return sdStorage.idxUpsert(idx, &entry);
  });
}

void testTransaction_writeAfterFailedRename(TestInvocation* t) {
  t->setName(F("A failed idxRename() leaves the transaction's index intact"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  t->verify(writeCardFile(IDX6_PATH, F("ear=3\negg=45\nfan=1\n")), F("writing the index failed"));
  if (!t->passed()) return;

  Index myIdx(F("idx6"));
  Transaction* txn = sdStorage.beginTxn(myIdx);
  t->verify(txn, F("beginTxn failed"));
  if (!t->passed()) return;
  t->verify(!sdStorage.idxRename(myIdx, "fan", "ear", txn), F("rename onto an earlier key should have failed"));
  t->verify(!sdStorage.idxRename(myIdx, "ear", "fan", txn), F("rename onto a later key should have failed"));
  IndexEntry entry(F("gum"), F("7"));
  t->verify(sdStorage.idxUpsert(myIdx, &entry, txn), F("upsert after the failed renames failed"));
  t->verify(sdStorage.commitTxn(txn), F("commitTxn failed"));
  if (!t->passed()) return;

  char after[48];
  t->verify(readCardFile(IDX6_PATH, after, sizeof(after)), F("reading the index failed"));
  t->verifyEqual(after, F("ear=3\negg=45\nfan=1\ngum=7\n"), F("the committed index is wrong"));
  f_unlink(IDX6_PATH);
}

// An index of the keys key000 to key199, in lines "keyNNN=NNN" (11 bytes
// each, 2200 in all), written straight to the card. It's large enough that
// lookups search it before scanning.
const uint16_t LARGE_INDEX_KEYS = 200;

void testIdx_largeIndex(TestInvocation* t) {
  t->setName(F("Index lookups and prefix search in a large index"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  FIL fil;
  t->verify(f_open(&fil, "/TESTROOT/~IDX/idx5.idx", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK,
      F("opening the index failed"));
  if (!t->passed()) return;
  char line[12]; // Enough for "key199=199\n" + null terminator
  bool allWritten = true;
  for (uint16_t i = 0; i < LARGE_INDEX_KEYS; i++) {
    snprintf_P(line, sizeof(line), PSTR("key%03u=%03u\n"), i, i);
    UINT written;
    allWritten &= f_write(&fil, line, strlen(line), &written) == FR_OK && written == strlen(line);
  }
  f_close(&fil);
  t->verify(allWritten, F("writing the index failed"));
  if (!t->passed()) return;

  Index myIdx(F("idx5"));
  char key[8]; // Enough for "key199x" + null terminator
  bool everyKeyFound = true;
  bool noKeyBetweenFound = true;
  for (uint16_t i = 0; i < LARGE_INDEX_KEYS; i++) {
    snprintf_P(key, sizeof(key), PSTR("key%03u"), i);
    everyKeyFound &= sdStorage.idxHasKey(myIdx, key);
    snprintf_P(key, sizeof(key), PSTR("key%03ux"), i);
    noKeyBetweenFound &= !sdStorage.idxHasKey(myIdx, key);
  }
  t->verify(everyKeyFound, F("a key in the index wasn't found"));
  t->verify(noKeyBetweenFound, F("a key between two keys was found"));
  t->verify(!sdStorage.idxHasKey(myIdx, "a"), F("a key before the first was found"));
  char value[4]; // Enough for "137" + null terminator
  t->verify(sdStorage.idxLookup(myIdx, "key137", value, sizeof(value)), F("lookup failed"));
  t->verifyEqual(value, F("137"));

  sdstorage::SearchResults results("key05");
  t->verify(sdStorage.idxPrefixSearch(myIdx, &results), F("prefix search failed"));
  t->verify(!results.trieMode, F("prefix search switched to trie mode"));
  uint8_t matches = 0;
  for (sdstorage::KeyValue* kv = results.matchResult; kv; kv = kv->next) {
    snprintf_P(key, sizeof(key), PSTR("key05%u"), matches);
    t->verifyEqual(kv->key, key, F("wrong prefix match"));
    matches++;
  }
  t->verify(matches == 10, F("prefix search should match key050 to key059"));

  // cleanup
  f_unlink("/TESTROOT/~IDX/idx5.idx");
}

void testTransaction_erase(TestInvocation* t) {
  t->setName(F("erase() in a transaction deletes on commit"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  StreamableDTO dto;
  dto.put(F("abc"), F("def"));
  t->verify(sdStorage.save(F("txnerase.dat"), &dto), F("save failed"));
  if (!t->passed()) return;

  Transaction* txn = sdStorage.beginTxn(F("txnerase.dat"));
  t->verify(txn, F("beginTxn failed"));
  if (!t->passed()) return;
  t->verify(sdStorage.erase(F("txnerase.dat"), txn), F("erase failed"));
  t->verify(sdStorage.commitTxn(txn), F("commit failed"));
  t->verify(!sdStorage.exists(F("txnerase.dat")), F("txnerase.dat not deleted"));
}

void testIdxRename_toLastKey(TestInvocation* t) {
  t->setName(F("idxRename() to a key that sorts last"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  f_unlink("/TESTROOT/~IDX/idx4.idx");

  Index myIdx(F("idx4"));
  IndexEntry entryA(F("abc"), F("1"));
  IndexEntry entryB(F("def"), F("2"));
  t->verify(sdStorage.idxUpsert(myIdx, &entryA), F("upsert abc failed"));
  t->verify(sdStorage.idxUpsert(myIdx, &entryB), F("upsert def failed"));
  if (!t->passed()) return;

  t->verify(sdStorage.idxRename(myIdx, "abc", "xyz"), F("rename failed"));
  t->verify(!sdStorage.idxHasKey(myIdx, "abc"), F("abc still present"));
  t->verify(sdStorage.idxHasKey(myIdx, "def"), F("def lost"));
  char buf[10];
  t->verify(sdStorage.idxLookup(myIdx, "xyz", buf, 10), F("xyz not found"));
  if (t->passed()) t->verifyEqual(buf, F("1"));

  // cleanup
  f_unlink("/TESTROOT/~IDX/idx4.idx");
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

void testSeqPeekNext_matchesNextInTxn(TestInvocation* t) {
  t->setName(F("seqPeekNext() names the value seqNext() then returns in a transaction"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  f_unlink("/TESTROOT/~SEQ/seq3.seq");

  Sequence mySeq(F("seq3"));
  uint64_t peeked = sdStorage.seqPeekNext(mySeq);
  t->verify(peeked == 1, F("a new sequence should peek 1"));
  t->verify(!sdStorage.exists(F("~SEQ/seq3.seq")), F("peeking should not create the sequence"));
  Transaction* txn = sdStorage.beginTxn(mySeq);
  t->verify(txn, F("beginTxn failed"));
  if (!t->passed()) return;
  t->verify(sdStorage.seqNext(mySeq, txn) == peeked, F("seqNext should return the peeked value"));
  t->verify(sdStorage.commitTxn(txn), F("commitTxn failed"));
  t->verify(sdStorage.seqPeekNext(mySeq) == 2, F("should peek 2 after the commit"));
  t->verify(sdStorage.seqCurrent(mySeq) == 1, F("peeking should not advance the sequence"));

  f_unlink("/TESTROOT/~SEQ/seq3.seq");
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

void testTransaction_repeatFileEdit(TestInvocation* t) {
  t->setName(F("Edit the same file twice in one transaction, interleaved with another file"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  sdStorage.erase(F("file2.dat"));
  sdStorage.erase(F("file3.dat"));

  Transaction* txn = sdStorage.beginTxn(F("file2.dat"), F("file3.dat"));
  t->verify(txn, F("beginTxn failed"));
  if (!t->passed()) return;

  StreamableDTO dtoA1;
  dtoA1.put(F("step"), F("1"));
  t->verify(sdStorage.save(F("file2.dat"), &dtoA1, txn), F("first save of file2 failed"));
  if (!t->passed()) return;

  StreamableDTO dtoB;
  dtoB.put(F("b"), F("1"));
  t->verify(sdStorage.save(F("file3.dat"), &dtoB, txn), F("save of file3 failed"));
  if (!t->passed()) return;

  // Reload file2 through the still-open transaction - this must see the
  // first save's content, not the pristine original.
  StreamableDTO dtoA2;
  t->verify(sdStorage.load(F("file2.dat"), &dtoA2, nullptr, txn), F("reload of file2 within the transaction failed"));
  if (!t->passed()) return;
  t->verifyEqual(dtoA2.get(F("step")), F("1"), F("reload within the transaction lost the first edit"));
  dtoA2.put(F("step"), F("2"));
  t->verify(sdStorage.save(F("file2.dat"), &dtoA2, txn), F("second save of file2 failed"));
  if (!t->passed()) return;

  t->verify(sdStorage.commitTxn(txn), F("commitTxn failed"));
  if (!t->passed()) return;

  StreamableDTO finalA;
  t->verify(sdStorage.load(F("file2.dat"), &finalA), F("final load of file2 failed"));
  if (!t->passed()) return;
  t->verifyEqual(finalA.get(F("step")), F("2"), F("committed file2 does not reflect both edits"));
  StreamableDTO finalB;
  t->verify(sdStorage.load(F("file3.dat"), &finalB), F("final load of file3 failed"));
  t->verifyEqual(finalB.get(F("b")), F("1"), F("committed file3 is incorrect"));

  // cleanup
  sdStorage.erase(F("file2.dat"));
  sdStorage.erase(F("file3.dat"));
}

void testTransaction_repeatFileAndIndexEdits(TestInvocation* t) {
  t->setName(F("Repeated file and index edits, interleaved, in one transaction"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  f_unlink("/TESTROOT/~IDX/idx2.idx");
  sdStorage.erase(F("file4.dat"));
  sdStorage.erase(F("file5.dat"));

  Index myIdx(F("idx2"));
  Transaction* txn = sdStorage.beginTxn(myIdx, F("file4.dat"), F("file5.dat"));
  t->verify(txn, F("beginTxn failed"));
  if (!t->passed()) return;

  StreamableDTO dtoA;
  dtoA.put(F("n"), F("1"));
  t->verify(sdStorage.save(F("file4.dat"), &dtoA, txn), F("save of file4 (edit 1) failed"));
  if (!t->passed()) return;
  IndexEntry entryA(F("file4"), F("1"));
  t->verify(sdStorage.idxUpsert(myIdx, &entryA, txn), F("first index upsert failed"));
  if (!t->passed()) return;

  StreamableDTO dtoB;
  dtoB.put(F("n"), F("1"));
  t->verify(sdStorage.save(F("file5.dat"), &dtoB, txn), F("save of file5 failed"));
  if (!t->passed()) return;

  StreamableDTO dtoA2;
  t->verify(sdStorage.load(F("file4.dat"), &dtoA2, nullptr, txn), F("reload of file4 within the transaction failed"));
  if (!t->passed()) return;
  t->verifyEqual(dtoA2.get(F("n")), F("1"), F("reload of file4 within the transaction lost the first edit"));
  dtoA2.put(F("n"), F("2"));
  t->verify(sdStorage.save(F("file4.dat"), &dtoA2, txn), F("save of file4 (edit 2) failed"));
  if (!t->passed()) return;
  IndexEntry entryA2(F("file4"), F("2"));
  t->verify(sdStorage.idxUpsert(myIdx, &entryA2, txn), F("second index upsert (same key) failed"));
  if (!t->passed()) return;

  t->verify(sdStorage.commitTxn(txn), F("commitTxn failed"));
  if (!t->passed()) return;

  StreamableDTO finalA;
  t->verify(sdStorage.load(F("file4.dat"), &finalA), F("final load of file4 failed"));
  if (!t->passed()) return;
  t->verifyEqual(finalA.get(F("n")), F("2"), F("committed file4 does not reflect both edits"));
  StreamableDTO finalB;
  t->verify(sdStorage.load(F("file5.dat"), &finalB), F("final load of file5 failed"));
  t->verifyEqual(finalB.get(F("n")), F("1"), F("committed file5 is incorrect"));
  char buf[10];
  t->verify(sdStorage.idxLookup(myIdx, "file4", buf, 10), F("index lookup failed"));
  t->verifyEqual(buf, F("2"), F("index does not reflect the second upsert"));

  // cleanup
  f_unlink("/TESTROOT/~IDX/idx2.idx");
  sdStorage.erase(F("file4.dat"));
  sdStorage.erase(F("file5.dat"));
}

void testTransaction_repeatFileIndexAndSequenceEdits(TestInvocation* t) {
  t->setName(F("Repeated file, index, and sequence edits, interleaved, in one transaction"));
  t->verify(beginSuccess, F("SKIPPED"));
  if (!t->passed()) return;
  f_unlink("/TESTROOT/~IDX/idx3.idx");
  f_unlink("/TESTROOT/~SEQ/seq2.seq");
  sdStorage.erase(F("file6.dat"));
  sdStorage.erase(F("file7.dat"));

  // This is the original motivating scenario for this whole fix: a
  // transaction spanning two files, one index, and one sequence, where
  // the sequence is advanced twice and the index is upserted twice (once
  // per advance) - getReadSource() has to keep every one of those staged
  // edits straight, interleaved, in a single still-open transaction.
  Index myIdx(F("idx3"));
  Sequence mySeq(F("seq2"));
  Transaction* txn = sdStorage.beginTxn(mySeq, myIdx, F("file6.dat"), F("file7.dat"));
  t->verify(txn, F("beginTxn failed"));
  if (!t->passed()) return;

  // First sequence advance, tagging file6 with the resulting id.
  uint64_t seq1 = sdStorage.seqNext(mySeq, txn);
  t->verify(seq1 == 1, F("first seqNext failed or returned the wrong value"));
  if (!t->passed()) return;
  StreamableDTO dtoA;
  dtoA.put(F("seqId"), F("1"));
  t->verify(sdStorage.save(F("file6.dat"), &dtoA, txn), F("save of file6 (edit 1) failed"));
  if (!t->passed()) return;
  // seq1 == 1, just asserted above - index the file by its (string) id.
  IndexEntry entryA(F("file6"), F("1"));
  t->verify(sdStorage.idxUpsert(myIdx, &entryA, txn), F("first index upsert failed"));
  if (!t->passed()) return;

  StreamableDTO dtoB;
  dtoB.put(F("n"), F("1"));
  t->verify(sdStorage.save(F("file7.dat"), &dtoB, txn), F("save of file7 failed"));
  if (!t->passed()) return;

  // Second sequence advance on the same still-open transaction - must
  // see 2, not silently repeat 1. Re-tag file6 with the new id.
  uint64_t seq2 = sdStorage.seqNext(mySeq, txn);
  t->verify(seq2 == 2, F("second seqNext on the same transaction failed or returned the wrong value"));
  if (!t->passed()) return;
  StreamableDTO dtoA2;
  t->verify(sdStorage.load(F("file6.dat"), &dtoA2, nullptr, txn), F("reload of file6 within the transaction failed"));
  if (!t->passed()) return;
  t->verifyEqual(dtoA2.get(F("seqId")), F("1"), F("reload of file6 within the transaction lost the first edit"));
  dtoA2.put(F("seqId"), F("2"));
  t->verify(sdStorage.save(F("file6.dat"), &dtoA2, txn), F("save of file6 (edit 2) failed"));
  if (!t->passed()) return;
  // seq2 == 2, just asserted above.
  IndexEntry entryA2(F("file6"), F("2"));
  t->verify(sdStorage.idxUpsert(myIdx, &entryA2, txn), F("second index upsert (same key) failed"));
  if (!t->passed()) return;

  t->verify(sdStorage.commitTxn(txn), F("commitTxn failed"));
  if (!t->passed()) return;

  t->verify(sdStorage.seqCurrent(mySeq) == 2, F("committed sequence does not reflect both advances"));
  StreamableDTO finalA;
  t->verify(sdStorage.load(F("file6.dat"), &finalA), F("final load of file6 failed"));
  if (!t->passed()) return;
  t->verifyEqual(finalA.get(F("seqId")), F("2"), F("committed file6 does not reflect both edits"));
  StreamableDTO finalB;
  t->verify(sdStorage.load(F("file7.dat"), &finalB), F("final load of file7 failed"));
  t->verifyEqual(finalB.get(F("n")), F("1"), F("committed file7 is incorrect"));
  char buf[10];
  t->verify(sdStorage.idxLookup(myIdx, "file6", buf, 10), F("index lookup failed"));
  t->verifyEqual(buf, F("2"), F("index does not reflect the second upsert"));

  // cleanup
  f_unlink("/TESTROOT/~IDX/idx3.idx");
  f_unlink("/TESTROOT/~SEQ/seq2.seq");
  sdStorage.erase(F("file6.dat"));
  sdStorage.erase(F("file7.dat"));
}

int main() {
  Uart0::begin(9600);
  timingInit();
  // PB0 is this chip's hardware SS pin, so it needs its own OUTPUT
  // configuration even when the card's CS is another pin - and it must happen BEFORE
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
    testPurge,
    testIndexUpsertLookupRemove,
    testIdxRename_toLastKey,
    testIdxRename_toExistingLaterKey,
    testIdxRename_toExistingEarlierKey,
    testIdxUpsert_stopsAtEmptyKey,
    testTransaction_writeAfterFailedRename,
    testIdx_largeIndex,
    testTransaction_erase,
    testSeqCurrentNext,
    testSeqPeekNext_matchesNextInTxn,
    testFsckRecoversStaleTransaction,
    testTransaction_repeatFileEdit,
    testTransaction_repeatFileAndIndexEdits,
    testTransaction_repeatFileIndexAndSequenceEdits
  };

  runTestSuiteShowMem(tests, before);

  while (true) {}
  return 0;
}
