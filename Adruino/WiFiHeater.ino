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

#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <WiFiUdp.h>
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <Event.h>  // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/Event
#include <DHT.h>  // http://www.github.com/markruys/arduino-DHT

const char *controlPassword = "password"; // device password for modifying any settings
int serverPort = 82;                    // port fwd for fwdip.php

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

DHT dht;

WiFiManager wifi(0);  // AP page:  192.168.4.1
ESP8266WebServer server( serverPort );
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp;
bool bNeedUpdate;
uint8_t  dst;           // current dst

uint16_t roomTemp;
uint16_t rh;

int httpPort = 80; // may be modified by open AP scan

struct Sched
{
  uint16_t setTemp;
  uint16_t timeSch;
  uint8_t thresh;
  uint8_t wday;  // Todo: weekday 0=any, 1-7 = day of week
};

#define MAX_SCHED 8

struct eeSet // EEPROM backed data
{
  uint16_t size;          // if size changes, use defauls
  uint16_t sum;           // if sum is diiferent from memory struct, write
  uint16_t vacaTemp;       // vacation temp
  int8_t  tz;            // Timezone offset from your global server
  uint8_t schedCnt;    // number of active scedules
  bool    bVaca;         // vacation enabled
  bool    bAvg;         // average target between schedules
  bool    bEnableOLED;
  bool    bRes;
  char    schNames[MAX_SCHED][16]; // 128  names for small display
  Sched   schedule[MAX_SCHED];  // 48 bytes
};

eeSet ee = {
  sizeof(eeSet),
  0xAAAA,
  650, -5, // vacaTemp, TZ
  4,
  false,                   // active schedules, vacation mode
  false,                   // average
  true,                   // OLED
  false,                   // res
  {"Morning", "Noon", "Day", "Night", "Sch5", "Sch6", "Sch7", "Sch8"},
  {
    {830,  7*60, 5, 0},  // temp, time, thresh, wday
    {810, 11*60, 3, 0},
    {810, 17*60, 3, 0},
    {835, 20*60, 3, 0},
    {830,  0*60, 3, 0},
    {830,  0*60, 3, 0},
    {830,  0*60, 3, 0},
    {830,  0*60, 3, 0}
  },
};

int currentTemp = 850;
int hiTemp; // current target
int loTemp;
bool bHeater = false;
uint8_t schInd = 0;
String sMessage;

String dataJson()
{
    String s = "{\"waterTemp\": ";
    s += sDec(currentTemp);
    s += ",\"setTemp\": ";
    s += sDec(ee.schedule[schInd].setTemp);
    s += ", \"hiTemp\": ";
    s += sDec(hiTemp);
    s += ", \"loTemp\": ";
    s += sDec(loTemp);
    s += ", \"schInd\": ";
    s += schInd;
    s += ", \"on\": ";
    s += bHeater;
    s += ", \"temp\": ";
    s += sDec(roomTemp);
    s += ", \"rh\": ";
    s += sDec(rh);
    s += "}";
    return s;
}

eventHandler event(dataJson);

