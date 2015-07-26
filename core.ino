#define IN_BUFFER_SIZE 128
#define CHANNELS 4

#define ESP_SERIAL Serial1
//#define ESP_SERIAL softSerial
//SoftwareSerial softSerial(2, 3);
#define DEBUG_SERIAL Serial

char inBuffer[IN_BUFFER_SIZE+1];
int inBufferLen = 0;

char inDataBuffer[CHANNELS][IN_BUFFER_SIZE+1];
int inDataBufferLen[CHANNELS];
boolean httpRequestRead[CHANNELS];

unsigned char state = 0;

Stream* espStream = &ESP_SERIAL;

boolean debugPrevNoData = false;

void setup() {
  //softSerial.begin(9600);
  ESP_SERIAL.begin(9600);
  DEBUG_SERIAL.begin(9600);
  delay(5000);
  
  inBuffer[0]=0; // USELESS
  for (int i=0; i < CHANNELS; i++) {
    inDataBufferLen[i]=0;
    inDataBuffer[i][0]=0; // USELESS
  }
}

int readLineLowLevel() {
  inBufferLen = 0;
  if (!espStream->available()) {
    inBufferLen=0;
    inBuffer[0]=0;
    return 0;
  }
  // Read to in buffer
  int prev = -1;
  int i = 0;
  //DEBUG_SERIAL.print("LL READ: {");
  while (true) {
    while(!espStream->available());
    char c = espStream->read();
    //DEBUG_SERIAL.print(c);
    i++;
    if (inBufferLen < IN_BUFFER_SIZE) {
      inBuffer[inBufferLen++] = c;
    }
    if (c == '\n' && prev == '\r') {
      break;
    }
    prev = c;
  }
  //DEBUG_SERIAL.println("}");
  inBuffer[inBufferLen] = 0;
  //DEBUG_SERIAL.print("RECV: {");
  //DEBUG_SERIAL.print(inBuffer);
  //DEBUG_SERIAL.println("}");
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
  DEBUG_SERIAL.print(data?"WAIT -> DATA\r\n":debugPrevNoData?"":"WAIT -> NO DATA\r\n");
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
  DEBUG_SERIAL.println(ok?"OK":"NOT OK");
  return ok;
}

void processConnect() {
  int n = inBuffer[0] - '0';
  inDataBuffer[n][0]=0;
  inDataBufferLen[n]=0;
  DEBUG_SERIAL.print("Connected ");
  DEBUG_SERIAL.println(n);
  httpRequestRead[n] = false;
  userConnect(n);
}

void processClosed() {
  int n = inBuffer[0] - '0';
  DEBUG_SERIAL.print("Closed ");
  DEBUG_SERIAL.println(n);
  userClosed(n);
}

void processIPDLine(int n) {
  /*
   * Do not respond unless inDataBuffer[n] equals "\r\n" (empty line)
   */
  DEBUG_SERIAL.print("+IPD LINE: ");
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

void processIPD() {
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
  while (t < l) {
    if (i >= IN_BUFFER_SIZE) {
        DEBUG_SERIAL.println();
        if (inDataBufferLen[n] > 0) {
          processIPDLine(n);
        }
        inDataBuffer[n][0]=0;
        inDataBufferLen[n]=0;
        readLineLowLevel();
        dp = 0;
        pc = -1;
        i = -1;
    }
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
      readLineLowLevel();
    } else {
      char c = inBuffer[i];
      DEBUG_SERIAL.print(c);
      if (dp < IN_BUFFER_SIZE) {
        inDataBuffer[n][dp]=c;
        inDataBuffer[n][dp+1]=0;
        dp++;
      }
      inDataBufferLen[n]=dp;
      if (c == '\n' && pc == '\r') {
        if (inDataBufferLen[n] > 0) {
          processIPDLine(n);
        }
        inDataBuffer[n][0]=0;
        inDataBufferLen[n]=0;
        readLineLowLevel();
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
      processIPD();
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

void netsend(int n, char* str) {
  boolean ok = false;
  while (!ok) {
    while(waitData(100)) {
      readLine();
    }
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
  }
  waitData(200);
  readLineLowLevel();
  waitData(200);
  skip();
  espStream->print(str);
  DEBUG_SERIAL.println(str);
  waitData(1000);
  readLineLowLevel();
  waitData(100);
  readLine();
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

