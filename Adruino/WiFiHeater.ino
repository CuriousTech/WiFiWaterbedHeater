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

//uncomment to enable Arduino IDE Over The Air update code
#define OTA_ENABLE

//#define USE_SPIFFS

#include <Wire.h>
#include <ssd1306_i2c.h> // https://github.com/CuriousTech/WiFi_Doorbell/tree/master/Libraries/ssd1306_i2c
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#ifdef OTA_ENABLE
#include <FS.h>
#include <ArduinoOTA.h>
#endif
#ifdef USE_SPIFFS
#include <FS.h>
#include <SPIFFSEditor.h>
#else
#include "pages.h"
#endif
#include <OneWire.h>
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <UdpTime.h> // https://github.com/CuriousTech/ESP07_WiFiGarageDoor/tree/master/libraries/UdpTime
#include <DHT.h>  // http://www.github.com/markruys/arduino-DHT
#include "eeMem.h"
#include "RunningMedian.h"
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse

const char controlPassword[] = "password";    // device password for modifying any settings
const int serverPort = 82;                    // HTTP port
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

UdpTime utime;

uint16_t roomTemp;
uint16_t rh;

int currentTemp = 850;
int hiTemp; // current target
int loTemp;
bool bHeater = false;
uint8_t schInd = 0;
String sMessage;
uint8_t msgCnt;
uint32_t onCounter;
float fTotalCost;

int toneFreq;
int tonePeriod;
bool bTone;

struct tempArr{
  uint8_t h;
  uint8_t m;
  uint16_t temp;
  uint8_t state;
  uint8_t res;
};
#define LOG_CNT 98
tempArr tempArray[LOG_CNT];

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);

const char days[7][4] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
const char months[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

float currCost()
{
  float fCurTotalCost = fTotalCost;
  if(onCounter) fCurTotalCost += (float)onCounter * (float)watts  / 10000.0 * (float)ee.ppkwh / 3600000.0; // add current cycle
  return fCurTotalCost;
}

String dataJson()
{
  String s = "{";
  s += "\"t\": ";  s += now() - ((ee.tz + utime.getDST()) * 3600);
  s += ",\"waterTemp\": "; s += sDec(currentTemp);
  s += ",\"setTemp\": ";  s += sDec(ee.schedule[schInd].setTemp);
  s += ", \"hiTemp\": ";  s += sDec(hiTemp);
  s += ", \"loTemp\": ";  s += sDec(loTemp);
  s += ", \"on\": ";    s += bHeater;
  s += ", \"temp\": ";  s += sDec(roomTemp);
  s += ", \"rh\": ";   s += sDec(rh);
  s += ", \"tc\": ";   s += String(currCost(), 3);
  s += "}";
  return s;
}

String setJson() // settings
{
  String s = "{\"item\":[";
  
  for(int i = 0; i < 8; i++)
  {
    if(i) s += ",";
    s += "[\"";  s += ee.schedule[i].name; s += "\",\"";
    int h = ee.schedule[i].timeSch / 60;
    int m = ee.schedule[i].timeSch % 60;
    s += h;  s += ":";
    if(m<10) s += "0";
    s += m;
    s += "\","; s += sDec(ee.schedule[i].setTemp);
    s += ",";  s += sDec(ee.schedule[i].thresh);
    s += "]";
  }
  s += "]";
  s += ",\"vt\":";   s += sDec(ee.vacaTemp);
  s += ",\"o\":";   s += ee.bEnableOLED;
  s += ",\"tz\":";  s += ee.tz;
  s += ",\"avg\":";  s += ee.bAvg;
  s += ",\"ppkwh\":"; s += ee.ppkwh;
  s += ",\"vo\":";   s += ee.bVaca;
  s += ",\"idx\":";  s += schInd;
  s += ",\"cnt\":";  s += ee.schedCnt;
  s += ",\"r\":"; s += ee.rate;
  s += ",\"tc\":[";
  for(int i = 0; i < 12; i++)
  {
    if(i) s += ",";
    s += "\"";
    s += String((float)ee.costs[i] / 100);
    s += "\"";
  }
  s += "]}";
  return s;
}

void parseParams(AsyncWebServerRequest *request)
{
  static char temp[256];
  char password[64];
  int val;

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
      case 'r': // rate
        if(val == 0) val = 1; // don't allow 0
        ee.rate = val;
        break;
      case 'f': // frequency
        toneFreq = val;
        bTone = true;
        break;
      case 'b': // beep : ?f=1200&b=1000
        tonePeriod = val;
        bTone = true;
        break;
      case 's':
        s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
        break;
      case 'p':
        wifi.setPass(s.c_str());
        break;
      case 't': // restore the month's total (in cents)
        fTotalCost = (float)val / 100;
        break;
    }
  }
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

