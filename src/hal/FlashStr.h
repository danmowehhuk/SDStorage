#ifndef SDSTORAGE_HAL_FLASHSTR_H
#define SDSTORAGE_HAL_FLASHSTR_H

#ifndef NO_ARDUINO

#include <Arduino.h>
using FlashStr = __FlashStringHelper;
// F() is already provided by Arduino.h

#else

#include <BareMetalHAL.h>
using FlashStr = BareMetalHAL::FlashStr;
// F() is already provided by BareMetalHAL.h

// On Arduino these come in transitively via Arduino.h. Off Arduino,
// nothing else pulls them in, and this library's C++ files use them
// throughout (strlen/strcmp/strchr/strncpy, malloc/free, isspace,
// snprintf_P).
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#endif

#endif