void parseParams()
{
  static char temp[256];
  String password;
  int val;
  int freq = 0;

  for ( uint8_t i = 0; i < server.args(); i++ ) // get password first
  {
    server.arg(i).toCharArray(temp, 256);
    String s = wifi.urldecode(temp);

    switch( server.argName(i).charAt(0)  )
    {
      case 'k': // key
          password = s;
          break;
    }
  }

  uint32_t ip = server.client().remoteIP();

  if(server.args() && (password != controlPassword) )
  {
    if(nWrongPass == 0)
      nWrongPass = 10;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every high speed wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 10;
    String data = "{\"ip\":\"";
    data += server.client().remoteIP().toString();
    data += "\",\"pass\":\"";
    data += password;
    data += "\"}";
    event.push("hack", data); // log attempts
    lastIP = ip;
    return;
  }

  lastIP = ip;

  for ( uint8_t i = 0; i < server.args(); i++ )
  {
    server.arg(i).toCharArray(temp, 256);
    String s = wifi.urldecode(temp);
    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    int which = server.argName(i).charAt(1) - '0'; // limitation = 9
    if(which >= MAX_SCHED) which = MAX_SCHED - 1; // safety
    val = s.toInt();
    bool b = (s == "true" || s == "ON") ? true:false;

    switch( server.argName(i).charAt(0)  )
    {
      case 'D': // D0
          ee.schedule[which].setTemp -= 1; // -0.1
          break;
      case 'U': // U0
          ee.schedule[which].setTemp += 1;
          break;
      case 'C': // C
          ee.schedCnt = val;
          if(ee.schedCnt > MAX_SCHED) ee.schedCnt = MAX_SCHED;
          break;
      case 'T': // T0 direct set (?TD=95.0&key=password)
          ee.schedule[which].setTemp = (uint16_t) (s.toFloat() * 10);
          break;
      case 'H': // H0 Threshold
          ee.schedule[which].thresh = (uint16_t) (s.toFloat() * 10);
          break;
      case 'Z': // TZ
          ee.tz = val;
          getUdpTime();
          break;
      case 'S': // S0-7
          if(s.indexOf(':') >= 0) // if :, otherwise raw minutes
            val = (val * 60) + s.substring(s.indexOf(':')+1).toInt();
          ee.schedule[which].timeSch = val;
          break;
      case 'O': // OLED
          ee.bEnableOLED = b;
          break;
      case 'V':     // vacation V0/V1
          if(which)
            ee.bVaca = b;
          else
            ee.vacaTemp = (uint16_t) (s.toFloat() * 10);
          break;
      case 'A': // Average
           ee.bAvg = b;
           break;
      case 'N': // name
          s.toCharArray(ee.schNames[ which ], 15);
          break;
      case 'm':  // message
          sMessage = server.arg(i);
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

  checkLimits();      // constrain and check new values
  checkSched(true);   // reconfigure to new schedule
}

void handleRoot() // Main webpage interface
{
//  digitalWrite(LED, HIGH);

  Serial.println("handleRoot");

  parseParams();

  String page;
  
  page = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
    "<title>WiFi Waterbed Heater</title>\n"
    "<style type=\"text/css\">\n"
    "table,input{\n"
    "border-radius: 5px;\n"
    "box-shadow: 2px 2px 12px #000000;\n"
    "background-image: -moz-linear-gradient(top, #dfffff, #5050ff);\n"
    "background-image: -ms-linear-gradient(top, #dfffff, #5050ff);\n"
    "background-image: -o-linear-gradient(top, #dfffff, #5050ff);\n"
    "background-image: -webkit-linear-gradient(top, #dfffff, #5050ff);\n"
    "background-image: linear-gradient(top, #dfffff, #5050ff);\n"
    "background-clip: padding-box;\n"
    "}\n"
    "body{width:320px;display:block;margin-left:auto;margin-right:auto;text-align:right;font-family: Arial, Helvetica, sans-serif;}}\n"
    "</style>\n"

    "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>"
    "<script type=\"text/javascript\">"
    "oledon=";
  page += ee.bEnableOLED;
  page += ";avg=";
  page += ee.bAvg;
  page += ";function startEvents()"
    "{"
      "eventSource = new EventSource(\"events?i=60&p=1\");"
      "eventSource.addEventListener('open', function(e){},false);"
      "eventSource.addEventListener('error', function(e){},false);"
      "eventSource.addEventListener('state',function(e){"
        "d = JSON.parse(e.data);"
        "document.all.temp.innerHTML=d.waterTemp+'&degF';"
        "document.all.on.innerHTML=d.on?\"<font color='red'><b>ON</b></font>\":\"OFF\";"
        "document.all.rt.innerHTML= 'Bedroom: '+d.temp+'&degF';"
        "document.all.rh.innerHTML=d.rh+'%';"
      "},false)"
    "}\n"
    "function oled(){"
      "oledon=!oledon;"
      "$.post(\"s\", { O: oledon, key: document.all.myKey.value });"
      "document.all.OLED.value=oledon?'OFF':'ON '"
    "}\n"
    "function setavg(){"
      "avg=!avg;"
      "$.post(\"s\", { A: avg, key: document.all.myKey.value });"
      "document.all.AVG.value=avg?'OFF':'ON '"
    "}\n"
    "setInterval(timer,1000);"
    "t=";
    page += now() - ((ee.tz + dst) * 3600); // set to GMT
    page +="000;function timer(){" // add 000 for ms
          "t+=1000;d=new Date(t);"
          "document.all.time.innerHTML=d.toLocaleTimeString()}"
    "</script>"

    "<body bgcolor=\"silver\" onload=\"{"
    "key = localStorage.getItem('key'); if(key!=null) document.getElementById('myKey').value = key;"
    "for(i=0;i<document.forms.length;i++) document.forms[i].elements['key'].value = key;}\">"
    "<h3>WiFi Waterbed Heater</h3>"
    "<table align=\"right\">";
    // Row 1
    page += "<tr>"
            "<td align=\"center\" colspan=2><div id=\"time\">";
    page += timeFmt(true, true);
    page += "</div></td><td><div id=\"temp\"> ";
    page += sDec(currentTemp) + "&degF</div> </td><td align=\"center\"><div id=\"on\">";
    page += bHeater ? "<font color=\"red\"><b>ON</b></font>" : "OFF";
    page += "</div></td></tr>"
    // Row 2
            "<tr>"
            "<td align=\"center\" colspan=2>Bedroom: </td><td><div id=\"rt\">";
    page += sDec(roomTemp);
    page += "&degF</div></td><td align=\"left\"><div id=\"rh\">";
    page += sDec(rh);
    page += "%</div> </td></tr>"
    // Row 3
            "<tr>"
            "<td colspan=2>";
    page += valButton("TZ ", "Z", String(ee.tz) );
    page += "</td><td colspan=2>Display:"
            "<input type=\"button\" value=\"";
    page += ee.bEnableOLED ? "OFF":"ON ";
    page += "\" id=\"OLED\" onClick=\"{oled()}\">"
          "</td></tr>"
    // Row 4
            "<tr><td colspan=2>";
    page += vacaForm();
    page += "</td><td colspan=2>Average:"
            "<input type=\"button\" value=\"";
    page += ee.bAvg ? "OFF":"ON ";
    page += "\" id=\"AVG\" onClick=\"{setavg()}\">"
            "</td></tr>"
    // Row 5
            "<tr>"
            "<td colspan=2>";
    page += button("Schedule ", "C", String((ee.schedCnt < MAX_SCHED) ? (ee.schedCnt+1):ee.schedCnt) );
    page += button("Count ", "C", String((ee.schedCnt > 0) ? (ee.schedCnt-1):ee.schedCnt) );
    page += "</td><td colspan=2>";
    page += button("Temp ", "U" + String( schInd ), "Up");
    page += button("Adjust ", "D" + String( schInd ), "Dn");
    page += "</td></tr>"
    // Row 5
            "<tr><td align=\"left\">Name</td><td align=\"left\">Time</td><td align=\"left\">Temp</td><td width=\"99px\" align=\"left\">Threshold</td></tr>";
    // Row 6-(7~13)
    for(int i = 0; i < ee.schedCnt; i++)
    {
      page += "<tr><td colspan=4";
      if(i == schInd && !ee.bVaca)
        page += " style=\"background-color:red\""; // Highlight active schedule
      page += ">";
      page += schedForm(i);
      page += "</td></tr>";
    }
    page += "<tr><td colspan=2><small>IP: ";
    page += server.client().remoteIP().toString();
    page += "</small></td><td colspan=2>"
            "<input id=\"myKey\" name=\"key\" type=text size=40 placeholder=\"password\" style=\"width: 90px\">"
            "<input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\">"
            "</td></tr>"
            "</table>"
            "</body></html>";
    server.send ( 200, "text/html", page );

//  digitalWrite(LED, LOW);
}

String button(String lbl, String id, String text) // Up/down buttons
{
  String s = "<form method='post'>";
  s += lbl;
  s += "<input name='";
  s += id;
  s += "' type='submit' value='";
  s += text;
  s += "'><input type=\"hidden\" name=\"key\" value=\"value\"></form>";
  return s;
}

String sDec(int t) // just 123 to 12.3 string
{
  String s = String( t / 10 ) + ".";
  s += t % 10;
  return s;
}

String valButton(String lbl, String id, String val)
{
  String s = "<form method='post'>";
  s += lbl;
  s += "<input name='";
  s += id;
  s += "' type=text size=1 value='";
  s += val;
  s += "'><input type=\"hidden\" name=\"key\"><input value=\"Set\" type=submit></form>";
  return s;
}

String schedForm(int sch)
{
  String s = "<form method='post'>";

  s += "<input name='N"; // name
  s += sch;
  s += "' type=text size=5 value='";
  s += ee.schNames[sch];
  s += "'> ";

  s += " <input name='S"; // time
  s += sch;
  s += "' type=text size=3 value='";
  int t = ee.schedule[sch].timeSch;
  s += ( t/60 );
  s += ":";
  if((t%60) < 10) s += "0";
  s += t%60;
  s += "'> ";

  s += "<input name='T"; // temp
  s += sch;
  s += "' type=text size=3 value='";
  s += sDec(ee.schedule[sch].setTemp);
  s += "F'> ";

  s += "<input name='H"; // thresh
  s += sch;
  s += "' type=text size=2 value='";
  s += sDec(ee.schedule[sch].thresh);
  s += "F'> ";
  
  s += "<input type=\"hidden\" name=\"key\"><input value=\"Set\" type=submit> </form>";
  return s;
}

String vacaForm(void)
{
  String s = "<form method='post'>";
  s += "Vaca <input name='V0' type=text size=3 value='";
  s += sDec(ee.vacaTemp);
  s += "F'>";
  s += "<input type=\"hidden\" name=\"key\"><input name='V1' value='";
  s += ee.bVaca ? "OFF":"ON";
  s += "' type=submit></form>";
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

void handleS() { // standard params, but no page
  Serial.println("handleS\n");
  parseParams();

  String page = "{\"ip\": \"";
  page += WiFi.localIP().toString();
  page += ":";
  page += serverPort;
  page += "\"}";
  server.send ( 200, "text/json", page );
}

// Return lots of vars as JSON
void handleJson()
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

  server.send ( 200, "text/json", page );
}

// event streamer (assume keep-alive)
void handleEvents()
{
  char temp[100];
//  Serial.println("handleEvents");
  uint16_t interval = 60; // default interval
  uint8_t nType = 0;

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
//    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    int val = s.toInt();
 
    switch( server.argName(i).charAt(0)  )
    {
      case 'i': // interval
        interval = val;
        break;
      case 'p': // push
        nType = 1;
        break;
      case 'c': // critical
        nType = 2;
        break;
    }
  }

  String content = "HTTP/1.1 200 OK\r\n"
      "Connection: keep-alive\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Content-Type: text/event-stream\r\n\r\n";
  server.sendContent(content);
  event.set(server.client(), interval, nType); // copying the client before the send makes it work with SDK 2.2.0
}

void handleNotFound() {
  Serial.println("handleNotFound\n");

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
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
  eeRead(); // don't access EE before WiFi init

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if ( MDNS.begin ( "waterbed", WiFi.localIP() ) ) {
    Serial.println ( "MDNS responder started" );
  }

  server.on ( "/", handleRoot );
  server.on ( "/s", handleS );
  server.on ( "/json", handleJson );
  server.on ( "/events", handleEvents );
//  server.on ( "/inline", []() {
//    server.send ( 200, "text/plain", "this works as well" );
//  } );
  server.onNotFound ( handleNotFound );
  server.begin();
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
  static uint8_t hour_save, min_save, sec_save;

  MDNS.update();
  server.handleClient();
  checkButtons();

  if(bNeedUpdate)
    if(checkUdpTime())
      checkSched(true);  // initialize

  if(sec_save != second()) // only do stuff once per second
  {
    sec_save = second();

    checkTemp();
    static uint8_t dht_cnt = 10;
    if(--dht_cnt == 0)
    {
      dht_cnt = 10;
      float r = dht.getHumidity();
      if(dht.getStatus() == DHT::ERROR_NONE)
      {
         rh = r * 10;
         roomTemp = dht.toFahrenheit(dht.getTemperature()) * 10;
      }
    }
    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();
      checkSched(false);        // check every minute for next schedule
      if (hour_save != hour()) // update our IP and time daily (at 2AM for DST)
      {
        eeWrite(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
        if( (hour_save = hour()) == 2)
        {
          getUdpTime();
        }
      }
    }
    if(displayOnTimer)
    {
      displayOnTimer --;
    }
    if(nWrongPass)
      nWrongPass--;
    event.heartbeat();
  }
  DrawScreen();
}

const char days[7][4] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
const char months[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

void DrawScreen()
{
  static int16_t ind;
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
      s = sMessage + " " + sMessage; // just double it
      if(msgCnt == 0) // starting
      {
        msgCnt = 3; // times to repeat message
        ind = 0;
      }
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

      s = s + s;
    }
    int w = display.drawPropString(ind, 0, s ); // this returns the proportional text width
    if( --ind < -(w))
    {
      if(msgCnt) // end of custom message display
      {
        if(--msgCnt == 0) // decrement times to repeat it
        {
            sMessage = "";
        }
      }
      ind = 0;
    }

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

// Check temp to turn heater on and off
void checkTemp()
{
#define TEMPS 16
  static uint16_t array[TEMPS];
  static bool bInit = false;
  static uint8_t idx = 0;
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
    event.print("Invalid CRC");
    return;
  }

  uint16_t raw = (data[1] << 8) | data[0];

  if(raw > 630 || raw < 200){ // first reading is always 1360 (0x550)
    Serial.print("DS err ");
    Serial.println(raw);
    return;
  }

//array[idx] = (( raw * 625) / 1000;  // to 10x celcius
  array[idx] = (raw * 1125) / 1000 + 320; // 10x fahrenheit

  if(!bInit) // fill it the first time
  {
    for(int i = 0; i < TEMPS; i++)
      array[i] = array[idx];
    bInit = true;
  }

  if(++idx >= TEMPS) idx = 0;

  uint16_t newTemp = 0;

  for(int i = 0; i < TEMPS; i++)
    newTemp += array[i];
  newTemp /= TEMPS;

  if(newTemp != currentTemp)
  {
    currentTemp = newTemp;
    event.pushInstant();
  }
  Serial.print("Temp: ");
  Serial.println(currentTemp);

  if(currentTemp <= loTemp && bHeater == false)
  {
    Serial.println("Heat on");
    bHeater = true;
    digitalWrite(HEAT, !bHeater);
    event.push(); // give a more precise account of changes
  }
  else if(currentTemp >= hiTemp && bHeater == true)
  {
    Serial.println("Heat off");
    bHeater = false;
    digitalWrite(HEAT, !bHeater);
    event.push();
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

void eeWrite() // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t *)&ee, sizeof(eeSet));

  if(old_sum == ee.sum)
    return; // Nothing has changed?

  wifi.eeWriteData(64, (uint8_t*)&ee, sizeof(ee)); // WiFiManager already has an instance open, so use that at offset 64+
}

void eeRead()
{
  eeSet eeTest;

  wifi.eeReadData(64, (uint8_t*)&eeTest, sizeof(eeSet));
  if(eeTest.size != sizeof(eeSet)) return; // revert to defaults if struct size changes
  uint16_t sum = eeTest.sum;
  eeTest.sum = 0;
  eeTest.sum = Fletcher16((uint8_t *)&eeTest, sizeof(eeSet));
  if(eeTest.sum != sum) return; // revert to defaults if sum fails
  memcpy(&ee, &eeTest, sizeof(eeSet));
}

uint16_t Fletcher16( uint8_t* data, int count)
{
   uint16_t sum1 = 0;
   uint16_t sum2 = 0;

   for( int index = 0; index < count; ++index )
   {
      sum1 = (sum1 + data[index]) % 255;
      sum2 = (sum2 + sum1) % 255;
   }

   return (sum2 << 8) | sum1;
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