void onEvents(AsyncEventSourceClient *client)
{
//  client->send(":ok", NULL, millis(), 1000);
  static bool rebooted = true;
  if(rebooted)
  {
    rebooted = false;
    events.send("Restarted", "alert");
  }
  events.send(dataJson().c_str(), "state");
}

const char *jsonListCmd[] = { "cmd",
  "key",
  "oled",
  "TZ",
  "avg",
  "cnt",
  "tadj", // 5
  "ppkwh",
  "vaca",
  "vacatemp",
  "I",
  "N", // 10
  "S",
  "T",
  "H",
  "beepF",
  "beepP", // 15
  NULL
};

bool bKeyGood;

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue)
{
  if(!bKeyGood && iName) return; // only allow for key
  char *p, *p2;
  static int beepPer = 1000;
  static int item = 0;

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
          utime.start();
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
        case 9: // I
          item = iValue;
          break;
        case 10: // N
          strncpy(ee.schedule[item].name, psValue, sizeof(ee.schedule[0].name) );
          break;
        case 11: // S
          p = strtok(psValue, ":");
          p2 = strtok(NULL, "");
          if(p && p2){
            iValue *= 60;
            iValue += atoi(p2);
          }
          ee.schedule[item].timeSch = iValue;
          break;
        case 12: // T
          ee.schedule[item].setTemp = atof(psValue)*10;
          break;
        case 13: // H
          ee.schedule[item].thresh = (int)(atof(psValue)*10);
          checkLimits();      // constrain and check new values
          checkSched(true);   // reconfigure to new schedule
          break;
       case 14: // beepF
          bTone = true;
          toneFreq = iValue;
          break;
       case 15: // beepP (beep period)
          tonePeriod = iValue;
          bTone = true;
          break;
      }
      break;
  }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  static bool bRestarted = true;

  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      if(bRestarted)
      {
        bRestarted = false;
        client->printf("alert;restarted");
      }
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
          ws.printfAll("set;%s", setJson().c_str()); // update the page settings
        }
      }
      break;
  }
}

const char hostName[] = "Waterbed";

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

  WiFi.hostname(hostName);
  wifi.autoConnect(hostName, controlPassword); // Tries config AP, then starts softAP mode for config
  if(!wifi.isCfg() == false)
  {
    if ( !MDNS.begin ( hostName, WiFi.localIP() ) )
      Serial.println ( "MDNS responder failed" );
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }

#ifdef USE_SPIFFS
  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", controlPassword));
