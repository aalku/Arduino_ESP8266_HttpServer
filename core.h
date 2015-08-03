#define BUFFER_SIZE 96
#define CHANNELS 4

//#include <SoftwareSerial.h>
#include <Stream.h>

#define ESP_SERIAL Serial1
//#define ESP_SERIAL softSerial
//SoftwareSerial softSerial(2, 3);

#ifndef DEBUG_SIGNAL_PIN
#define DEBUG_SIGNAL_PIN 9
#endif

#ifndef DEBUG_FORCED
#define DEBUG_FORCED false
#endif

#define DEBUG_TEST_1 ((DEBUG_FORCED || (Serial && !digitalRead(DEBUG_SIGNAL_PIN))) ? true : false)

#define DEBUG_INIT() do { \
  extern boolean debugEnabled; \
  extern long debugEnabledLastTimeChecked; \
  debugEnabledLastTimeChecked = millis(); \
  pinMode(DEBUG_SIGNAL_PIN, INPUT_PULLUP); \
  Serial.begin(9600); \
  debugEnabled = DEBUG_TEST_1; \
} while (0);

#define DEBUG_PRINT(x) do { \
  extern boolean debugEnabled; \
  extern long debugEnabledLastTimeChecked; \
  if (millis() > debugEnabledLastTimeChecked + 500) { \
    debugEnabled = DEBUG_TEST_1; \
    debugEnabledLastTimeChecked = millis(); \
  } \
  if (debugEnabled) { Serial.print(x);} else { Serial.flush(); } \
} while (0);
#define DEBUG_PRINTLN(x) do { \
  extern boolean debugEnabled; \
  extern long debugEnabledLastTimeChecked; \
  if (millis() > debugEnabledLastTimeChecked + 500) { \
    debugEnabled = DEBUG_TEST_1; \
    debugEnabledLastTimeChecked = millis(); \
  } \
  if (debugEnabled) { Serial.println(x);} else { Serial.flush(); } \
} while (0);

//#define DEBUG_PRINT(x)
//#define DEBUG_PRINTLN(x) 

typedef void (*LineCallback)(char* line, void* context);
