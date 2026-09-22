#pragma once

#include <Arduino.h>

#ifdef CAN_STORE_MANY_STRINGS
#define TIMESTAMP_BUFSZ (64)
#define TIMESTAMP()                                                                                                                                                                                                                            \
    {                                                                                                                                                                                                                                          \
        char timestampbuf[TIMESTAMP_BUFSZ];                                                                                                                                                                                                    \
        snprintf(timestampbuf, TIMESTAMP_BUFSZ - 1, "[%3lu.%03lu%03lu] [%s::%u]  ", millis() / 1000UL, millis() % 1000UL, micros() % 1000UL, __FILE__, __LINE__);                                                                              \
        Serial.print(timestampbuf);                                                                                                                                                                                                            \
    }
#define TIMESTAMPLN()                                                                                                                                                                                                                          \
    {                                                                                                                                                                                                                                          \
        TIMESTAMP();                                                                                                                                                                                                                           \
        Serial.println();                                                                                                                                                                                                                      \
    }
#define DEBUG(...)   Serial.print(__VA_ARGS__)
#define DEBUGLN(...) Serial.println(__VA_ARGS__)
#else
#define TIMESTAMP()
#define TIMESTAMPLN()
#define DEBUG(...)
#define DEBUGLN(...)
#endif