#endif

  // attach AsyncEventSource
  events.onConnect(onEvents);
  server.addHandler(&events);
  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    if(wifi.isCfg())
      request->send( 200, "text/html", wifi.page() ); // WIFI config page
  });

  server.on ( "/iot", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest *request) // Main webpage interface
  {
    parseParams(request);
    #ifdef USE_SPIFFS
      request->send(SPIFFS, "/index.htm");
    #else
      request->send_P(200, "text/html", page1);
    #endif
  });
  server.on ( "/s", HTTP_GET | HTTP_POST, handleS );
  server.on ( "/set", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send( 200, "text/json", setJson() );
  });
  server.on ( "/json", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send( 200, "text/json", dataJson() );
  });

  server.onNotFound([](AsyncWebServerRequest *request){ // be silent
//    request->send(404);
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    int n = WiFi.scanComplete();
    if(n == -2){
      WiFi.scanNetworks(true);
    } else if(n){
      for (int i = 0; i < n; ++i){
        if(i) json += ",";
        json += "{";
        json += "\"rssi\":"+String(WiFi.RSSI(i));
        json += ",\"ssid\":\""+WiFi.SSID(i)+"\"";
        json += ",\"bssid\":\""+WiFi.BSSIDstr(i)+"\"";
        json += ",\"channel\":"+String(WiFi.channel(i));
        json += ",\"secure\":"+String(WiFi.encryptionType(i));
        json += ",\"hidden\":"+String(WiFi.isHidden(i)?"true":"false");
        json += "}";
      }
      WiFi.scanDelete();
      if(WiFi.scanComplete() == -2){
        WiFi.scanNetworks(true);
      }
    }
    json += "]";
    request->send(200, "text/json", json);
    json = String();
  });
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "tdata=[";
    bool bSent = false;
    for (int i = 0; i <= 96; ++i){
      if(tempArray[i].state == 2) // now
      {
        tempArray[i].h = hour();
        tempArray[i].m = minute();
        tempArray[i].temp = currentTemp;
      }
      if(tempArray[i].temp) // only send entries in use (hitting a limit when SSL is compiled in)
      {
        if(bSent) json += ",";
        json += "{";
        json += "\"tm\":\"" + String(tempArray[i].h) + ":" + String(tempArray[i].m) + "\"";
        json += ",\"t\":\"" + sDec(tempArray[i].temp) + "\"";
        json += ",\"s\":" + String(tempArray[i].state);
        json += "}";
        bSent = true;
      }
    }
    json += "]";
    request->send(200, "text/json", json);
    json = String();
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){ // close this one quick
    request->send(404);
  });

  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });
  server.begin();

  MDNS.addService("http", "tcp", serverPort);
#ifdef OTA_ENABLE
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    eemem.update();
  });
#endif

  jsonParse.addList(jsonListCmd);

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

  if(wifi.isCfg() == false)
    utime.start();

  Tone(2600, 200);
  dht.setup(EXPAN1, DHT::DHT22);
}

uint16_t ssCnt = 30;

void loop()
{
  static RunningMedian<uint16_t,24> tempMedian;
  static RunningMedian<uint16_t,24> rhMedian;
  static uint8_t min_save, sec_save, mon_save;
  static bool bLastOn;

  MDNS.update();
#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif

  checkButtons();
  if(!wifi.isCfg())
    if(utime.check(ee.tz))
      checkSched(true);  // initialize

  if(bTone)
  {
    Tone(toneFreq, tonePeriod);
    bTone = false;
  }
  
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
        float t;
        rhMedian.getAverage(2, t);
        rh = t;
        tempMedian.add((uint16_t)(dht.toFahrenheit(dht.getTemperature()) * 10));
        tempMedian.getAverage(2, t);
        roomTemp = t;
      }
    }

    static int ka = 10;
    if(--ssCnt == 0)
    {
       sendState();
       ka = 10;
    }else if(--ka == 0)
    {
      events.send("","");
      ka = 10;
    }

    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();
      checkSched(false);     // check every minute for next schedule
      if( min_save == 0)     // update our IP and time daily (at 2AM for DST)
      {
        if( hour() == 2)
        {
          utime.start();
        }
        if(mon_save != month())
        {
          ee.costs[mon_save-1] = fTotalCost * 100; // shift the cost at the end of the month
          mon_save = month();
          fTotalCost = 0;
        }
        addLog();
        eemem.update();      // update EEPROM if needed while we're at it (give user time to make many adjustments)
      }
      if(min_save == 30)
        addLog(); // half hour log
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
        fTotalCost += (float)onCounter * (float)watts / 10000.0 * (float)ee.ppkwh / 3600000.0;
      }
      bLastOn = bHeater;
      onCounter = 0;
    }
  }
  if(wifi.isCfg())
    return;

  // Draw screen
  static bool blnk = false;
  static long last;

  // draw the screen here
  display.clear();

  if(ee.bEnableOLED || displayOnTimer) // draw only ON indicator if screen off
  {
    display.setFontScale2x2(false); // the small text
    display.drawString( 8, 22, "Temp");
    display.drawString(80, 22, "Set");
    const char *ps = ee.bVaca ? "Vacation" : ee.schedule[schInd].name;
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
    if(bHeater)
    {
      if( (millis() - last) > 400) // 400ms toggle for blinker
      {
        last = millis();
        blnk = !blnk;
      }
      if(blnk)
        display.drawString(1, 55, "Heat");
    }
  }
  else if(bHeater)  // small strobe dot when display is off
  {
    if(second() != last)
    {
      display.drawString( 60, 30, ".");
      last = second();
    }
  }
  display.display();
}

