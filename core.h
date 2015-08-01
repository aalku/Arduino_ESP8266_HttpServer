#define BUFFER_SIZE 96
#define CHANNELS 4

//#include <SoftwareSerial.h>
#include <Stream.h>

#define ESP_SERIAL Serial1
//#define ESP_SERIAL softSerial
//SoftwareSerial softSerial(2, 3);
#define DEBUG_SERIAL Serial

typedef void (*LineCallback)(char* line, void* context);
