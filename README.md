# SDStorage

## Introduction

**SDStorage** is a C++ library for Arduino that provides an easy way to persist and manage structured data objects (DTOs) on an SD card. It is built to work seamlessly with [**StreamableDTO**](https://github.com/danmowehhuk/StreamableDTO) objects, allowing you to save and load DTOs to files. In addition, SDStorage offers powerful features like native indexing for efficient record lookup and support for transactions to ensure data integrity when updating multiple objects at once.

**Key Features:**

- **[StreamableDTO](https://github.com/danmowehhuk/StreamableDTO) Persistence:** Designed specifically to save and load StreamableDTO objects to SD cards.
- **Native Indexes:** Create one or more indexes on any string-based key/value data (e.g., a field of your DTO). All index operations run in O(n) time but require only O(1) memory, making them well-suited for low-memory embedded systems.
- **Prefix Searches:** Indexes support efficient prefix-based lookups. This allows you to retrieve all entries with keys matching a given prefix, which is perfect for building UI features like auto-complete (e.g., ComboBoxes that suggest entries as you type) or implementing trie-based searches.
- **Atomic Transactions:** SDStorage supports transactional updates, meaning you can group multiple operations (such as saving a DTO and updating several indexes) into one atomic unit.

## Installation

**Prerequisites:** SDStorage relies on the [**StreamableDTO**](https://github.com/danmowehhuk/StreamableDTO) library for DTO definitions, and [SdFat](https://github.com/greiman/SdFat) to interface with an SD card.

**Using the Library Manager (Arduino IDE):** Simply open the Arduino Library Manager, search for "SDStorage" and install it. (If the library is not yet in the Library Manager, you can install it manually as described below.) 

**Manual Installation:** Clone or download the SDStorage repository from GitHub. If downloading a ZIP, open Arduino IDE and go to Sketch > Include Library > Add .ZIP Library... and select the downloaded SDStorage.zip file. Alternatively, copy the SDStorage folder into your Arduino libraries directory. 

Once installed, you can include SDStorage in your sketch like any other library:
```cpp
#include <SDStorage.h>
#include <StreamableDTO.h>
```

> NOTE: SDStorage uses the FAT16 file format, so all filenames and directory names are case-insensitive
> and must conform to the 8.3 format (i.e., up to 8 characters for the name, and 3 for the extension).

## Basic Usage: Saving and Loading DTOs

Using SDStorage is straightforward for basic persistence of DTOs. First, initialize the SD card and the SDStorage system, then use the `save(...)` and `load(...)` methods to write to or read from files on the SD card.

**Initialization:** Create an SDStorage instance, specifying the SD chip select pin and a root directory name where data files will be kept. The boolean argument indicates that the root directory name is stored in flash (`PROGMEM`) versus RAM. For example:

```cpp
#define SD_CS_PIN    10                       // SD card chip select pin
static const char SD_ROOT[] PROGMEM = "DATA"; // Root folder for SDStorage files

// Optional error handling function (called on critical errors)
void haltOnError() {
    Serial.println(F("Critical SD error. Halting."));
    while(true) {}  // halt system
}

// Initialize SDStorage
SDStorage sdStorage(SD_CS_PIN, SD_ROOT, true, haltOnError);
```

In the above snippet, `SD_ROOT` is the name of the directory on the SD card where SDStorage will store all its files (e.g., `DATA`). You don't need to prepend any filenames with the root directory name. That will happen automatically.

After constructing the object, initialize it in your `setup()` by calling `sdStorage.begin()`. This mounts the SD card and prepares the storage system:

```cpp
if (!sdStorage.begin()) {
    Serial.println(F("SDStorage init failed"));
}
```

**Saving a DTO:** Once initialized, saving a DTO to a file with `save(filename, dto)`. For example, using an untyped StreamableDTO or your custom DTO class:

```cpp
StreamableDTO config;
config.put("deviceName", "SensorA");
config.put("threshold", "42");
// ... populate DTO fields ...

const char* filename = "config1.dat";  // file will be "/DATA/config1.dat" on SD
if (sdStorage.save(filename, &config)) {
    Serial.println(F("DTO saved successfully!"));
} else {
    Serial.println(F("Failed to save DTO."));
}
```

**Loading a DTO:** To retrieve data, use `load(filename, dto)`. Provide the same file name and a DTO object to load into:

```cpp
StreamableDTO configCopy;
if (sdStorage.load(filename, &configCopy)) {
    Serial.println(F("DTO loaded!"));
    Serial.println(configCopy.get("deviceName"));  // Should print "SensorA"
    Serial.println(configCopy.get("threshold"));   // Should print "42"
}
```

**Checking for Existence and Deletion:** You can check if a given record (file) exists using `exists(filename)` and remove a record with `erase(filename)`:

```cpp
if (sdStorage.exists(filename)) {
    sdStorage.erase(filename);
    Serial.println(F("Record deleted."));
}
```

For more details on basic save/load usage, see the [`basic` example](/examples/basic/basic.ino).

## Using Indexes

In addition to record storage and retrieval, SDStorage allows you to create indexes on your data, which function like a simple database index or key-value store for quick lookups. An index maps string keys to string values of your choosing. In typical use, you might use an index to map a field in your DTO (like an ID or searchable name) to the filename of the record containing that DTO.

**Creating an Index:** To use indexes, first define an `Index` object with a name. The name uniquely identifies the index and is also used as a filename under the hood, so must adhere to FAT16 naming rules (except with no file extension):

```cpp
#include <Index.h>

sdstorage::Index idIndex(F("id_idx"));  // create an index named "id_idx"
```

Here we use `F("...")` to place the index name in `PROGMEM` (flash) to save RAM. You could also use a regular string. Defining an index does not create or populate storage—it simply declares that the index exists.

**Index Entries:** To add or update an entry in the index, use `idxUpsert(...)`. For example, if you were indexing device records by ID:

```cpp
sdstorage::IndexEntry entry(F("1234"), F("devices/a_device.dat")); // deviceId, filename
if (sdStorage.idxUpsert(idIndex, &entry)) {
    Serial.println(F("Index updated"));
} else {
    Serial.println(F("Index update failed"));
}
```

**Index Lookup:** To retrieve the value for a given key, use `idxLookup(...)`:

```cpp
char valueBuffer[32];
if (sdStorage.idxLookup(idIndex, "1234", valueBuffer, sizeof(valueBuffer))) {
    Serial.print(F("Record for 1234 is: "));
    Serial.println(valueBuffer);  // e.g., prints "devices/a_device.dat"
}
```

**Existence, Renaming and Removing Keys**:
- `idxHasKey(index, key)` returns `true` if the given key exists in the index.
- `idxRemove(index, key)` deletes the entry for that key from the index.
- `idxRename(index, oldKey, newKey)` allows you to change the key for an entry (the new key cannot already exist).

Example:

```cpp
if (sdStorage.idxHasKey(idIndex, "12345")) {
    sdStorage.idxRemove(idIndex, "12345");
    Serial.println(F("Removed index entry for 12345."));
}
```

**Under the Hood:** Index files are maintained on the SD card in a way that allows for searches in O(n) time, but O(1) memory. Keys are sorted and always scanned in ascending order. You can have multiple indexes for different keys (for example, one index by device ID, another by device name, etc.). Each index is independent and identified by its name. For more details, see the [`index` example](/examples/index/index.ino).

## Prefix Searches for Autocomplete

In addition to exact lookups, SDStorage’s indexing system supports prefix search on keys. This is useful for implementing features like auto-complete or combo-boxes in user interfaces.

**Using Prefix Search:** To perform a prefix search, you’ll use `idxPrefixSearch(...)`. 

**Example:** Suppose your index contains keys for names: `{"alice" -> "file1", "albert" -> "file2", "bob" -> "file3", "bryan" -> "file4", ...}`. If you want to get suggestions for names beginning with "al":

```cpp
#include <Index.h>

sdstorage::Index nameIndex(F("name_idx"));

char prefix[] = "al";
sdstorage::SearchResults results(prefix);
if (sdStorage.idxPrefixSearch(nameIndex, &results)) {
    Serial.print(F("Number of results: "));
    Serial.println(results.matchCount);
    sdstorage::KeyValue* kv = &results.matchResult[0];
    if (!results.trieMode) {
      while (kv) {
        Serial.print(kv->key);
        Serial.print(": ");
        if (kv->value) {             // might be 'nullptr' for an empty value
          Serial.println(kv->value);
        } else {
          Serial.println();
        }
        kv = kv->next;
      }
    }
}
```

**Trie Mode:** If the number of matches is 10 or greater, SDStorage switches to trie mode. In trie mode, instead of returning all the matching keys, the `matchResult` field will be an array of next possible characters. Be sure to check if `trieMode` is `true` before processing search results. 

For more details, see the [`search` example](/examples/search/search.ino). That sketch populates an index with sample data and demonstrates two scenarios: one where the prefix is specific enough to get a full list of results, and another where the prefix is broad (many results) triggering the trie mode behavior. The example shows how to handle both cases in your code.


## Transactions: Atomic Updates

`SDStorage` can perform atomic updates (all succeeding or all failing) of multiple files and/or indexes with transactions. If power is lost during a write operation, `SDStorage` will try to complete the transaction on restart, or it will abort and clean up, leaving everything unchanged. Even if you don’t use transactions explicitly, SDStorage wraps each individual write in an implicit transaction, allowing recovery from partial writes on restart.

You can have multiple transactions at the same time, but they do not nest. Also, they cannot include any of the same files or indexes. Trying to begin a transaction with a file or index that is already part of another transaction will cause the system to hang.

Currently, only one index operation can be performed per index within a transaction.

**Beginning a Transaction:** To start a transaction, call `sdStorage.beginTxn(...)` passing all the filenames and `Index`es you plan to change. This returns a `Transaction*` that you will need to pass to any write operations you want to be part of the transaction:

```cpp
const char* filename = "config1.dat";
Index myIdx("my_index");

Transaction* txn = sdStorage.beginTxn(filename, myIdx); // can pass any number of filenames & indexes

if (txn) { // txn is nullptr if beginTxn fails

    sdStorage.save(filename, &configDto, txn);

    IndexEntry entry("123", "config1.dat"); 
    sdStorage.idxUpsert(myIdx, &entry, txn);

    if (!sdStorage.commitTxn(txn)) {
        Serial.println(F("Transaction commit failed, changes not applied"));
    } else {
        Serial.println(F("Transaction committed successfully!"));
    }

}
```

In this example, we start a transaction, then call save and idxUpsert including the `Transaction*`. When `commitTxn(txn)` is called, all the changes are applied in one go.

> IMPORTANT: If you forget to pass the `Transaction*` to a write operation, the system will hang because
> the file or index is locked by your explicit transaction, but the write operation also tries to lock it to
> an implicit transaction. If the file or index was NOT part of your explicit transaction, then the write
> operation will happen immediately, not as part of your transaction.