void addLog()
{
  int iPos = hour() << 2; //+ ( minute() / 15); // 4 per hour
  if( minute() ) // force 1 and 3 slots
  {
    if(minute() < 30)
      iPos ++;
    else if(minute() == 30)
      iPos += 2;
    else
      iPos += 3;
  }
  if(iPos == 0) //fill in to 24:00
  {
    if(tempArray[95].state == 2)
      memset(&tempArray[95], 0, sizeof(tempArr));
    tempArray[96].h = 24;
    tempArray[96].m = 0;
    tempArray[96].temp = currentTemp;
    tempArray[96].state = bHeater;
  }
  tempArray[iPos].h = hour();
  tempArray[iPos].m = minute();
  tempArray[iPos].temp = currentTemp;
  tempArray[iPos].state = bHeater;

  if(iPos)
    if(tempArray[iPos-1].state == 2)
      memset(&tempArray[iPos-1], 0, sizeof(tempArr));

  tempArray[iPos+1].temp = currentTemp;
  tempArray[iPos+1].state = 2;  // use 2 as a break between old and new
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
  int w = display.propCharWidth(s.charAt(ind)); // measure first char
  if(w > 100) w = 10; // bug in function (0 returns 11637)
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
            sMessage = String();
      }
    }
  }
}

// Check temp to turn heater on and off
void checkTemp()
{
  static RunningMedian<uint16_t,32> tempMedian;
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
    events.send("Invalid CRC", "alert");
    return;
  }

  uint16_t raw = (data[1] << 8) | data[0];

  if(raw > 630 || raw < 200){ // first reading is always 1360 (0x550)
    events.send("DS err ", "alert");
    return;
  }

// tempMedian.add(( raw * 625) / 1000 );  // to 10x celcius
  tempMedian.add( (raw * 1125) / 1000 + 320) ; // 10x fahrenheit

  float t;
  tempMedian.getAverage(2, t);
  uint16_t newTemp = t;

  static uint16_t oldHT;
  if(newTemp != currentTemp || hiTemp != oldHT)
  {
    currentTemp = newTemp;
    oldHT = hiTemp;
    sendState();
  }

  if(currentTemp <= loTemp && bHeater == false)
  {
    bHeater = true;
    digitalWrite(HEAT, !bHeater);
    sendState();
    addLog();
  }
  else if(currentTemp >= hiTemp && bHeater == true)
  {
    bHeater = false;
    digitalWrite(HEAT, !bHeater);
    sendState();
    addLog();
  }
}

void sendState()
{
  events.send(dataJson().c_str(), "state");
  ws.printfAll("state;%s", dataJson().c_str());
  ssCnt = ee.rate;
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

void changeTemp(int delta)
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

void Tone(unsigned int frequency, unsigned long duration)
{
  analogWriteFreq(frequency);
  analogWrite(TONE, 500);
  delay(duration);
  analogWrite(TONE,0);
}
