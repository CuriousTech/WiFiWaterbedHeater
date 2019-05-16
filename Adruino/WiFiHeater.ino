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

// #define SDEBUG
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
#include "eeMem.h"
#include "RunningMedian.h"
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse
#include <AM2320.h>
#include "jsonstring.h"

const char controlPassword[] = "password";    // device password for modifying any settings
const int serverPort = 80;                    // HTTP port
//const char hostName[] = "Waterbed";
const char hostName[] = "Waterbed2";

#define BTN_2     0 // right top
#define BTN_1     1 // left top
#define VIBE      2  // side expansion pad
#define ESP_LED   2  //Blue LED on ESP07 (on low)
#define SDA       4
#define SCL       5  // OLED and AM2320
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
AM2320 am;
uint16_t light;

eeMem eemem;

WiFiManager wifi;  // AP page:  192.168.4.1
AsyncWebServer server( serverPort );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

UdpTime utime;

uint16_t roomTemp;
uint16_t rh;

int currentTemp = 860;
int hiTemp; // current target
int loTemp;
bool bHeater = false;
uint8_t schInd = 0;
String sMessage;
uint8_t msgCnt;
uint32_t onCounter;
bool bStartDisplay;

int toneFreq = 1000;
int tonePeriod = 100;
int toneCnt;

int vibePeriod = 200;
int vibeCnt;

struct tempArr{
  uint16_t min;
  uint16_t temp;
  uint8_t state;
  uint16_t rm;
  uint16_t rh;
};
#define LOG_CNT 98
tempArr tempArray[LOG_CNT];

void jsonCallback(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);

