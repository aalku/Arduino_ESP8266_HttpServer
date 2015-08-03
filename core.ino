#include "core.h"


char inBuffer[BUFFER_SIZE+1];
int inBufferLen = 0;

char inDataBuffer[CHANNELS][BUFFER_SIZE+1];
int inDataBufferLen[CHANNELS];

boolean httpRequestRead[CHANNELS];

boolean open[CHANNELS];

unsigned char state = 0;

Stream* espStream = &ESP_SERIAL;

boolean debugPrevNoData = false;

boolean unread = false;

boolean debugEnabled;

long debugEnabledLastTimeChecked;

void setup() {
  DEBUG_INIT();
  //softSerial.begin(9600);
  ESP_SERIAL.begin(9600);
  espStream->print("AT\r\n");
  delay(500);
  espStream->print("AT+RST\r\n");
  delay(3000);
  skip();  
  
  inBuffer[0]=0; // USELESS
  for (int i=0; i < CHANNELS; i++) {
    inDataBufferLen[i]=0;
    inDataBuffer[i][0]=0;
    open[i]=false;
  }
}

int readLineLowLevel() {
  if (!unread) {
    inBufferLen = 0;
    if (!espStream->available()) {
      inBuffer[inBufferLen]=0;
      return 0;
    }
  }
  unread=false;
  //DEBUG_PRINT("LL READ: {");
  // Read to in buffer
  int i = 0;
  while (true) {
    if (inBufferLen >= 2 && inBuffer[inBufferLen-2] == '\r' && inBuffer[inBufferLen-1] == '\n') {
      break;
    }
    while(!espStream->available());
    char c = espStream->read();
    i++;
    //DEBUG_PRINT(c);
    //DEBUG_PRINT(c, HEX);
    if (inBufferLen < BUFFER_SIZE) {
      inBuffer[inBufferLen++] = c;
    } else {
      if (c == '\r') {
        inBufferLen = BUFFER_SIZE - 2;
        inBuffer[inBufferLen++] = c;
      }
    }
  }
  //DEBUG_PRINTLN("}");
  inBuffer[inBufferLen] = 0;
  //DEBUG_PRINT("RECV: {");
  //DEBUG_PRINT(inBuffer);
  //DEBUG_PRINTLN("}");
  return i;
}

boolean ok = false;

void skip() {
  DEBUG_PRINT("SKIP: {");
  while (espStream->available()) {
    char c = espStream->read();
    delay(1);
    DEBUG_PRINT(c);
  }
  DEBUG_PRINTLN("}");
}

boolean waitData(long time) {
  delay(1);
  for (long i = 0; i < time && !espStream->available(); i+=5) {
    delay(5);
  }
  boolean data = espStream->available();
  //DEBUG_PRINT(data?"WAIT -> DATA\r\n":debugPrevNoData?"":"WAIT -> NO DATA\r\n");
  debugPrevNoData = !data;
  return data;
}

boolean send(char* str) {
  espStream->print(str);
  DEBUG_PRINT("SENT: {");
  DEBUG_PRINT(str);
  DEBUG_PRINTLN("}");
}

boolean setupEsp() {
  skip();
  send("zzzz\r\n");
  delay(100);
  skip();
  send("\r\nATE0\r\n");
  delay(100);
  skip();
  return okCmd("AT\r\n");
}

boolean okCmd(char* cmd) {
  send(cmd);
  waitData(1000);
  delay(100);
  readLine();
  readLine();
  boolean ok = (inBufferLen == 4 && memcmp(inBuffer, "OK\r\n", 4) == 0);
  //DEBUG_PRINTLN(ok?"OK":"NOT OK");
  return ok;
}

void processConnect() {
  int n = inBuffer[0] - '0';
  inDataBuffer[n][0]=0;
  inDataBufferLen[n]=0;
  DEBUG_PRINT("Connected ");
  DEBUG_PRINTLN(n);
  open[n]=true;
  httpRequestRead[n] = false;
  userConnect(n);
}

void processClosed() {
  int n = inBuffer[0] - '0';
  DEBUG_PRINT("Closed ");
  DEBUG_PRINTLN(n);
  open[n]=false;
  inDataBuffer[n][0]=0;
  inDataBufferLen[n]=0;
  userClosed(n);
}

