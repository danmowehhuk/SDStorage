#ifndef _SDStorageTestHelper_h
#define _SDStorageTestHelper_h


class SDStorageTestHelper {

  public:
    String realFilename(SDStorage* sdStorage, const String& filename) {
      return sdStorage->realFilename(filename);
    };

};


#endif
