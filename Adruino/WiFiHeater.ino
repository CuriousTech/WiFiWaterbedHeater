/**The MIT License (MIT)

Copyright (c) 2015 by Greg Cunningham, CuriousTech

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
#include <OneWire.h>
#include "ssd1306_i2c.h"
#include "icons.h"
#include <Time.h>

#include <WiFiClient.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>
#include "WiFiManager.h"
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>

const char *controlPassword = "password"; // device password for modifying any settings
const char *serverFile = "Waterbed";    // Creates /iot/Waterbed.php
int serverPort = 82;                    // port fwd for fwdip.php
const char *myHost = "www.yourdomain.com"; // php forwarding/time server (fwdip.php)

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
WiFiUDP Udp;

extern "C" {
  #include "user_interface.h" // Needed for deepSleep which isn't used
}

#define LED       0  // back LED for debug
#define ESP_LED   2  //Blue LED on ESP07 (on low)
#define HEARTBEAT 2
#define HEAT     12  // Also blue "HEAT" LED
#define BTN_UP   13
#define DS18B20  14
#define BEEP     15
#define BTN_DN   16

OneWire ds(DS18B20);
byte ds_addr[8];
long read_temp;
long read_scratch;
bool bScratch;
uint32_t lastIP;
int nWrongPass;

SSD1306 display(0x3c, 4, 5); // Initialize the oled display for address 0x3c, sda=4, sdc=5
bool bDisplay_on = true;

WiFiManager wifi(0);  // AP page:  192.168.4.1
extern MDNSResponder mdns;
ESP8266WebServer server( serverPort );

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
  int8_t   tz;            // Timezone offset from your global server
  uint8_t  schedCnt;    // number of active scedules
  uint8_t  bVaca;         // vacation enabled
  uint8_t  bAvg;         // average target between schedules
  char     schNames[MAX_SCHED][16]; // 128  names for small display
  Sched   schedule[MAX_SCHED];  // 48 bytes
};

eeSet ee = {
  sizeof(eeSet),
  0xAAAA,
  650, -5, // vacaTemp, TZ
  4, 0,                   // active schedules, vacation mode
  0,                   // average
  {"Morning", "Noon", "Day", "Night", "Sch5", "Sch6", "Sch7", "Sch8"},
  {
    {820,  7*60, 5, 0},  // temp, time, thresh, wday
    {810, 11*60, 3, 0},
    {810, 17*60, 3, 0},
    {820, 20*60, 3, 0},
    {830,  0*60, 3, 0},
    {830,  0*60, 3, 0},
    {830,  0*60, 3, 0},
    {830,  0*60, 3, 0}
  },
};

uint8_t hour_save, min_save, sec_save;

bool bState[2];
bool lbState[2];
long debounce[2];
long lRepeatMillis;
int currentTemp = 999;
int hiTemp; // current target
int loTemp;
bool bHeater = false;
bool bNeedUpdate = true;
uint8_t schInd = 0;

#define CLIENTS 4
class eventClient
{
public:
  eventClient()
  {
  }

  void set(WiFiClient cl, int t)
  {
    m_client = cl;
    m_interval = t;
    m_timer = 0;
    m_keepAlive = 10;
    m_client.print(":ok\n");
    push();
  }

  bool inUse()
  {
    return m_client.connected();
  }

  void push()
  {
    if(m_client.connected() == 0)
      return;
    String s = dataJson(0);
    m_client.print("event: state\n");
    m_client.println("data: " + s + "\n");
    m_keepAlive = 11;
    m_timer = 0;
  }

  void beat()
  {
    if(m_client.connected() == 0)
      return;

    if(++m_timer >= m_interval)
      push();
 
    if(--m_keepAlive <= 0)
    {
      m_client.print("\n");
      m_keepAlive = 10;
    }
  }

private:
  String dataJson(long ip)
  {
    String s = "{\"temp\": ";
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
    s += ",\"ip\": \"";
    s += ipString(ip);
    s += "\"}";
    return s;
  }

  WiFiClient m_client;
  int8_t m_keepAlive;
  uint16_t m_interval;
  uint16_t m_timer;
};
eventClient ec[CLIENTS];

String ipString(IPAddress ip) // Convert IP to string
{
  String sip = String(ip[0]);
  sip += ".";
  sip += ip[1];
  sip += ".";
  sip += ip[2];
  sip += ".";
  sip += ip[3];
  return sip;
}

void parseParams()
{
  char temp[100];
  String password;
  int val;
  eeSet save;
  memcpy(&save, &ee, sizeof(ee));

  for ( uint8_t i = 0; i < server.args(); i++ )
  {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    int which = server.argName(i).charAt(1) - '0'; // limitation = 9
    if(which >= MAX_SCHED) which = MAX_SCHED - 1; // safety
    val = s.toInt();
    bool b = (s == "ON") ? true:false;

    switch( server.argName(i).charAt(0)  )
    {
      case 'k': // key
          password = s;
          break;
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
          bNeedUpdate = true;
          break;
      case 'S': // S0-7
          if(s.indexOf(':') >= 0) // if :, otherwise raw minutes
            val = (val * 60) + s.substring(s.indexOf(':')+1).toInt();
          ee.schedule[which].timeSch = val;
          break;
      case 'O': // OLED
          bDisplay_on = b;
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
      case 'W':   // Test the watchdog
          digitalWrite(BEEP, b);
          break;
      case 'B':   // for PIC programming
          pinMode(BEEP, b ? OUTPUT:INPUT);
          pinMode(HEARTBEAT, b ? OUTPUT:INPUT);
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
    eventPush(ip); // log attempts
  }

  if(nWrongPass) memcpy(&ee, &save, sizeof(ee)); // undo any changes

  lastIP = ip;

  checkLimits();      // constrain and check new values
  checkSched(true);   // reconfigure to new schedule
}

void handleRoot() // Main webpage interface
{
  digitalWrite(LED, HIGH);

  Serial.println("handleRoot");

  parseParams();

  String page;
  
  page = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
    "<title>WiFi Waterbed Heater</title>"
    "<style>div,input {margin-bottom: 5px;}body{width:320px;display:block;margin-left:auto;margin-right:auto;text-align:right;font-family: Arial, Helvetica, sans-serif;}}</style>"

    "<script src=\"http://ajax.googleapis.com/ajax/libs/jquery/1.3.2/jquery.min.js\" type=\"text/javascript\" charset=\"utf-8\"></script>"
    "<script type=\"text/javascript\">"
    "function startEvents()"
    "{"
      "eventSource = new EventSource(\"events?i=60\");"
      "eventSource.addEventListener('open', function(e){},false);"
      "eventSource.addEventListener('error', function(e){},false);"
      "eventSource.addEventListener('state',function(e){"
        "d = JSON.parse(e.data);"
        "document.all.temp.innerHTML=d.temp+\"&degF\";"
        "document.all.on.innerHTML=d.on?\"<font color='red'><b>ON</b></font>\":\"OFF\";"
      "},false)"
    "}"
    "setInterval(timer,1000);"
    "t=";
    page += now() - (ee.tz * 60 * 60); // set to GMT
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
    page += "</div></td><td><div id=\"temp\">";
    page += sDec(currentTemp) + "&degF </div></td><td><div id=\"on\">";
    page += bHeater ? "<font color=\"red\"><b>ON</b></font>" : "OFF";
    page += "</div></td></tr>"
    // Row 2
            "<tr>"
            "<td colspan=2>";
    page += valButton("TZ ", "Z", String(ee.tz) );
    page += "</td><td colspan=2>";
    page += button("OLED ", "OLED", bDisplay_on ? "OFF":"ON" );
    page += "</td></tr>"
    // Row 3
            "<tr><td colspan=2>";
    page += vacaForm();
    page += "</td><td colspan=2>";
    page += button("Avg ", "A", ee.bAvg ? "OFF":"ON" );
    page += "</td></tr>"
    // Row 4
            "<tr>"
            "<td colspan=2>";
    page += button("Schedule ", "C", String((ee.schedCnt < MAX_SCHED) ? (ee.schedCnt+1):ee.schedCnt) );
    page += button("Count ", "C", String((ee.schedCnt > 0) ? (ee.schedCnt-1):ee.schedCnt) );
    page += "</td><td colspan=2>";
    page += button("Temp ", "U" + String( schInd ), "Up");
    page += button("Adjust ", "D" + String( schInd ), "Dn");
    page += "</td></tr>"
    // Row 5
            "<tr><td align=\"left\">Name</td><td align=\"left\">Time</td><td align=\"left\">Temp</td><td width=\"100px\" align=\"left\">Threshold</td></tr>";
    // Row 6-(7~13)
    for(int i = 0; i < ee.schedCnt; i++)
    {
      page += "<tr><td colspan=4";
      if(i == schInd && !ee.bVaca)
        page += " style=\"background-color:teal\""; // Highlight active schedule
      page += ">";
      page += schedForm(i);
      page += "</td></tr>";
    }
    page += "<tr><td colspan=2><small>IP: ";
    page += ipString(server.client().remoteIP());
    page += "</small></td><td colspan=2>"
            "<input id=\"myKey\" name=\"key\" type=text size=40 placeholder=\"password\" style=\"width: 90px\">"
            "<input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\">"
            "</td></tr>"
            "</table>"
            "</body></html>";
    server.send ( 200, "text/html", page );
  
  digitalWrite(LED, LOW);
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

void handleS() { // /s?x=y can be redirected to index
  Serial.println("handleS\n");
  parseParams();

  String page = "{\"ip\": \"";
  page += ipString(WiFi.localIP());
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
  page += "\"temp\": ";
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
  page += "}";

  server.send ( 200, "text/json", page );
}

// event streamer (assume keep-alive) (esp8266 2.1.0 can't handle this)
void handleEvents()
{
  char temp[100];
  Serial.println("handleEvents");
  uint16_t interval = 60; // default interval
 
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    int val = s.toInt();
 
    switch( server.argName(i).charAt(0)  )
    {
      case 'i': // interval
          interval = val;
          break;
    }
  }

  server.send( 200, "text/event-stream", "" );
  bool good = false;
  for(int i = 0; i < CLIENTS; i++) // find an unused client
    if(!ec[i].inUse())
    {
      ec[i].set(server.client(), interval);
      break;
    }
}

void eventHeartbeat()
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].beat();
}

void eventPush(long ip) // push to all
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].push();
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
  pinMode(LED, OUTPUT);
  pinMode(BEEP, OUTPUT);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DN, INPUT); // IO16 has external pullup
  pinMode(HEAT, OUTPUT);
  pinMode(HEARTBEAT, OUTPUT);

  digitalWrite(HEAT, HIGH); // high is off
  digitalWrite(BEEP, HIGH); // disable watchdog

  // initialize dispaly
  display.init();
  digitalWrite(HEARTBEAT, LOW);

  Serial.begin(115200);
//  delay(3000);
  Serial.println();
  Serial.println();

  wifi.autoConnect("Waterbed"); // Tries all open APs, then starts softAP mode for config
  eeRead(); // don't access EE before WiFi init

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  if ( mdns.begin ( "esp8266", WiFi.localIP() ) ) {
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
  digitalWrite(HEARTBEAT, HIGH);
  server.begin();

  digitalWrite(BEEP, LOW); // beep
  digitalWrite(BEEP, HIGH);  // this gets past the initial beep wait
  digitalWrite(BEEP, LOW);
  digitalWrite(BEEP, HIGH);
  digitalWrite(BEEP, LOW); // enable watchdog

  if( ds.search(ds_addr) )
  {
    Serial.print("OneWire device: ");
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

  digitalWrite(HEARTBEAT, LOW);
  ctSetIp();
  getUdpTime();
  bNeedUpdate = true;
}

void loop()
{
  mdns.update();
  server.handleClient();
  checkButtons();
  if(bNeedUpdate)
  {
    if( checkUdpTime() )           // update with NTP
    {
      checkSched(true); // sync schedule to updated time
      bNeedUpdate = false;
    }
  }

  if(sec_save != second()) // only do stuff once per second
  {
    sec_save = second();

    checkTemp();

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
          bNeedUpdate = true;
        }
      }
    }
   
    if(nWrongPass)
      nWrongPass--;

    eventHeartbeat();
  
    digitalWrite(HEARTBEAT, !digitalRead(HEARTBEAT));
  }
  DrawScreen();
}

int16_t ind;
bool blnk = false;
long last;

void DrawScreen()
{
  // draw the screen here
  display.clear();

  if( (millis() - last) > 400) // 400ms togle for blinker
  {
    last = millis();
    blnk = !blnk;
  }

  if(bDisplay_on) // draw only ON indicator if screen off
  {
    display.setFontScale2x2(false); // the small text
    display.drawString( 8, 22, "Temp");
    display.drawString(80, 22, "Set");
    const char *ps = ee.bVaca ? "Vacation" : ee.schNames[schInd];
    display.drawString(90-(strlen(ps) << 2), 55, ps);

    String s = timeFmt(true, true); // the scroller
    s += "  ";
    s += dayShortStr(weekday());
    s += " ";
    s += String(day());
    s += " ";
    s += monthShortStr(month());
    s += "  ";
    int len = s.length();
    s = s + s;

    int w = display.drawPropString(ind, 0, s ); // this returns the proportional text width
    if( --ind < -(w)) ind = 0;

    String temp = sDec(currentTemp) + "]"; // <- that's a degree symbol
    display.drawPropString(2, 33, temp );
    temp = sDec(ee.schedule[schInd].setTemp) + "]";
    display.drawPropString(70, 33, temp );
    if(bHeater && blnk)
      display.drawString(1, 55, "Heat");
  }
  else if(bHeater)  // small blinky dot when display is off
  {
//    const char *xbm = blnk ? active_bits : inactive_bits;
//    display.drawXbm(2, 56, 8, 8, xbm);  // heater on indicator

    if(blnk) display.drawString( 2, 56, ".");
  }
  display.display();
}

// Check temp to turn heater on and off
void checkTemp()
{
  if ((millis() - read_temp) > 5000) // start a conversion every 5 seconds
  {
    read_temp = millis();
    ds.reset();
    ds.select(ds_addr);
    ds.write(0x44,0);   // start conversion, no parasite power on at the end
    read_scratch = millis();
    bScratch = true;
    return;
  }
  if(!bScratch || (millis() - read_scratch) < 1000) // conversion takes 750ms
    return;
  bScratch = false;

  uint8_t data[10];
  uint8_t present = ds.reset();
  ds.select(ds_addr);
  ds.write(0xBE);         // Read Scratchpad

  if(!present)      // safety
  {
    bHeater = false;
    digitalWrite(HEAT, LOW);
    return;
  }

  for ( int i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
//    Serial.print(data[i], HEX);
//    Serial.print(" ");
  }
//  Serial.println();

  if(OneWire::crc8( data, 8) != data[8])  // bad CRC
  {
    bHeater = false;
    digitalWrite(HEAT, LOW);
    Serial.println("Invalid CRC");
    return;
  }

  uint16_t raw = (data[1] << 8) | data[0];

//  int newTemp = (( raw * 625) / 1000;  // to 10x celcius
  int newTemp = ( (raw * 1125) + 320000) / 1000; // 10x fahrenheit
  if(newTemp > currentTemp + 100)
    Serial.println("Skipping strange reading");
  else currentTemp = newTemp;
  Serial.println( currentTemp );
//  Serial.print("Temp: ");
//  Serial.println(currentTemp);

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

    int m = ((hour() * 60) + minute()); // current TOD in minutes

    if(m < start) // offset by start of current schedule
      m -= start - (24*60); // rollover
    else
      m -= start;

    hiTemp = tween(ee.schedule[schInd].setTemp, ee.schedule[s2].setTemp, m, range);
    thresh = tween(ee.schedule[schInd].thresh, ee.schedule[s2].thresh, m, range);
    loTemp = hiTemp - thresh;
  }

  if(currentTemp <= loTemp && bHeater == false)
  {
    Serial.println("Heat on");
    bHeater = true;
    digitalWrite(HEAT, !bHeater);
    eventPush(0); // give a more precise account of changes
  }
  else if(currentTemp >= hiTemp && bHeater == true)
  {
    Serial.println("Heat off");
    bHeater = false;
    digitalWrite(HEAT, !bHeater);
    eventPush(0);
  }
}

// avarge value at current minute between times
int tween(int t1, int t2, int m, int range)
{
  if(range == 0) range = 1; // div by zero check
  int t = (t2 - t1) * (m * 100 / range) / 100;
  return t + t1;
}

// todo: B-spline the tween

// Check the buttons
void checkButtons()
{
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
        bDisplay_on = true;
      }
    }
    else if(bState[0] == LOW) // holding down
    {
      if( (millis() - lRepeatMillis) > REPEAT_DELAY) // increase for slower repeat
      {
        changeTemp(1);
        lRepeatMillis = millis();
        bDisplay_on = true;
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
}

// Setup a php script on my server to send traffic here from /iot/waterbed.php, plus sync time
// PHP script: https://github.com/CuriousTech/ESP07_Multi/blob/master/fwdip.php
bool ctSetIp()
{
  WiFiClient client;
  if (!client.connect(myHost, httpPort))
  {
    Serial.println("Host ip connection failed");
    delay(100);
    return false;
  }

  String url = "/fwdip.php?name=";
  url += serverFile;
  url += "&port=";
  url += serverPort;

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + myHost + "\r\n" + 
               "Connection: close\r\n\r\n");

  delay(10);
  String line = client.readStringUntil('\n');
  // Read all the lines of the reply from server
  while(client.available())
  {
    line = client.readStringUntil('\n');
    line.trim();
    if(line.startsWith("IP:")) // I don't need my global IP
    {
    }
    else if(line.startsWith("Time:")) // Time from the server in a simple format
    {
    }
  }
 
  client.stop();
  return true;
}

uint8_t DST() // 2016 starts 2AM Mar 13, ends Nov 6
{
  uint8_t m = month();
  int8_t d = day();
  int8_t dow = weekday();
  if ((m  >  3 && m < 11 ) || 
      (m ==  3 && d >= 8 && dow == 0 && hour() >= 2) ||  // DST starts 2nd Sunday of March;  2am
      (m == 11 && d <  8 && dow >  0) ||
      (m == 11 && d <  8 && dow == 0 && hour() < 2))   // DST ends 1st Sunday of November; 2am
    return 1;
 return 0;
}

void getUdpTime()
{
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
  long timeZoneOffset = 3600 * (ee.tz + DST());
  unsigned long epoch = secsSince1900 - seventyYears + timeZoneOffset + 1; // bump 1 second

  // Grab the fraction
  highWord = word(packetBuffer[44], packetBuffer[45]);
  lowWord = word(packetBuffer[46], packetBuffer[47]);
  unsigned long d = (highWord << 16 | lowWord) / 4295000; // convert to ms

  delay(1000-d); // sync to fraction in ms
  setTime(epoch);
//  Serial.print("Time ");
//  Serial.println(timeFmt(true, true));
  return true;
}
