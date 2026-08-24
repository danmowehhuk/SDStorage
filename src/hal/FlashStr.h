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

#endif

#endif