const char days[7][4] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
const char months[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

float currCost()
{
        // seconds * (price_per_KWH / 10000) / secs_per_hour * watts
  float fCurTotalCost = ee.fTotalCost;
  if(onCounter) fCurTotalCost += (float)onCounter * (float)ee.watts  / 10000.0 * (float)ee.ppkwh / 3600000.0; // add current cycle
  return fCurTotalCost;
}

float currWatts()
{
  float fCurTotalWatts = ee.fTotalWatts;
  if(onCounter) fCurTotalWatts += (float)onCounter * (float)ee.watts  / 3600; // add current cycle
  return fCurTotalWatts;
}

bool updateAll(bool bForce)
{
  ee.fTotalCost = currCost();
  ee.fTotalWatts = currWatts();
  return eemem.update(bForce);
}

String dataJson()
{
  jsonString js;

  js.Var("t", (uint32_t)(now() - ((ee.tz + utime.getDST()) * 3600)) );
  js.Var("waterTemp", String((float)currentTemp/10, 1) );
  js.Var("setTemp", String((float)ee.schedule[schInd].setTemp/10, 1) );
  js.Var("hiTemp",  String((float)hiTemp/10, 1) );
  js.Var("loTemp",  String((float)loTemp/10, 1) );
  js.Var("on",   (digitalRead(HEAT)==LOW) ? 1:0 );
  js.Var("temp", String((float)roomTemp/10, 1) );
  js.Var("rh",   String((float)rh/10, 1) );
  js.Var("tc",   String(currCost(), 3) );
  js.Var("wh",   String(currWatts(), 1) );
  js.Var("l",   light);

  return js.Close();
}

String setJson() // settings
{
  jsonString js;

  js.Var("vt", String((float)ee.vacaTemp/10, 1) );
  js.Var("o",   ee.bEnableOLED);
  js.Var("tz",  ee.tz);
  js.Var("avg", ee.bAvg);
  js.Var("ppkwh",ee.ppkwh);
  js.Var("vo",  ee.bVaca);
  js.Var("idx", schInd);
  js.Var("cnt", ee.schedCnt);
  js.Var("w",  ee.watts);
  js.Var("r",  ee.rate);
  js.Var("e",  ee.bEco);
  js.Array("item", ee.schedule, 8);
  js.ArrayCost("tc", ee.costs, 12);
  js.Array("tw", ee.wh, 12);

  return js.Close();
}

void ePrint(const char *s)
{
  ws.textAll(String("print;") + s);
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

    jsonString js("hack");
    js.Var("ip", request->client()->remoteIP().toString() );
    js.Var("pass", password);
    ws.textAll(js.Close());
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
          bStartDisplay = true;
          displayOnTimer = 60;
        }
        break;
      case 'r': // rate
        if(val == 0)
          break; // don't allow 0
        ee.rate = val;
        sendState();
        break;
      case 'a':
        updateAll(true);
        delay(100);
        ESP.reset();
        break;
      case 'b': // beep : /s?key=pass&b=1000,800,3 (tone or ms,tone,cnt)
        toneCnt = 1;
        {
          int n = s.indexOf(",");
          if(n >0)
          {
            tonePeriod = constrain(val, 10, 1000);
            s.remove(0, n+1);
            val = s.toInt();
          }
          toneFreq = val;
          n = s.indexOf(",");
          if(n >0)
          {
            s.remove(0, n+1);
            toneCnt = constrain(s.toInt(), 1, 20);
          }
        }
        break;
      case 's':
        s.toCharArray(ee.szSSID, sizeof(ee.szSSID));
        break;
      case 'p':
        s.toCharArray(ee.szSSIDPassword, sizeof(ee.szSSIDPassword));
        updateAll( false );
        wifi.setPass();
        break;
      case 'c': // restore the month's total (in cents) after updates
        ee.fTotalCost = (float)val / 100;
        break;
      case 'w': // restore the month's total (in watthours) after updates
        ee.fTotalWatts = (float)val;
        break;
      case 'v': // vibe
        {
          int n = s.indexOf(",");
          if(n >0)
          {
            s.remove(0, n+1);
            vibeCnt = s.toInt();
          }
        }
        vibePeriod = val;
        pinMode(VIBE, OUTPUT);
        digitalWrite(VIBE, LOW);
        break;
      case 't': // tadj[1] (for room temp)
        ee.tAdj[1] = constrain(val, -200, 200);
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
  parseParams(request);

  jsonString js;
  String s = WiFi.localIP().toString() + ":";
  s += serverPort;
  js.Var("ip", s);
  request->send ( 200, "text/json", js.Close() );
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
  "vibe",
  "watts",
  "dot",
  "save",
  "aadj", // 20
  "eco",
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
          if( ee.bEnableOLED == false && iValue)
            bStartDisplay = true;
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
          ws.textAll(String("set;") + setJson()); // update all the entries
          break;
        case 5: // tadj
          changeTemp(iValue, false);
          ws.textAll(String("set;") + setJson()); // update all the entries
          break;
        case 6: // ppkw
          ee.ppkwh = iValue;
          break;
        case 7: // vaca
          ee.bVaca = iValue;
          break;
        case 8: // vacatemp
          ee.vacaTemp = constrain( (int)(atof(psValue)*10), 600, 840); // 60-84F
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
          toneCnt = 1;
          toneFreq = iValue;
          break;
       case 15: // beepP (beep period)
          tonePeriod = iValue;
          toneCnt = 1;
          break;
       case 16: // vibe (vibe period)
          vibePeriod = iValue;
          vibeCnt = 1;
          pinMode(VIBE, OUTPUT);
          digitalWrite(VIBE, LOW);
          break;
        case 17: // watts
          ee.watts = iValue;
          break;
        case 18: // dot displayOnTimer
          if((displayOnTimer ==0 ) && (displayOnTimer = iValue))
          {
            bStartDisplay = true;
          }
          break;
        case 19: // save
          updateAll(true);
          break;
        case 20: // aadj
          changeTemp(iValue, true);
          ws.textAll(String("set;") + setJson()); // update all the entries
          break;
        case 21: // eco
          ee.bEco = iValue ? true:false;
          break;
      }
      break;
  }
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{  //Handle WebSocket event
  static bool bRestarted = true;
  String s;

  switch(type)
  {
    case WS_EVT_CONNECT:      //client connected
      if(bRestarted)
      {
        bRestarted = false;
        client->text("alert;Restarted");
      }
      s = String("state;") + dataJson().c_str();
      client->text(s);
      s = String("set;") + setJson().c_str();
      client->text(s);
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
          ws.textAll(String("set;") + setJson()); // update the page settings
        }
      }
      break;
  }
}

void setup()
{
#ifdef SDEBUG
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("Starting");
#else
  pinMode(BTN_1, INPUT_PULLUP);
#endif

  pinMode(BTN_2, INPUT_PULLUP);  // IO0 needs external WPU
  pinMode(TONE, OUTPUT);
  digitalWrite(TONE, LOW);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DN, INPUT); // IO16 has external pullup
  pinMode(HEAT, OUTPUT);
  digitalWrite(HEAT, HIGH); // high is off

  // initialize dispaly
  display.init();
  display.flipScreenVertically();

  WiFi.hostname(hostName);
  wifi.autoConnect(hostName, controlPassword); // Tries config AP, then starts softAP mode for config
  if(!wifi.isCfg() == false)
  {
    MDNS.begin ( hostName, WiFi.localIP() );
#ifdef SDEBUG
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
#endif
  }

