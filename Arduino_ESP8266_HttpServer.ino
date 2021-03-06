#include "core.h"

#define CWLAP_MAX_LEN 255

int req[CHANNELS];
char buff[BUFFER_SIZE+1];

#define MAX_APNAME 64
typedef struct NParams {
  char APNAME[MAX_APNAME+1]="";
  char APIP[3*4+3+1]="";
  char JAPNAME[MAX_APNAME+1]="";
  char STAIP[3*4+3+1]="";
};

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
    DEBUG_PRINT("REQ: ");
    DEBUG_PRINTLN(buff);
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

void callbackCWLAP(char* line, void* context) {
  //DEBUG_PRINT("AT+CWLAP RESPONSE: ");
  //DEBUG_PRINTLN(line);
  char* buff = (char*)context;
  int k = strlen(buff);
  int l = strlen(line);
  if (l > 7 && memcmp(line, "+CWLAP:(", 7) == 0) {
    int i;
    for (i = 8;; i++) {
      if (i == l) {
        DEBUG_PRINTLN("CWLAP ERROR 1");
        return;
      }
      if (line[i]=='"') {
        //DEBUG_PRINTLN("CWLAP \" at");
        //DEBUG_PRINTLN(i);
        if (k > 0) {
          buff[k++] = '\t';
        }
        break;
      }
    }
    for (i++;; i++) {
      if (i == l) {
        DEBUG_PRINTLN("CWLAP ERROR 2");
        return;
      }
      if (line[i]=='"') {
        //DEBUG_PRINTLN("CWLAP \" at");
        //DEBUG_PRINTLN(i);
        buff[k] = 0;
        break;
      } else {
        if (k < CWLAP_MAX_LEN - 1) {
          buff[k++] = line[i];
          //DEBUG_PRINTLN("CWLAP char at");
          //DEBUG_PRINTLN(i);
        }
      }
    }
  }
}

void callbackIP(char* line, void* context) {
  NParams* p = (NParams*)context;
  int o = 0;
  int max = 0;
  char* dst = NULL;
  if (memcmp_P(line, PSTR("+CWSAP_CUR:\""), 12)==0) {
    o = 12;
    max = MAX_APNAME;
    dst = p->APNAME;
  } else if (memcmp_P(line, PSTR("+CWJAP_CUR:\""), 12)==0) {
    o = 12;
    max = MAX_APNAME;
    dst = p->JAPNAME;
  } else if (memcmp_P(line, PSTR("+CIFSR:APIP,\""), 13)==0) {
    o = 13;
    max = 3*4+3+1;
    dst = p->APIP;
  } else if (memcmp_P(line, PSTR("+CIFSR:STAIP,\""), 14)==0) {
    o = 14;
    max = 3*4+3+1;
    dst = p->STAIP;
  }
  if (dst != NULL) {
    for (int i = o; i-o < max; i++) {
      if (line[i] == '"') {
        break;
      } else {
        dst[i-o]=line[i];
        dst[i-o+1]=0;
      }
    }
  }
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
    char wifilist[CWLAP_MAX_LEN] = "";
    netsend_P(n, STATUS_200);
    netsend_P(n, headers_A);
    netsend_P(n, headers_B);
    netsend_P(n, HTML_PART_A);
    netsend_P(n, PSTR("Arduino httpd"));
    netsend_P(n, HTML_PART_B);

    NParams nparams;
    commandWaitCallback_P(PSTR("AT+CIFSR\r\n"), 1000, 1000, &callbackIP, (void*)&nparams);
    commandWaitCallback_P(PSTR("AT+CWSAP_CUR?\r\n"), 1000, 1000, &callbackIP, (void*)&nparams);
    commandWaitCallback_P(PSTR("AT+CWJAP_CUR?\r\n"), 1000, 1000, &callbackIP, (void*)&nparams);

    netsend_P(n, PSTR("<label>Network parameters:</label><ul><li>SOFTAP NAME: "));
    if (nparams.APNAME[0] != 0) {
      netsend(n, nparams.APNAME);
    } else {
      netsend_P(n, PSTR("DISABLED"));
    }
    netsend_P(n, PSTR("</li><li>SOFTAP IP: "));
    if (nparams.APIP[0] != 0) {
      netsend(n, nparams.APIP);
    } else {
      netsend_P(n, PSTR("DISABLED"));
    }
    netsend_P(n, PSTR("</li><li>WLAN NAME: "));
    if (nparams.JAPNAME[0] != 0) {
      netsend(n, nparams.JAPNAME);
    } else {
      netsend_P(n, PSTR("NOT CONNECTED"));
    }
    netsend_P(n, PSTR("</li><li>WLAN IP: "));
    if (nparams.STAIP[0] != 0) {
      netsend(n, nparams.STAIP);
    } else {
      netsend_P(n, PSTR("NOT CONNECTED"));
    }
    netsend_P(n, PSTR("</li></ul>"));
    
    commandWaitCallback_P(PSTR("AT+CWLAP\r\n"), 10000, 1000, &callbackCWLAP, (void*)wifilist);
    netsend_P(n, PSTR("<label>WiFi networks:</label><ul>"));
    char* token = strtok(wifilist, "\t");
    while (token != NULL) {
      buff[0] = 0;
      strcat_P(buff, PSTR("<li>"));
      strcat(buff, token);
      strcat_P(buff, PSTR("</li>"));
      netsend(n, buff);
      token = strtok(NULL, "\t");
    }

    netsend_P(n, PSTR("</ul><form method='GET' action='wifisave'><label>Connect to network:</label>"));
    netsend_P(n, PSTR("<input type='text' placeholder='network' name='n'/>"));
    netsend_P(n, PSTR("<input type='text' placeholder='password' name='p'/>"));
    netsend_P(n, PSTR("<input type='submit' value='Connect/Disconnect'/></form>"));
    netsend_P(n, HTML_PART_C);
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

