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

void setup() {
  //softSerial.begin(9600);
  ESP_SERIAL.begin(9600);
  DEBUG_SERIAL.begin(9600);
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
  //DEBUG_SERIAL.print("LL READ: {");
  // Read to in buffer
  int i = 0;
  while (true) {
    if (inBufferLen >= 2 && inBuffer[inBufferLen-2] == '\r' && inBuffer[inBufferLen-1] == '\n') {
      break;
    }
    while(!espStream->available());
    char c = espStream->read();
    i++;
    //DEBUG_SERIAL.print(c);
    //DEBUG_SERIAL.print(c, HEX);
    if (inBufferLen < BUFFER_SIZE) {
      inBuffer[inBufferLen++] = c;
    } else {
      if (c == '\r') {
        inBufferLen = BUFFER_SIZE - 2;
        inBuffer[inBufferLen++] = c;
      }
    }
  }
  //DEBUG_SERIAL.println("}");
  inBuffer[inBufferLen] = 0;
  //DEBUG_SERIAL.print("RECV: {");
  //DEBUG_SERIAL.print(inBuffer);
  //DEBUG_SERIAL.println("}");
  return i;
}

boolean ok = false;

void skip() {
  DEBUG_SERIAL.print("SKIP: {");
  while (espStream->available()) {
    char c = espStream->read();
    delay(1);
    DEBUG_SERIAL.print(c);
  }
  DEBUG_SERIAL.println("}");
}

boolean waitData(long time) {
  delay(1);
  for (long i = 0; i < time && !espStream->available(); i+=5) {
    delay(5);
  }
  boolean data = espStream->available();
  //DEBUG_SERIAL.print(data?"WAIT -> DATA\r\n":debugPrevNoData?"":"WAIT -> NO DATA\r\n");
  debugPrevNoData = !data;
  return data;
}

boolean send(char* str) {
  espStream->print(str);
  DEBUG_SERIAL.print("SENT: {");
  DEBUG_SERIAL.print(str);
  DEBUG_SERIAL.println("}");
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
  //DEBUG_SERIAL.println(ok?"OK":"NOT OK");
  return ok;
}

void processConnect() {
  int n = inBuffer[0] - '0';
  inDataBuffer[n][0]=0;
  inDataBufferLen[n]=0;
  DEBUG_SERIAL.print("Connected ");
  DEBUG_SERIAL.println(n);
  open[n]=true;
  httpRequestRead[n] = false;
  userConnect(n);
}

void processClosed() {
  int n = inBuffer[0] - '0';
  DEBUG_SERIAL.print("Closed ");
  DEBUG_SERIAL.println(n);
  open[n]=false;
  inDataBuffer[n][0]=0;
  inDataBufferLen[n]=0;
  userClosed(n);
}