#ifdef USE_SPIFFS
  SPIFFS.begin();
  server.addHandler(new SPIFFSEditor("admin", controlPassword));
#endif

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
      request->send_P(200, "text/html", index_page);
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
        jsonString js;

        js.Var("rssi", String(WiFi.RSSI(i)) );
        js.Var("ssid", WiFi.SSID(i) );
        js.Var("bssid", WiFi.BSSIDstr(i) );
        js.Var("channel", String(WiFi.channel(i)) );
        js.Var("secure", String(WiFi.encryptionType(i)) );
        js.Var("hidden", String(WiFi.isHidden(i)?"true":"false") );
        json += js.Close();
      }
      WiFi.scanDelete();
      if(WiFi.scanComplete() == -2){
        WiFi.scanNetworks(true);
      }
    }
    json += "]";
    request->send(200, "text/json", json);
  });
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "tdata=[";
    bool bSent = false;
    for (int i = 0; i <= 96; ++i){
      if(tempArray[i].state == 2) // now
      {
        tempArray[i].min = (hour()*60) + minute();
        tempArray[i].temp = currentTemp;
        tempArray[i].rm = roomTemp;
        tempArray[i].rh = rh;
      }
      if(tempArray[i].temp) // only send entries in use (hitting a limit when SSL is compiled in)
      {
        if(bSent) json += ",";
        jsonString js;
        js.Var("tm", tempArray[i].min );
        js.Var("t", String(sDec(tempArray[i].temp)) );
        js.Var("s", tempArray[i].state );
        js.Var("rm", String( sDec(tempArray[i].rm) ) );
        js.Var("rh", String( sDec(tempArray[i].rh) ) );
        json += js.Close();
        bSent = true;
      }
    }
    json += "]";
    request->send(200, "text/json", json);
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
#ifdef USE_SPIFFS
      request->send(SPIFFS, "/favicon.ico");
#else
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
#endif
  });

  server.onFileUpload([](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  });
  server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });
  server.begin();

#ifdef OTA_ENABLE
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    digitalWrite(HEAT, HIGH);
    updateAll( true );
  });
#endif
  jsonParse.addList(jsonListCmd);

  if( ds.search(ds_addr) )
  {
 #ifdef SDEBUG
    Serial.print("OneWire device: "); // 28 22 92 29 7 0 0 6B
    for( int i = 0; i < 8; i++) {
      Serial.print(ds_addr[i], HEX);
      Serial.print(" ");
    }
    Serial.println("");
    if( OneWire::crc8( ds_addr, 7) != ds_addr[7]){
      Serial.println("Invalid CRC");
    }
 #endif
  }
  else
  {
#ifdef SDEBUG
    Serial.println("No OneWire devices");
 #endif
  }

  if(wifi.isCfg() == false)
  {
    utime.start();
    MDNS.addService("http", "tcp", serverPort);
  }

  Tone(2600, 200);
  bStartDisplay = ee.bEnableOLED;
}

uint16_t ssCnt = 30;

