//#include <SoftwareSerial.h>
#include <Stream.h>

/*
 * One input buffer and another per channel for the incoming lines thar arrive split.
 * Always try to read complete lines
 * If a buffer overflows the rest of the line is discarded.
 * Http processing is line by line. Text is not accumulated but you can save any info and send answer once request is complete after blank line.
 * 
 * Eveything is this way to save memory.
 */

void userConnect(int n) {
  
}

void userHttpRequest(int n, char* line) {
  // Read GET request and parameters
  // DO NOT SEND ANYTHING TO THE CLIENT OR EVEN TALK TO THE ESP8266 HERE!!
}

void userHttpHeader(int n, char* line) {
  // Why would I need any header on this project?
  // DO NOT SEND ANYTHING TO THE CLIENT OR EVEN TALK TO THE ESP8266 HERE!!
}

void userHttpResponse(int n) {
  // You can talk to the ESP8266 and send a response here.
  netsend(n, "HTTP/1.0 200 OK\r\n");
  netsend(n, "Content-Type: text/html\r\n");
  netsend(n, "\r\n");
  netsend(n, "<html><head><title>Arduino - Hello world</title>\r\n");
  netsend(n, "</head><body>\r\n");
  netsend(n, "Hello world!! This is arduino!!\r\n");
  netsend(n, "</body></html>\r\n");
}

void userClosed(int n) {
  
}

