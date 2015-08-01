#include "core.h"

int req[CHANNELS];
char buff[BUFFER_SIZE+1];

const char PROGMEM headers_A[BUFFER_SIZE] = "Content-Type: text/html\r\nCache-Control: no-cache, no-store, must-revalidate\r\n";
const char PROGMEM headers_B[BUFFER_SIZE] = "Pragma: no-cache\r\nExpires: -1\r\n\r\n";
const char PROGMEM HTML_PART_A[BUFFER_SIZE] = "<html><head><title>";
const char PROGMEM HTML_PART_B[BUFFER_SIZE] = "</title>\r\n</head><body>\r\n";
const char PROGMEM HTML_PART_C[BUFFER_SIZE] = "\r\n</body></html>\r\n";
const char PROGMEM STATUS_404[] = "HTTP/1.0 404 Not Found\r\n";
const char PROGMEM STATUS_200[] = "HTTP/1.0 200 OK\r\n";

/*
 * One input buffer and another per channel for the incoming lines thar arrive split.
 * Always try to read complete lines
 * If a buffer overflows the rest of the line is discarded.
 * Http processing is line by line. Text is not accumulated but you can save any info and send answer once request is complete after blank line.
 * 
 * Eveything is this way to save memory.
 */

void userConnect(int n) {
  req[n]=0;
}

void userHttpRequest(int n, char* line) { 
  // Read GET request and parameters
  if (memcmp_P(line, PSTR("GET /"), 5)==0) {
    int l = strlen(line);
    int o = 4;
    int i = o;
    for (; i < l; i++) {
      char c = line[i];
      if (c == ' ' || c == '?') {
        break;
      }
      if (i < BUFFER_SIZE) {
        buff[i-o]=c;
        buff[i-o + 1] = 0;
      }
    }
    i++;
    char* args = NULL;
    if (buff[i] == '?') {
      buff[i] = 0;
      i++;
      args=&buff[i];
      for (; i < l; i++) {
        if (buff[i] == ' ') {
          buff[i] == 0;
          break;
        }
      }
    } else {
      buff[i] = 0;
    }
    // TODO req[n] = idReq(buff);
    DEBUG_SERIAL.print("REQ: ");
    DEBUG_SERIAL.println(buff);
    if (strncmp(buff, "/", BUFFER_SIZE)==0) {
      req[n] = 1;
    } else if (strncmp_P(buff, PSTR("/wifi"), BUFFER_SIZE)==0) {
      req[n] = 2;
    }

  }
  // DO NOT SEND ANYTHING TO THE CLIENT OR EVEN TALK TO THE ESP8266 HERE!!
}

void userHttpHeader(int n, char* line) {
  // Why would I need any header on this project?
  // DO NOT SEND ANYTHING TO THE CLIENT OR EVEN TALK TO THE ESP8266 HERE!!
}

void userHttpResponse(int n) {
  // You can talk to the ESP8266 and send a response here.
  if (req[n] == 1) {
    netsend_P(n, STATUS_200);
    netsend_P(n, headers_A);
    netsend_P(n, headers_B);
    netsend_P(n, HTML_PART_A);
    netsend_P(n, PSTR("Arduino httpd"));
    netsend_P(n, HTML_PART_B);
    netsend_P(n, PSTR("Hello world!!"));
    netsend_P(n, HTML_PART_C);
    //snprintf(buff, sizeof(buff), "<p>Req=%d</p>\r\n", req[n]);
    //netsend(n, buff);
  } else if (req[n] == 2) {
    while(waitData(500)) {
      readLine();
    }
    send("AT+CWLAP\r\n");
    delay(1000);
    while(waitData(500)) {
      readLine();
    }
    netsend_P(n, STATUS_200);
    netsend_P(n, headers_A);
    netsend_P(n, headers_B);
    netsend_P(n, HTML_PART_A);
    netsend_P(n, PSTR("Arduino httpd"));
    netsend_P(n, HTML_PART_B);
    netsend_P(n, PSTR("WiFi networks:<ul>"));
    netsend_P(n, PSTR("<li>"));
    netsend_P(n, PSTR("1"));
    netsend_P(n, PSTR("</li>"));
    netsend_P(n, PSTR("<li>"));
    netsend_P(n, PSTR("2"));
    netsend_P(n, PSTR("</li>"));
    netsend_P(n, PSTR("</ul>"));
    netsend_P(n, HTML_PART_C);
    //snprintf(buff, sizeof(buff), "<p>Req=%d</p>\r\n", req[n]);
    //netsend(n, buff);
  } else {
    netsend_P(n, STATUS_404);
    netsend_P(n, headers_A);
    netsend_P(n, headers_B);
    netsend_P(n, HTML_PART_A);
    netsend_P(n, PSTR("ERROR 404: NOT FOUND!!"));
    netsend_P(n, HTML_PART_B);
    netsend_P(n, PSTR("ERROR 404: NOT FOUND!!"));
    netsend_P(n, HTML_PART_C);

  }
}

void userClosed(int n) {
  
}