void processIPDLine(int n) {
  /*
   * Do not respond unless inDataBuffer[n] equals "\r\n" (empty line)
   */
  DEBUG_PRINT("+IPD LINE: ");
  DEBUG_PRINT(n);
  DEBUG_PRINT(" : ");
  if (!open[n]) {
    DEBUG_PRINTLN("ERROR. NOT CONNECTED");
    return;
  }
  DEBUG_PRINTLN(inDataBuffer[n]);
  if (strcmp(inDataBuffer[n], "\r\n") != 0) {
    if (httpRequestRead[n] == false) {
      userHttpRequest(n, inDataBuffer[n]);
      httpRequestRead[n] = true;
    } else {
      userHttpHeader(n, inDataBuffer[n]);
    }
  } else {
    userHttpResponse(n);
    DEBUG_PRINT("AT+CIPCLOSE=");
    DEBUG_PRINTLN(n);
    espStream->print("AT+CIPCLOSE=");
    espStream->print(n);
    espStream->print("\r\n");
  }
}

void processIPD(int chars) {
  /*
   * FIXME: There is something broken here. When a line is too long it should crop it and continue but it does not continue correctly.
   */
  if (inBufferLen < 9) {
    DEBUG_PRINT("+IPD ERROR: ");
    DEBUG_PRINTLN(inBuffer);
    return;
  }
  int n = inBuffer[5] - '0';
  int l=0;
  int j = -1;
  for (int i = 7; i < inBufferLen; i++) {
    if (inBuffer[i] == ':') {
      j = i + 1;
      break;
    }
    l = l * 10 + inBuffer[i] - '0';
  }
  DEBUG_PRINT("+IPD METADATA: n=");
  DEBUG_PRINT(n);
  DEBUG_PRINT(", l=");
  DEBUG_PRINTLN(l);
  if (j < 0) {
    DEBUG_PRINT("+IPD ERROR: ");
    DEBUG_PRINTLN(inBuffer);
    return;
  }
  DEBUG_PRINT("+IPD LL: {{");
  int dp = inDataBufferLen[n];
  int pc = -1;
  int i = j;
  int t = 0;
  if (chars > BUFFER_SIZE) {
    t += chars - BUFFER_SIZE;
  }
  while (t < l) {
    if (i > inBufferLen) { //i < IN_BUFFER_SIZE && 
      DEBUG_PRINT("+IPD ERROR: i (");
      DEBUG_PRINT(i);
      DEBUG_PRINT(") >= inBufferLen (");
      DEBUG_PRINT(inBufferLen);
      DEBUG_PRINTLN(")");
      return;
    }
    if (n >= l) {
      DEBUG_PRINTLN("+IPD ERROR: n >= l");
      return;
    }
    if (inBufferLen == 0) {
      chars = readLineLowLevel();
      if (chars > BUFFER_SIZE) {
        t += chars - BUFFER_SIZE;
      }
    } else {
      char c = inBuffer[i];
      DEBUG_PRINT(c);
      if (dp < BUFFER_SIZE) {
        inDataBuffer[n][dp]=c;
        inDataBuffer[n][dp+1]=0;
        dp++;
        inDataBufferLen[n]=dp;
      }
      if (c == '\n' && pc == '\r') {
        if (inDataBufferLen[n] > 0) {
          processIPDLine(n);
        }
        inDataBuffer[n][0]=0;
        inDataBufferLen[n]=0;
        chars = readLineLowLevel();
        if (chars > BUFFER_SIZE) {
          t += chars - BUFFER_SIZE;
        }
        dp = 0;
        pc = -1;
        i = -1;
      }
      pc = c;
      i++;
      t++;
    }
  }
  DEBUG_PRINTLN("}}");
}

int readLine() {
  boolean ok = false;
  int n;
  while (!ok) {
    n = readLineLowLevel();
    if (strstr(inBuffer, "+IPD,") == inBuffer) {
      processIPD(n);
    } else if (strstr(inBuffer, ",CONNECT") == &inBuffer[1]) {
      processConnect();
    } else if (strstr(inBuffer, ",CLOSED") == &inBuffer[1]) {
      processClosed();
    } else {
      DEBUG_PRINT("ReadLine: {");
      DEBUG_PRINT(inBuffer);
      DEBUG_PRINTLN("}");
      ok = true;
    }
  }
  return n;
}