void loop()
{
  static RunningMedian<uint16_t,24> tempMedian[2];
  static RunningMedian<uint16_t,8> lightMedian;
  static uint8_t min_save, sec_save, mon_save = 0;
  static bool bLastOn;
  static bool bSkip;

  MDNS.update();
#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif
  checkButtons();
  if(!wifi.isCfg())
    if(utime.check(ee.tz))
      checkSched(true);  // initialize

  if(toneCnt)
  {
    Tone(toneFreq, tonePeriod);
    if(--toneCnt)
      delay(tonePeriod);
  }

  if(vibeCnt)
  {
    delay(vibePeriod);
    pinMode(VIBE, INPUT);
    digitalWrite(VIBE, HIGH);
    if(--vibeCnt)
    {
      pinMode(VIBE, OUTPUT);
      digitalWrite(VIBE, LOW);
    }
  }

  if(bStartDisplay)
  {
    bStartDisplay = false;
    display.init();
    display.flipScreenVertically();
  }

  if(sec_save != second()) // only do stuff once per second
  {
    sec_save = second();

    checkTemp();
/*    static uint8_t am_cnt = 5;
    if(--am_cnt == 0)
    {
      am_cnt = 30;

    }
*/
    if(--ssCnt == 0)
       sendState();

    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();
      checkSched(false);     // check every minute for next schedule
      if( min_save == 0)     // update clock daily (at 2AM for DST)
      {
        if( hour() == 2)
        {
          utime.start();
          updateAll(true);      // update EEPROM daily
        }
        if( mon_save != month() )
        {
          if(mon_save) // first hour after start hour
          {
            ee.costs[mon_save-1] = ee.fTotalCost * 100; // f dollars to dec cents at the end of the month
            ee.fTotalCost = 0;
            ee.wh[mon_save-1] = ee.fTotalWatts; // save watt hours used at the end of the month
            ee.fTotalWatts = 0;
          }
          mon_save = month();
        }
        addLog();
        if( updateAll(false) )      // update EEPROM if changed
          ws.textAll("print; EE updated");
      }
      if(min_save == 30)
        addLog(); // half hour log

      float temp2, rh2;
      if(am.measure(temp2, rh2))
      {
        float newtemp, newrh;

        tempMedian[0].add( (1.8 * temp2 + 32.0) * 10 );
        tempMedian[0].getAverage(2, newtemp);
        tempMedian[1].add(rh2 * 10);
        tempMedian[1].getAverage(2, newrh);
        if(roomTemp != newtemp)
        {
          roomTemp = newtemp;
          rh = newrh;
          sendState();
        }
      }else
      {
        String s;
        s = "print;AM2320 error ";
        s += bHeater;
        ws.textAll(s);
      }
    
    }

    if(displayOnTimer) displayOnTimer --;
    if(nWrongPass)    nWrongPass--;
    if(digitalRead(HEAT) == LOW) onCounter++;

    static uint16_t s = 1;
    if(ee.bEco && bHeater) // eco mode
    {
      bool bBoost = (currentTemp < loTemp - 10); // > 1.0 deg = full power
      if(bBoost == false)
      {
        if(--s == 0)
        {
          s = 60; // turn on/off every 60 seconds
          digitalWrite(HEAT, !digitalRead(HEAT));
        }
      }
    }

    if(bHeater != bLastOn || onCounter > (60*60*12)) // total up when it turns off or before 32 bit carry error
    {
      if(bLastOn)
      {
        updateAll( false );
      }
      bLastOn = bHeater;
      onCounter = 0;
    }

  }

  static int lgTime;
  if(millis() - lgTime >= 100)
  {
    lgTime = millis();
    uint16_t newLight;
    lightMedian.add( analogRead(0) );
    lightMedian.getMedian(newLight);
    if(newLight > light + 1) // light increased
      displayOnTimer = 30;
    light = newLight;
  }

  if(wifi.isCfg())
    return;
  if(bSkip){bSkip = false; return;}
 
  // Draw screen
  static bool blnk;
  static long last;
  static bool bClear;

  if(bClear && !(ee.bEnableOLED || displayOnTimer))
    return;
  // draw the screen here
  display.clear();

  if(ee.bEnableOLED || displayOnTimer) // draw only ON indicator if screen off
  {
    String s;

    bClear = false;

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
    static uint16_t x[2] = {0, 64};

    if(minute()&1)
      x[0] = 64, x[1] = 0;
    else
      x[0] = 0, x[1] = 64;
 
    display.setFontScale2x2(false); // the small text
    display.drawString(x[0]+ 9, 22, "Temp");
    display.drawString(x[1]+19, 22, "Set");
    const char *ps = ee.bVaca ? "Vacation" : ee.schedule[schInd].name;
    display.drawString(x[1]+32-(strlen(ps) << 2), 55, ps);

    String temp = sDec(currentTemp) + "]"; // <- that's a degree symbol
    display.drawPropString(x[0]+2, 33, temp );
    temp = sDec( hiTemp ) + "]";
    display.drawPropString(x[1]+10, 33, temp );

    if(bHeater)
    {
      if( (millis() - last) > 400) // 400ms toggle for blinker
      {
        last = millis();
        blnk = !blnk;
      }
      if(blnk)
        display.drawString(x[0]+1, 55, "Heat");
    }
  }
  else if(bHeater)  // small strobe dot when display is off
  {
    if(second() != last)
    {
      display.drawString( (second()<<1) + (minute()&1), minute()%30, ".");
      last = second();
    }
  }
  else bClear = true; // display has been cleared
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
    tempArray[96].min = 24*60;
    tempArray[96].temp = currentTemp;
    tempArray[96].state = bHeater;
    tempArray[96].rm = roomTemp;
    tempArray[96].rh = rh;
  }
  tempArray[iPos].min = (hour() * 60) + minute();
  tempArray[iPos].temp = currentTemp;
  tempArray[iPos].state = bHeater;
  tempArray[iPos].rm = roomTemp;
  tempArray[iPos].rh = rh;

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