void processIPDLine(int n) {
  /*
   * Do not respond unless inDataBuffer[n] equals "\r\n" (empty line)
   */
  DEBUG_SERIAL.print("+IPD LINE: ");
  DEBUG_SERIAL.print(n);
  DEBUG_SERIAL.print(" : ");
  if (!open[n]) {
    DEBUG_SERIAL.println("ERROR. NOT CONNECTED");
    return;
  }
  DEBUG_SERIAL.println(inDataBuffer[n]);
  if (strcmp(inDataBuffer[n], "\r\n") != 0) {
    if (httpRequestRead[n] == false) {
      userHttpRequest(n, inDataBuffer[n]);
      httpRequestRead[n] = true;
    } else {
      userHttpHeader(n, inDataBuffer[n]);
    }
  } else {
    userHttpResponse(n);
    DEBUG_SERIAL.print("AT+CIPCLOSE=");
    DEBUG_SERIAL.println(n);
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
    DEBUG_SERIAL.print("+IPD ERROR: ");
    DEBUG_SERIAL.println(inBuffer);
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
  DEBUG_SERIAL.print("+IPD METADATA: n=");
  DEBUG_SERIAL.print(n);
  DEBUG_SERIAL.print(", l=");
  DEBUG_SERIAL.println(l);
  if (j < 0) {
    DEBUG_SERIAL.print("+IPD ERROR: ");
    DEBUG_SERIAL.println(inBuffer);
    return;
  }
  DEBUG_SERIAL.print("+IPD LL: {{");
  int dp = inDataBufferLen[n];
  int pc = -1;
  int i = j;
  int t = 0;
  if (chars > BUFFER_SIZE) {
    t += chars - BUFFER_SIZE;
  }
  while (t < l) {
    if (i > inBufferLen) { //i < IN_BUFFER_SIZE && 
      DEBUG_SERIAL.print("+IPD ERROR: i (");
      DEBUG_SERIAL.print(i);
      DEBUG_SERIAL.print(") >= inBufferLen (");
      DEBUG_SERIAL.print(inBufferLen);
      DEBUG_SERIAL.println(")");
      return;
    }
    if (n >= l) {
      DEBUG_SERIAL.println("+IPD ERROR: n >= l");
      return;
    }
    if (inBufferLen == 0) {
      chars = readLineLowLevel();
      if (chars > BUFFER_SIZE) {
        t += chars - BUFFER_SIZE;
      }
    } else {
      char c = inBuffer[i];
      DEBUG_SERIAL.print(c);
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
  DEBUG_SERIAL.println("}}");
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
      DEBUG_SERIAL.print("ReadLine: {");
      DEBUG_SERIAL.print(inBuffer);
      DEBUG_SERIAL.println("}");
      ok = true;
    }
  }
  return n;
}

boolean netsend_P(int n, PGM_P str) {
  //DEBUG_SERIAL.println("NSP");
  char buff[BUFFER_SIZE+1];
  strncpy_P(buff, str, BUFFER_SIZE);
  buff[BUFFER_SIZE]=0;
  return netsend(n, buff);
}

boolean netsend(int n, char* str) {
  //DEBUG_SERIAL.println("NSLL");
  int tries = 10;
  int i = 0;
  boolean ok = false;
  while (!ok) {
    while(waitData(10)) {
      readLine();
    }
    delay(10);
    espStream->print("AT+CIPSEND=");
    DEBUG_SERIAL.print("AT+CIPSEND=");
    espStream->print(n);
    DEBUG_SERIAL.print(n);
    espStream->print(",");
    DEBUG_SERIAL.print(",");
    int l = strlen(str);
    espStream->print(l);
    DEBUG_SERIAL.println(l);
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
  DEBUG_SERIAL.println(str);
  waitData(1000);
  readLineLowLevel();
  waitData(100);
  readLine();
  return true;
}

boolean sendPrep() {
  DEBUG_SERIAL.print("SendPrep: ");
  inBufferLen = 0;
  if (!espStream->available()) {
    inBuffer[inBufferLen]=0;
    return false;
  }
  unread=false;
  for (int i = 0; i < 2; i++) {
    char c = espStream->read();
    DEBUG_SERIAL.print(c);
    if (inBufferLen < BUFFER_SIZE) {
      inBuffer[inBufferLen++] = c;
    }
    inBuffer[inBufferLen] = 0;
  }
  if (inBuffer[inBufferLen-2] == '>' /* && inBuffer[inBufferLen-1] == ' '*/) {
    DEBUG_SERIAL.println("SendPrep OK");
    return true;
  } else {
    DEBUG_SERIAL.println("UNREAD");
    unread = true;
    return false;
  }
}

void loop() {
  if (!ok) {
    DEBUG_SERIAL.println("need init");
    if (!setupEsp()) {
      DEBUG_SERIAL.println("bad init!");
      return;
    }
    DEBUG_SERIAL.println("init ok");
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
      DEBUG_SERIAL.print("COMMAND RESPONSE: ");
      DEBUG_SERIAL.println(inBuffer);
      callback(inBuffer, context);
    } else {
      break;
    }
  }
  DEBUG_SERIAL.println("COMMAND END");
}

boolean commandWaitCallback_P(PGM_P command, long timeoutInitial, long timeoutAfter, LineCallback callback, void* context) {
  char buff[BUFFER_SIZE+1];
  strncpy_P(buff, command, BUFFER_SIZE);
  buff[BUFFER_SIZE]=0;
  commandWaitCallback(buff, timeoutInitial, timeoutAfter, callback, context);
}


