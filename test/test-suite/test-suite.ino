#include <SDStorage.h>

#define SD_CS_PIN 12

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

SDStorage sdStorage(SD_CS_PIN, String("TESTROOT"));
StreamableManager strManager;

void testBegin(TestInvocation* t) {
  t->name = STR("SDStorage initialization");
  do {
    if (!sdStorage.begin()) {
      t->message = STR("begin() failed");
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

    testBegin

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