void setHeat()
{
  digitalWrite(HEAT, !bHeater);
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
      ds.write(0x44, 0);   // start conversion, no parasite power on at the end
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
    setHeat();
    return;
  }

  for ( int i = 0; i < 9; i++)          // we need 9 bytes
    data[i] = ds.read();

  if(OneWire::crc8( data, 8) != data[8])  // bad CRC
  {
    bHeater = false;
    setHeat();
    ws.textAll("alert;Invalid CRC");
    return;
  }

  uint16_t raw = (data[1] << 8) | data[0];

  if(raw > 630 || raw < 200){ // first reading is always 1360 (0x550)
    ws.textAll("alert;DS error");
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
    setHeat();
    sendState();
    addLog();
  }
  else if(currentTemp >= hiTemp && bHeater == true)
  {
    bHeater = false;
    setHeat();
    sendState();
    addLog();
  }
}

void sendState()
{
  ws.textAll(String("state;") + dataJson() );
  ssCnt = ee.rate;
}

// Check the buttons
void checkButtons()
{
#define BTN_CNT 4
  static bool bState[BTN_CNT];
  static bool bNewState[BTN_CNT];
  static bool lbState[BTN_CNT];
  static long debounce[BTN_CNT];
  static long lRepeatMillis;
  static bool bRepeat;

  static uint8_t buttons[] = {BTN_UP, BTN_DN, BTN_1, BTN_2};

#define REPEAT_DELAY 200 // increase for slower repeat

  for(int i = 0; i < BTN_CNT; i++)
  {
    bNewState[i] = digitalRead(buttons[i]);
    if(bNewState[i] != lbState[i])
      debounce[i] = millis(); // reset on state change

    bool bInvoke = false;
    if((millis() - debounce[i]) > 30)
    {
      if(bNewState[i] != bState[i]) // press or release
      {
        bState[i] = bNewState[i];
        if (bState[i] == LOW) // pressed
        {
          if(ee.bEnableOLED == false && displayOnTimer == 0) // skip first press with display off
            displayOnTimer = 30;
          else
            bInvoke = true;
          lRepeatMillis = millis(); // initial increment (doubled)
          bRepeat = false;
        }
      }
      else if(bState[i] == LOW) // holding down
      {
        if( (millis() - lRepeatMillis) > REPEAT_DELAY * (bRepeat?1:2) )
        {
          bInvoke = true;
          lRepeatMillis = millis();
          bRepeat = true;
        }
      }
    }

    if(bInvoke) switch(i)
    {
      case 0: // temp up
        if(ee.bVaca) // first tap, disable vacation mode
          ee.bVaca = false;
        else
          changeTemp(1, true);
        break;
      case 1: // temp down
        if(ee.bVaca) // disable vacation mode
          ee.bVaca = false;
        else
          changeTemp(-1, true);
        break;
      case 2: // left top button
        if(displayOnTimer)
          displayOnTimer = 0;
        else
          displayOnTimer = 30;
        break;
      case 3: // right top button
        ee.bEnableOLED = !ee.bEnableOLED;
        if(ee.bEnableOLED)
          bStartDisplay = true;
        displayOnTimer = 0;
        break;
    }
    lbState[i] = bNewState[i];
  }
}

void changeTemp(int delta, bool bAll)
{
  if(ee.bVaca) return;
  if(bAll)
  {
    for(int i = 0; i < ee.schedCnt; i++)
      ee.schedule[i].setTemp += delta;
  }
  else if(ee.bAvg) // bump both used in avg mode
  {
    ee.schedule[schInd].setTemp += delta;
    ee.schedule[ (schInd+1) % ee.schedCnt].setTemp += delta;
  }
  else
  {
    ee.schedule[schInd].setTemp += delta;    
  }
  checkLimits();
  checkSched(false);     // update temp
}

void checkLimits()
{
  for(int i = 0; i < ee.schedCnt; i++)
  {
    ee.schedule[i].setTemp = constrain(ee.schedule[i].setTemp, 600, 900); // sanity check (60~90)
    ee.schedule[i].thresh = constrain(ee.schedule[i].thresh, 1, 100); // (50~80)
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
