/**The MIT License (MIT)

Copyright (c) 2016 by Greg Cunningham, CuriousTech

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <Wire.h>
#include <ssd1306_i2c.h> // https://github.com/CuriousTech/WiFi_Doorbell/tree/master/Libraries/ssd1306_i2c
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <OneWire.h>
#include <WiFiUdp.h>
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <DHT.h>  // http://www.github.com/markruys/arduino-DHT
#include "eeMem.h"
#include "RunningMedian.h"
#include "pages.h"
#include <JsonClient.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonClient

const char controlPassword[] = "password";    // device password for modifying any settings
const int serverPort = 82;                    // port fwd for fwdip.php
const int watts = 290;                        // standard waterbed heater wattage (300) or measured watts

#define EXPAN1    0  // side expansion pad
#define EXPAN2    2  // side expansion pad
#define ESP_LED   2  //Blue LED on ESP07 (on low)
#define SDA       4
#define SCL       5  // OLED
#define HEAT     12  // Heater output
#define BTN_UP   13
#define DS18B20  14
#define TONE     15  // Speaker.  Beeps on powerup, but can also be controlled by aother IoT or add an alarm clock.
#define BTN_DN   16

OneWire ds(DS18B20);
byte ds_addr[8];
uint32_t lastIP;
int nWrongPass;

SSD1306 display(0x3C, SDA, SCL); // Initialize the oled display for address 0x3C, SDA=4, SCL=5 (is the OLED mislabelled?)
int displayOnTimer;

eeMem eemem;
DHT dht;

WiFiManager wifi;  // AP page:  192.168.4.1
AsyncWebServer server( serverPort );
AsyncEventSource events("/events"); // event source (Server-Sent events)
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp;
bool bNeedUpdate;
uint8_t  dst;           // current dst

uint16_t roomTemp;
uint16_t rh;

int httpPort = 80; // may be modified by open AP scan

int currentTemp = 850;
int hiTemp; // current target
int loTemp;
bool bHeater = false;
uint8_t schInd = 0;
String sMessage;
uint8_t msgCnt;
uint32_t onCounter;
float fTotalCost;
float fLastTotalCost;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonClient jsonParse(jsonCallback);

const char days[7][4] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
const char months[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

float currCost()
{
  float fCurTotalCost = fTotalCost;
  if(onCounter) fCurTotalCost += (float)(onCounter * watts * ee.ppkwh) / 36000000000; // add current cycle
  return fCurTotalCost;
}

String dataJson()
{
  String s = "{";
  s += "\"t\": ";
  s += now() - ((ee.tz + dst) * 3600);
  s += ",\"waterTemp\": ";
  s += sDec(currentTemp);
  s += ",\"setTemp\": ";
  s += sDec(ee.schedule[schInd].setTemp);
  s += ", \"hiTemp\": ";
  s += sDec(hiTemp);
  s += ", \"loTemp\": ";
  s += sDec(loTemp);
  s += ", \"on\": ";
  s += bHeater;
  s += ", \"temp\": ";
  s += sDec(roomTemp);
  s += ", \"rh\": ";
  s += sDec(rh);
  s += ", \"tc\": ";
  s += String(currCost(), 2);
  s += "}";
  return s;
}

String setJson() // settings
{
  String s = "{";
  for(int i = 0; i < 8; i++)
  {
    s += "\"N";
    s += i;
    s += "\":\"";
    s += ee.schNames[i];
    s += "\",\"S";
    s += i;
    s += "\":\"";
    int h = ee.schedule[i].timeSch / 60;
    int m = ee.schedule[i].timeSch % 60;
    s += h;
    s += ":";
    if(m<10) s += "0";
    s += m;
    s += "\",\"T";
    s += i;
    s += "\":";
    s += sDec(ee.schedule[i].setTemp);
    s += ",\"H";
    s += i;
    s += "\":";
    s += sDec(ee.schedule[i].thresh);
    s += ",";
  }
  s += "\"vt\":";
  s += sDec(ee.vacaTemp);
  s += ",\"o\":";
  s += ee.bEnableOLED;
  s += ",\"tz\":";
  s += ee.tz;
  s += ",\"avg\":";
  s += ee.bAvg;
  s += ",\"ppkwh\":";
  s += ee.ppkwh;
  s += ",\"vo\":";
  s += ee.bVaca;
  s += ",\"idx\":";
  s += schInd;
  s += ",\"cnt\":";
  s += ee.schedCnt;
  s += ",\"tc2\":\"";
  s += months[(month()+10)%12]; // last month
  s += ": $";
  s += String(fLastTotalCost, 2);
  s += "\",\"m\":\"";
  s += months[month()]; // this month
  s += "\"}";
  return s;
}

void parseParams(AsyncWebServerRequest *request)
{
  static char temp[256];
  char password[64];
  int val;
  int freq = 0;

 if(request->params() == 0)
    return;

  // get password first
  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    switch( p->name().charAt(0)  )
    {
      case 'k': // key
        s.toCharArray(password, sizeof(password));
        break;
    }
  }

  uint32_t ip = request->client()->remoteIP();

  if(strcmp(controlPassword, password))
  {
    if(nWrongPass == 0)
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    String data = "{\"ip\":\"";
    data += request->client()->remoteIP().toString();
    data += "\",\"pass\":\"";
    data += password;
    data += "\"}";
    events.send(data.c_str(), "hack"); // log attempts
    lastIP = ip;
    return;
  }

  lastIP = ip;

  for ( uint8_t i = 0; i < request->params(); i++ ) {
    AsyncWebParameter* p = request->getParam(i);
    p->value().toCharArray(temp, 100);
    String s = wifi.urldecode(temp);

    int which = p->name().charAt(1) - '0'; // limitation = 9
    if(which >= MAX_SCHED) which = MAX_SCHED - 1; // safety
    val = s.toInt();
    bool b = (s == "true") ? true:false;

    switch( p->name().charAt(0)  )
    {
      case 'm':  // message
          sMessage = s;
          sMessage += " ";
          msgCnt = 4;
          if(ee.bEnableOLED == false)
          {
            displayOnTimer = 60;
          }
          break;
      case 'f': // frequency
          freq = val;
          break;
      case 'b': // beep : ?f=1200&b=1000
          Tone(TONE, freq, val);
          break;
    }
  }
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

void handleRoot(AsyncWebServerRequest *request) // Main webpage interface
{
//  digitalWrite(LED, HIGH);
  Serial.println("handleRoot");

  parseParams(request);

  request->send_P( 200, "text/html", page1 );
//  digitalWrite(LED, LOW);
}

String sDec(int t) // just 123 to 12.3 string
{
  String s = String( t / 10 ) + ".";
  s += t % 10;
  return s;
}

// Time in hh:mm[:ss][AM/PM]
String timeFmt(bool do_sec, bool do_M)
{
  String r = "";
  if(hourFormat12() < 10) r = " ";
  r += hourFormat12();
  r += ":";
  if(minute() < 10) r += "0";
  r += minute();
  if(do_sec)
  {
    r += ":";
    if(second() < 10) r += "0";
    r += second();
    r += " ";
  }
  if(do_M)
  {
      r += isPM() ? "PM":"AM";
  }
  return r;
}

void handleS(AsyncWebServerRequest *request) { // standard params, but no page
  Serial.println("handleS\n");
  parseParams(request);

  String page = "{\"ip\": \"";
  page += WiFi.localIP().toString();
  page += ":";
  page += serverPort;
  page += "\"}";
  request->send ( 200, "text/json", page );
}

// Return lots of vars as JSON
void handleJson(AsyncWebServerRequest *request)
{
  Serial.println("handleJson\n");
  String page = "{";
  for(int i = 0; i < 8; i++)
  {
    page += "\"setTemp";
    page += i;
    page += "\": ";
    page += sDec(ee.schedule[i].setTemp);
    page += ", ";
    page += "\"timeSch";
    page += i;
    page += "\": ";
    page += ee.schedule[i].timeSch;
    page += ", ";
    page += "\"Thresh";
    page += i;
    page += "\": ";
    page += sDec(ee.schedule[i].thresh);
    page += ", ";
  }
  page += "\"waterTemp\": ";
  page += sDec(currentTemp);
  page += ", \"hiTemp\": ";
  page += sDec(hiTemp);
  page += ", \"loTemp\": ";
  page += sDec(loTemp);
  page += ", \"schInd\": ";
  page += schInd;
  page += ", \"schedCnt\": ";
  page += ee.schedCnt;
  page += ", \"on\": ";
  page += bHeater;
  page += ", \"temp\": ";
  page += sDec(roomTemp);
  page += ", \"rh\": ";
  page += sDec(rh);
  page += "}";

  request->send( 200, "text/json", page );
}

void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

void onEvents(AsyncEventSourceClient *client)
{
//  client->send(":ok", NULL, millis(), 1000);
  events.send(dataJson().c_str(), "state");
}

const char *jsonListCmd[] = { "cmd",
  "key",
  "oled",
  "TZ", // location
  "avg",
  "cnt",
  "tadj",
  "ppkwh",
  "vaca",
  "vacatemp",
  "N0",  "N1",  "N2",  "N3",  "N4",  "N5",  "N6",  "N7",
  "S0",  "S1",  "S2",  "S3",  "S4",  "S5",  "S6",  "S7",
  "T0",  "T1",  "T2",  "T3",  "T4",  "T5",  "T6",  "T7",
  "H0",  "H1",  "H2",  "H3",  "H4",  "H5",  "H6",  "H7",
  NULL
};

bool bKeyGood;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(!bKeyGood && iName) return; // only allow for key
  char *p, *p2;

  switch(iEvent)
  {
    case 0: // cmd
      switch(iName)
      {
        case 0: // key
          if(!strcmp(psValue, controlPassword)) // first item must be key
            bKeyGood = true;
          break;
        case 1: // OLED
          ee.bEnableOLED = iValue ? true:false;
          break;
        case 2: // TZ
          ee.tz = iValue;
          getUdpTime();
          break;
        case 3: // avg
          ee.bAvg = iValue;
          break;
        case 4: // cnt
          ee.schedCnt = constrain(iValue, 1, 8);
          ws.printfAll("set;%s", setJson().c_str()); // update all the entries
          break;
        case 5: // tadj
          ee.schedule[schInd].setTemp += iValue*10;
          ee.schedule[schInd].setTemp = constrain(ee.schedule[schInd].setTemp, 600,900);
          break;
        case 6: // ppkw
          ee.ppkwh = iValue;
          break;
        case 7: // vaca
          ee.bVaca = iValue;
          break;
        case 8: // vacatemp
          ee.vacaTemp = constrain( (int)(atof(psValue)*10), 400, 800); // 40-80F
          break;
        case 9: // N0
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
          strncpy(ee.schNames[iName-9], psValue, sizeof(ee.schNames[0]) );
          break;
        case 17: // S0
        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
          p = strtok(psValue, ":");
          p2 = strtok(NULL, "");
          if(p && p2){
            iValue *= 60;
            iValue += atoi(p2);
          }
          ee.schedule[iName-17].timeSch = iValue;
          break;
        case 25: // T0
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
          ee.schedule[iName-25].setTemp = atof(psValue)*10;
          break;
        case 33: // H0
        case 34:
        case 35:
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
          ee.schedule[iName-33].thresh = (int)(atof(psValue)*10);
          checkLimits();      // constrain and check new values
          checkSched(true);   // reconfigure to new schedule
          break;
      }
      break;
  }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      client->printf("state;%s", dataJson().c_str());
      client->printf("set;%s", setJson().c_str());
      client->ping();
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      break;
    case WS_EVT_ERROR:    //error was received from the other end
      break;
    case WS_EVT_PONG:    //pong message was received (in response to a ping request maybe)
      break;
    case WS_EVT_DATA:  //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if(info->final && info->index == 0 && info->len == len){
        //the whole message is in a single frame and we got all of it's data
        if(info->opcode == WS_TEXT){
          data[len] = 0;

          char *pCmd = strtok((char *)data, ";"); // assume format is "name;{json:x}"
          char *pData = strtok(NULL, "");
          if(pCmd == NULL || pData == NULL) break;
          bKeyGood = false; // for callback (all commands need a key)
          jsonParse.process(pCmd, pData);
        }
      }
      break;
  }
}

void setup()
{
  pinMode(TONE, OUTPUT);
  digitalWrite(TONE, LOW);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DN, INPUT); // IO16 has external pullup
  pinMode(HEAT, OUTPUT);
  digitalWrite(HEAT, HIGH); // high is off

  // initialize dispaly
  display.init();
  display.flipScreenVertically();

  Serial.begin(115200);
//  delay(3000);
  Serial.println();

  WiFi.hostname("waterbed");
  wifi.autoConnect("Waterbed"); // Tries all open APs, then starts softAP mode for config

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if ( !MDNS.begin ( "waterbed", WiFi.localIP() ) ) {
    Serial.println ( "MDNS responder failed" );
  }

  // attach AsyncEventSource
  events.onConnect(onEvents);
  server.addHandler(&events);
  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET | HTTP_POST, handleRoot );
  server.on ( "/s", HTTP_GET | HTTP_POST, handleS );
  server.on ( "/json", HTTP_GET | HTTP_POST, handleJson );

  server.onNotFound(onRequest);
  server.onFileUpload(onUpload);
  server.onRequestBody(onBody);

  server.begin();

  jsonParse.addList(jsonListCmd);
  MDNS.addService("http", "tcp", serverPort);

  if( ds.search(ds_addr) )
  {
    Serial.print("OneWire device: "); // 28 22 92 29 7 0 0 6B
    for( int i = 0; i < 8; i++) {
      Serial.print(ds_addr[i], HEX);
      Serial.print(" ");
    }
    Serial.println("");
    if( OneWire::crc8( ds_addr, 7) != ds_addr[7]){
      Serial.println("Invalid CRC");
    }
  }
  else
  {
    Serial.println("No OneWire devices");
  }

  getUdpTime();

  Tone(TONE, 2600, 200);
  dht.setup(EXPAN1, DHT::DHT22);
}

void loop()
{
  static RunningMedian<uint16_t,12> tempMedian;
  static RunningMedian<uint16_t,12> rhMedian;
  static uint8_t hour_save, min_save, sec_save, mon_save;
  static bool bLastOn;

  MDNS.update();

  checkButtons();

  if(bNeedUpdate)
    if(checkUdpTime())
      checkSched(true);  // initialize

  if(sec_save != second()) // only do stuff once per second
  {
    sec_save = second();

    checkTemp();
    static uint8_t dht_cnt = 5;
    if(--dht_cnt == 0)
    {
      dht_cnt = 10;
      rhMedian.add((uint16_t)(dht.getHumidity()*10));
      if(dht.getStatus() == DHT::ERROR_NONE)
      {
        rhMedian.getMedian(rh);
        tempMedian.add((uint16_t)(dht.toFahrenheit(dht.getTemperature()) * 10));
        tempMedian.getMedian(roomTemp);
      }
      events.send(dataJson().c_str(), "state"); // update every 10 seconds
      ws.printfAll("state;%s", dataJson().c_str());
    }
    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();
      checkSched(false);        // check every minute for next schedule
      if (hour_save != hour()) // update our IP and time daily (at 2AM for DST)
      {
        eemem.update(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
        if( (hour_save = hour()) == 2)
        {
          getUdpTime();
        }
        if(mon_save != month())
        {
          mon_save = month();
          fLastTotalCost = fTotalCost; // shift the cost at the end of the month
          fTotalCost = 0;
        }
      }
    }
    if(displayOnTimer)
    {
      displayOnTimer --;
    }
    if(nWrongPass)
      nWrongPass--;
    if(bHeater)
      onCounter++;
    if(bHeater != bLastOn || onCounter > (60*60*12)) // total up when it turns off or before 32 bit carry error
    {
      if(bLastOn)
      {                       // seconds * (price_per_KWH / 10000) / secs_per_hour * watts
        fTotalCost += (float)(onCounter * watts * ee.ppkwh) / 36000000000;
      }
      bLastOn = bHeater;
      onCounter = 0;
    }
  }
  DrawScreen();
}

void DrawScreen()
{
  static bool blnk = false;
  static long last;
  static int8_t msgCnt = 0;

  // draw the screen here
  display.clear();

  if( (millis() - last) > 400) // 400ms togle for blinker
  {
    last = millis();
    blnk = !blnk;
  }

  if(ee.bEnableOLED || displayOnTimer) // draw only ON indicator if screen off
  {
    display.setFontScale2x2(false); // the small text
    display.drawString( 8, 22, "Temp");
    display.drawString(80, 22, "Set");
    const char *ps = ee.bVaca ? "Vacation" : ee.schNames[schInd];
    display.drawString(90-(strlen(ps) << 2), 55, ps);

    String s;
  
    if(sMessage.length()) // message sent over wifi
    {
      s = sMessage; // custom message
      if(msgCnt == 0) // starting
        msgCnt = 3; // times to repeat message
    }
    else
    {
      s = timeFmt(true, true); // the default scroller
      s += "  ";
      s += days[weekday()-1];
      s += " ";
      s += String(day());
      s += " ";
      s += months[month()-1];
      s += "  ";
      s += sDec(roomTemp);
      s += "]F "; // <- that's a degree symbol
      s += sDec(rh);
      s += "% ";
    }

    Scroller(s);

    String temp = sDec(currentTemp) + "]"; // <- that's a degree symbol
    display.drawPropString(2, 33, temp );
    temp = sDec(ee.schedule[schInd].setTemp) + "]";
    display.drawPropString(70, 33, temp );
    if(bHeater && blnk)
      display.drawString(1, 55, "Heat");
  }
  else if(bHeater)  // small blinky dot when display is off
  {
    if(blnk) display.drawString( 2, 56, ".");
  }
  display.display();
}

// Text scroller optimized for very long lines
void Scroller(String s)
{
  static int16_t ind = 0;
  static char last = 0;
  static int16_t x = 0;

  if(last != s.charAt(0)) // reset if content changed
  {
    x = 0;
    ind = 0;
  }
  last = s.charAt(0);
  int len = s.length(); // get length before overlap added
  s += s.substring(0, 18); // add ~screen width overlap
  int w = display.propCharWidth(s.charAt(ind)); // first char for measure
  String sPart = s.substring(ind, ind + 18);
  display.drawPropString(x, 0, sPart );

  if( --x <= -(w))
  {
    x = 0;
    if(++ind >= len) // reset at last char
    {
      ind = 0;
      if(msgCnt) // end of custom message display
      {
        if(--msgCnt == 0) // decrement times to repeat it
            sMessage = "";
      }
    }
  }
}

// Check temp to turn heater on and off
void checkTemp()
{
  static RunningMedian<uint16_t,16> tempMedian;
  static uint8_t state = 0;

  switch(state)
  {
    case 0: // start a conversion
      ds.reset();
      ds.select(ds_addr);
      ds.write(0x44,0);   // start conversion, no parasite power on at the end
      state++;
      return;
    case 1:
      state++;
      break;
    case 2:
      state = 0; // 1 second rest
      return;
  }

  uint8_t data[10];
  uint8_t present = ds.reset();
  ds.select(ds_addr);
  ds.write(0xBE);         // Read Scratchpad

  if(!present)      // safety
  {
    bHeater = false;
    digitalWrite(HEAT, !bHeater);
    return;
  }

  for ( int i = 0; i < 9; i++)          // we need 9 bytes
    data[i] = ds.read();

  if(OneWire::crc8( data, 8) != data[8])  // bad CRC
  {
    bHeater = false;
    digitalWrite(HEAT, !bHeater);
    Serial.println("Invalid CRC");
    return;
  }

  uint16_t raw = (data[1] << 8) | data[0];

  if(raw > 630 || raw < 200){ // first reading is always 1360 (0x550)
    Serial.print("DS err ");
    Serial.println(raw);
    return;
  }

// tempMedian.add(( raw * 625) / 1000 );  // to 10x celcius
  tempMedian.add( (raw * 1125) / 1000 + 320) ; // 10x fahrenheit

  uint16_t newTemp;
  tempMedian.getMedian(newTemp);

  static uint16_t oldHT;
  if(newTemp != currentTemp || hiTemp != oldHT)
  {
    currentTemp = newTemp;
    oldHT = hiTemp;
    events.send(dataJson().c_str(), "state");
    ws.printfAll("state;%s", dataJson().c_str());
  }

  if(currentTemp <= loTemp && bHeater == false)
  {
    bHeater = true;
    digitalWrite(HEAT, !bHeater);
    events.send(dataJson().c_str(), "state"); // give a more precise account of changes
  }
  else if(currentTemp >= hiTemp && bHeater == true)
  {
    bHeater = false;
    digitalWrite(HEAT, !bHeater);
    events.send(dataJson().c_str(), "state");
    ws.printfAll("state;%s", dataJson().c_str());
  }
}

// Check the buttons
void checkButtons()
{
  static bool bState[2];
  static bool lbState[2];
  static long debounce[2];
  static long lRepeatMillis;

  bool bUp = digitalRead(BTN_UP);
  bool bDn = digitalRead(BTN_DN);

  if(bUp != lbState[0]) debounce[0] = millis(); // reset on state change
  if(bDn != lbState[1]) debounce[1] = millis();

#define REPEAT_DELAY 130

  if ((millis() - debounce[0]) > 20)
  {
    if (bUp != bState[0])
    {
      bState[0] = bUp;
      if (bState[0] == LOW)
      {
        changeTemp(1);
        lRepeatMillis = millis(); // initial increment
        if(ee.bEnableOLED == false)
          displayOnTimer = 30;
      }
    }
    else if(bState[0] == LOW) // holding down
    {
      if( (millis() - lRepeatMillis) > REPEAT_DELAY) // increase for slower repeat
      {
        changeTemp(1);
        lRepeatMillis = millis();
        if(ee.bEnableOLED == false)
          displayOnTimer = 30;
      }
    }
  }

  if ((millis() - debounce[1]) > 20)
  {
    if (bDn != bState[1])
    {
      bState[1] = bDn;
      if (bState[1] == LOW)
      {
        changeTemp(-1);
        lRepeatMillis = millis(); // initial decrement
      }
    }
    else if(bState[1] == LOW) // holding down
    {
      if( (millis() - lRepeatMillis) > REPEAT_DELAY)
      {
        changeTemp(-1);
        lRepeatMillis = millis();
      }
    }
  }

  lbState[0] = bUp;
  lbState[1] = bDn;
}

void changeTemp(int8_t delta)
{
  ee.schedule[schInd].setTemp += delta;
  checkLimits();
}

void checkLimits()
{
  for(int i = 0; i < ee.schedCnt; i++)
  {
    ee.schedule[i].setTemp = constrain(ee.schedule[i].setTemp, 600, 900); // sanity check
    ee.schedule[i].thresh = constrain(ee.schedule[i].thresh, 1, 100); // sanity check
  }
}

void checkSched(bool bUpdate)
{
  long timeNow = (hour()*60) + minute();

  if(bUpdate)
  {
    schInd = ee.schedCnt - 1;
    for(int i = 0; i < ee.schedCnt; i++) // any time check
      if(timeNow >= ee.schedule[i].timeSch && timeNow < ee.schedule[i+1].timeSch)
      {
        schInd = i;
        break;
      }
  }
  else for(int i = 0; i < ee.schedCnt; i++) // on-time check
  {
    if(timeNow == ee.schedule[i].timeSch)
    {
      schInd = i;
      break;
    }
  }

  hiTemp = ee.bVaca ? ee.vacaTemp : ee.schedule[schInd].setTemp;
  loTemp = ee.bVaca ? (hiTemp - 10) : (hiTemp - ee.schedule[schInd].thresh);
  int thresh = ee.schedule[schInd].thresh;

  if(!ee.bVaca && ee.bAvg) // averageing mode
  {
    int start = ee.schedule[schInd].timeSch;
    int range;
    int s2;

    // Find minute range between schedules
    if(schInd == ee.schedCnt - 1) // rollover
    {
      s2 = 0;
      range = ee.schedule[s2].timeSch + (24*60) - start;
    }
    else
    {
      s2 = schInd + 1;
      range = ee.schedule[s2].timeSch - start;
    }

    int m = (hour() * 60) + minute(); // current TOD in minutes

    if(m < start) // offset by start of current schedule
      m -= start - (24*60); // rollover
    else
      m -= start;

    hiTemp = tween(ee.schedule[schInd].setTemp, ee.schedule[s2].setTemp, m, range);
    thresh = tween(ee.schedule[schInd].thresh, ee.schedule[s2].thresh, m, range);
    loTemp = hiTemp - thresh;
  }
}

// avarge value at current minute between times
int tween(int t1, int t2, int m, int range)
{
  if(range == 0) range = 1; // div by zero check
  int t = (t2 - t1) * (m * 100 / range) / 100;
  return t + t1;
}

void DST() // 2016 starts 2AM Mar 13, ends Nov 6
{
  tmElements_t tm;
  breakTime(now(), tm);
  // save current time
  uint8_t m = tm.Month;
  int8_t d = tm.Day;
  int8_t dow = tm.Wday;

  tm.Month = 3; // set month = Mar
  tm.Day = 14; // day of month = 14
  breakTime(makeTime(tm), tm); // convert to get weekday

  uint8_t day_of_mar = (7 - tm.Wday) + 8; // DST = 2nd Sunday

  tm.Month = 11; // set month = Nov (0-11)
  tm.Day = 7; // day of month = 7 (1-30)
  breakTime(makeTime(tm), tm); // convert to get weekday

  uint8_t day_of_nov = (7 - tm.Wday) + 1;

  if ((m  >  3 && m < 11 ) ||
      (m ==  3 && d > day_of_mar) ||
      (m ==  3 && d == day_of_mar && hour() >= 2) ||  // DST starts 2nd Sunday of March;  2am
      (m == 11 && d <  day_of_nov) ||
      (m == 11 && d == day_of_nov && hour() < 2))   // DST ends 1st Sunday of November; 2am
   dst = 1;
 else
   dst = 0;
}


void Tone(uint8_t _pin, unsigned int frequency, unsigned long duration)
{
  analogWriteFreq(frequency);
  analogWrite(_pin, 500);
  delay(duration);
  analogWrite(_pin,0);
}

void getUdpTime()
{
  if(bNeedUpdate) return;
  Serial.println("getUdpTime");
  Udp.begin(2390);
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  // time.nist.gov
  Udp.beginPacket("0.us.pool.ntp.org", 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
  bNeedUpdate = true;
}

bool checkUdpTime()
{
  static int retry = 0;

  if(!Udp.parsePacket())
  {
    if(++retry > 500)
     {
        bNeedUpdate = false;
        getUdpTime();
        retry = 0;
     }
    return false;
  }
  Serial.println("checkUdpTime good");

  // We've received a packet, read the data from it
  Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

  Udp.stop();
  // the timestamp starts at byte 40 of the received packet and is four bytes,
  // or two words, long. First, extract the two words:

  unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  unsigned long secsSince1900 = highWord << 16 | lowWord;
  // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
  const unsigned long seventyYears = 2208988800UL;
  long timeZoneOffset = 3600 * (ee.tz + dst);
  unsigned long epoch = secsSince1900 - seventyYears + timeZoneOffset + 1; // bump 1 second

  // Grab the fraction
//  highWord = word(packetBuffer[44], packetBuffer[45]);
//  lowWord = word(packetBuffer[46], packetBuffer[47]);
//  unsigned long d = (highWord << 16 | lowWord) / 4295000; // convert to ms

  setTime(epoch);
  DST(); // check the DST and reset clock
  timeZoneOffset = 3600 * (ee.tz + dst);
  epoch = secsSince1900 - seventyYears + timeZoneOffset + 1; // bump 1 second
  setTime(epoch);
  
//  Serial.print("Time ");
//  Serial.println(timeFmt(true, true));
  bNeedUpdate = false;
  return true;
}