boolean netsend_P(int n, PGM_P str) {
  //DEBUG_PRINTLN("NSP");
  char buff[BUFFER_SIZE+1];
  strncpy_P(buff, str, BUFFER_SIZE);
  buff[BUFFER_SIZE]=0;
  return netsend(n, buff);
}

boolean netsend(int n, char* str) {
  //DEBUG_PRINTLN("NSLL");
  int tries = 10;
  int i = 0;
  boolean ok = false;
  while (!ok) {
    while(waitData(10)) {
      readLine();
    }
    delay(10);
    espStream->print("AT+CIPSEND=");
    DEBUG_PRINT("AT+CIPSEND=");
    espStream->print(n);
    DEBUG_PRINT(n);
    espStream->print(",");
    DEBUG_PRINT(",");
    int l = strlen(str);
    espStream->print(l);
    DEBUG_PRINTLN(l);
    espStream->print("\r\n");
    waitData(1000);
    readLineLowLevel();
    ok = !(inBufferLen > 4 && memcmp(inBuffer, "busy", 4) == 0);
    if (ok) {
      waitData(200);
      readLineLowLevel();
      waitData(100);
      if (true) {
        ok = sendPrep();
      } else {
        skip();
      }
    }
    if (i++ > tries) {
      // ABORT
      espStream->print("\r\nAT\r\n");
      while(waitData(100)) {
        readLine();
      }
      return false;
    }
  }
  espStream->print(str);
  DEBUG_PRINTLN(str);
  waitData(1000);
  readLineLowLevel();
  waitData(100);
  readLine();
  return true;
}

boolean sendPrep() {
  DEBUG_PRINT("SendPrep: ");
  inBufferLen = 0;
  if (!espStream->available()) {
    inBuffer[inBufferLen]=0;
    return false;
  }
  unread=false;
  for (int i = 0; i < 2; i++) {
    char c = espStream->read();
    DEBUG_PRINT(c);
    if (inBufferLen < BUFFER_SIZE) {
      inBuffer[inBufferLen++] = c;
    }
    inBuffer[inBufferLen] = 0;
  }
  if (inBuffer[inBufferLen-2] == '>' /* && inBuffer[inBufferLen-1] == ' '*/) {
    DEBUG_PRINTLN("SendPrep OK");
    return true;
  } else {
    DEBUG_PRINTLN("UNREAD");
    unread = true;
    return false;
  }
}

void loop() {
  while (Serial.available()) {
    Serial.read();
  }
  if (!ok) {
    DEBUG_PRINTLN("need init");
    if (!setupEsp()) {
      DEBUG_PRINTLN("bad init!");
      return;
    }
    DEBUG_PRINTLN("init ok");
    ok = true;
    ok |= okCmd("AT+CIPMUX=1\r\n");
    ok |= okCmd("AT+CIPSERVER=1,80\r\n");
  }
  if (waitData(100)) {
    if (readLine() > 0) {
    }
  }
}

boolean commandWaitCallback(char* command, long timeoutInitial, long timeoutAfter, LineCallback callback, void* context) {
  while (waitData(100)) {
    readLine();
  }
  send(command);
  waitData(timeoutInitial);
  while(true) {
    if (waitData(timeoutAfter)) {
      readLine();
      DEBUG_PRINT("COMMAND RESPONSE: ");
      DEBUG_PRINTLN(inBuffer);
      callback(inBuffer, context);
    } else {
      break;
    }
  }
  DEBUG_PRINTLN("COMMAND END");
}

boolean commandWaitCallback_P(PGM_P command, long timeoutInitial, long timeoutAfter, LineCallback callback, void* context) {
  char buff[BUFFER_SIZE+1];
  strncpy_P(buff, command, BUFFER_SIZE);
  buff[BUFFER_SIZE]=0;
  commandWaitCallback(buff, timeoutInitial, timeoutAfter, callback, context);
}


