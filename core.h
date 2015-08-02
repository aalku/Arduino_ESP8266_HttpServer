#define BUFFER_SIZE 96
#define CHANNELS 4

//#include <SoftwareSerial.h>
#include <Stream.h>

#define ESP_SERIAL Serial1
//#define ESP_SERIAL softSerial
//SoftwareSerial softSerial(2, 3);

#define DEBUG_INIT() { Serial.begin(9600); };

#define DEBUG_PRINT(x) { if (true) { Serial.print(x);} }
#define DEBUG_PRINTLN(x) { if (true) { Serial.println(x);} }

//#define DEBUG_PRINT(x)
//#define DEBUG_PRINTLN(x) 

typedef void (*LineCallback)(char* line, void* context);
