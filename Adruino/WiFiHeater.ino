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

const char *controlPassword = "yourpass"; // device password for modifying any settings
const char *serverFile = "Waterbed";    // Creates /iot/Waterbed.php
int serverPort = 80;                    // port fwd for fwdip.php
const char *myHost = "www.yourdomain.com"; // server with fwdip.php on it

union ip4
{
  struct
  {
    uint8_t b[4];
  } b;
  uint32_t l;
};

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

struct eeSet // EEPROM backed data
{
  char     dataServer[64];
  uint8_t  dataServerPort; // may be modified by listener (dataHost)
  int8_t   tz;            // Timezone offset from your server
  uint16_t setTemp[2];
  uint16_t timeSch[2];
  uint16_t thresh;
  uint16_t interval;
  uint16_t check; // or CRC
};
        // dsrv, prt, tz, day, nt    9AM,   8PM, th 0.5, 10mins
eeSet ee = { "", 80,  1, {950,980}, {9*60, 20*60}, 5,  10*60, 0xAAAA};

uint8_t hour_save, sec_save;
int ee_sum; // sum for checking if any setting has changed

bool bState[2];
bool lbState[2];
long debounce[2];
long lRepeatMillis;
int currentTemp = 999;
bool bHeater = false;
bool bNeedUpdate = true;
int logCounter;

String ipString(long l)
{
  ip4 ip;
  ip.l = l;
  String sip = String(ip.b.b[0]);
  sip += ".";
  sip += ip.b.b[1];
  sip += ".";
  sip += ip.b.b[2];
  sip += ".";
  sip += ip.b.b[3];
  return sip;
}

void handleRoot() // Main webpage interface
{
  digitalWrite(LED, HIGH);
  char temp[100];
  String password;
  int val;
  bool bUpdateTime = false;
  bool ipSet = false;
  eeSet save;
  memcpy(&save, &ee, sizeof(ee));

  Serial.println("handleRoot");

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    server.arg(i).toCharArray(temp, 100);
    String s = wifi.urldecode(temp);
    Serial.println( i + " " + server.argName ( i ) + ": " + s);
    bool which = (tolower(server.argName(i).charAt(1) ) == 'n') ? 1:0;

    switch( server.argName(i).charAt(0)  )
    {
      case 'k': // key
          password = s;
          break;
      case 'D': // DN or DD
          ee.setTemp[ which ] -= 10;
          break;
      case 'U': // UN or UD
          ee.setTemp[ which ] += 10;
          break;
      case 'T': // TN or TD direct set (?TD=95.0&key=password)
          ee.setTemp[ which ] = (uint16_t) (s.toFloat() * 10);
          break;
      case 'H': // Threshold
          ee.thresh = (uint16_t) (s.toFloat() * 10);
          break;
      case 'Z': // TZ
          ee.tz = s.toInt();
          bUpdateTime = true; // refresh current time
          break;
      case 'i': // ?ip=server&port=80&int=60&key=password (htTp://192.168.0.197:82/s?ip=192.168.0.189&port=81&int=600&key=password)
          if(which) // interval
          {
            ee.interval = s.toInt();
          }
          else // ip
          {
            s.toCharArray(ee.dataServer, 64); // todo: parse into domain/URI
            Serial.print("Server ");
            Serial.println(ee.dataServer);
            ipSet = true;
           }
           break;
      case 'p': // port
          ee.dataServerPort = s.toInt();
          break;
      case 'S': // SN or SD
          val = (s.toInt() * 60) + s.substring(s.indexOf(':')+1).toInt();
          ee.timeSch[ which ] = val;
          break;
      case 'O': // OLED
          bDisplay_on = s.toInt() ? true:false;
          break;
      case 'A':   // Test the watchdog
          digitalWrite(BEEP, s.toInt() ? true:false);
          break;
      case 'B':   // for PIC programming
          if(s.toInt())
          {
              pinMode(BEEP, OUTPUT);
              pinMode(HEARTBEAT, OUTPUT);
          }
          else
          {
              pinMode(BEEP, INPUT);
              pinMode(HEARTBEAT, INPUT);
          }
          break;
    }
  }

  uint32_t ip = server.client().remoteIP();

  if(server.args() && (password != controlPassword) )
  {
    memcpy(&ee, &save, sizeof(ee)); // undo any changes
    if(nWrongPass < 4)
      nWrongPass = 4;
    else if((nWrongPass & 0xFFFFF000) == 0 ) // time doubles for every wrong password attempt.  Max 1 hour
      nWrongPass <<= 1;
    if(ip != lastIP)  // if different IP drop it down
       nWrongPass = 4;
    ctSendIP(false, ip); // log attempts
  }
  lastIP = ip;

  checkLimits();

  String page;
  
  if(ipSet) // if data IP being set, return local IP
  {
    page = "{\"ip\": \"";
    page += ipString(WiFi.localIP());
    page += ":";
    page += serverPort;
    page += "\"}";
    server.send ( 200, "text/json", page );
  }
  else
  {
    page = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>"
          "<title>WiFi Waterbed Heater</title>";
    page += "<style>div,input {margin-bottom: 5px;}body{width:260px;display:block;margin-left:auto;margin-right:auto;text-align:right;font-family: Arial, Helvetica, sans-serif;}}</style>";
    page += "<body onload=\"{"
      "key = localStorage.getItem('key'); if(key!=null) document.getElementById('myKey').value = key;"
      "for(i=0;i<document.forms.length;i++) document.forms[i].elements['key'].value = key;"
      "}\">";

    page += "<h3>WiFi Waterbed Heater </h3>";
    page += "<p>" + timeFmt(true, true);
    page += " &nbsp&nbsp&nbsp&nbsp ";
    page += sDec(currentTemp) + "&degF ";
    page += bHeater ? "<font color=\"red\"><b>ON</b></font>" : "OFF";
    page += "</p>";
  
    page += "<table align=\"right\"><tr><td colspan=2 align=\"center\">";
    page += (activeTemp()) ? "Day" : "<font color=\"red\"><b>Day</b></font>";
    page += "</td>";
    page += "<td colspan=2 align=\"center\">";
    page += (activeTemp()) ? "<font color=\"red\"><b>Night</b></font>" : "Night";
    page += "</td></tr><tr><td>";
    page += button("DD", "Dn"); page += "</td><td>"; page += button("UD", "Up");
    page += "</td><td>";
    page += button("DN", "Dn");  page += "</td><td>"; page += button("UN", "Up");
    page += "</td></tr><tr><td colspan=2>";
    page += tempButton("TD", ee.setTemp[0] );  page += "</td><td colspan=2>"; page += tempButton("TN", ee.setTemp[1] );
    page += "</td></tr><tr><td colspan=2>";
    page += timeButton("SD", ee.timeSch[0]);  page += "</td><td colspan=2>";  page += timeButton("SN", ee.timeSch[1]);
    page += "</td></tr><tr><td colspan=2 align=\"center\">Threshold</td><td colspan=2 align=\"center\">Timezone</td></tr>";
    page += "<tr><td colspan=2>";
    page += tempButton("H", ee.thresh);  page += "</td><td colspan=2>";  page += valButton("Z", String(ee.tz) );
    page += "</td></tr></table>";

    page += "<input id=\"myKey\" name=\"key\" type=text size=50 placeholder=\"password\" style=\"width: 150px\">";
    page += "<input type=\"button\" value=\"Save\" onClick=\"{localStorage.setItem('key', key = document.all.myKey.value)}\">";
    page += "<br>Logged IP: ";
    page += ipString(ip);
    page += "<br></body></html>";
    server.send ( 200, "text/html", page );
  }

  if(bUpdateTime)
    ctSetIp();

  digitalWrite(LED, LOW);
}

String button(String id, String text) // Up/down buttons
{
  String s = "<form method='post' action='s'><input name='";
  s += id;
  s += "' type='submit' value='";
  s += text;
  s += "'><input type=\"hidden\" name=\"key\" value=\"value\"></form>";
  return s;
}

String timeButton(String id, int t) // time and set buttons
{
  String s = String( t/60 ) + ":";
  if((t%60) < 10) s += "0";
  s += t%60;
  return valButton(id, s);
}

String tempButton(String id, int t) // temp and set buttons
{
  String s = sDec(t) + "F";
  return valButton(id, s);
}

String sDec(int t)
{
  String s = String( t / 10 ) + ".";
  s += t % 10;
  return s;
}

String valButton(String id, String val)
{
  String s = "<form method='post' action='s'><input name='";
  s += id;
  s += "' type=text size=4 value='";
  s += val;
  s += "'><input type=\"hidden\" name=\"key\"><input value=\"Set\" type=submit></form>";
  return s;
}

// Set sec to 60 to remove seconds
String timeFmt(bool do_sec, bool do_12)
{
  String r = "";
  if(hourFormat12() <10) r = " ";
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
  if(do_12)
  {
      r += isPM() ? "PM":"AM";
  }
  return r;
}

void handleS() { // /s?x=y can be redirected to index
  Serial.println("handleS\n");
  handleRoot();
}

// Todo: JSON I/O
void handleJson()
{
  Serial.println("handleJson\n");
  String page = "{\"setTemp0\": ";
  page += ee.setTemp[0];
  page += ", \"setTemp1\": ";
  page += ee.setTemp[1];
  page += ", \"timeSch0\": ";
  page += ee.timeSch[0];
  page += ", \"timeSch1\": ";
  page += ee.timeSch[1];
  page += ", \"Threshold\": ";
  page += ee.thresh;
  page += ", \"temp\": ";
  page += currentTemp;
  page += ", \"on\": ";
  page += bHeater;
  page += "}";
  
  server.send ( 200, "text/json", page );
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

  display.clear();
  display.display();

  Serial.begin(115200);
//  delay(3000);
  Serial.println();
  Serial.println();

  wifi.findOpenAP(myHost); // Tries all open APs, then starts softAP mode for config
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
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
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

  logCounter = 60;
  ctSendIP(true, WiFi.localIP());
  digitalWrite(HEARTBEAT, LOW);
}

void loop()
{
  mdns.update();
  server.handleClient();
  checkButtons();

  if(sec_save != second()) // only do stuff once per second
  {
    sec_save = second();

    checkTemp();

    if (hour_save != hour()) // update our IP and time daily (at 2AM for DST)
    {
      display.init();
      if( (hour_save = hour()) == 2)
        bNeedUpdate = true;
    }

    if(bNeedUpdate)
      if( ctSetIp() )
        bNeedUpdate = false;

    if(nWrongPass)
      nWrongPass--;

    if(logCounter)
    {
      if(--logCounter == 0)
      {
        logCounter = ee.interval;
        ctSendLog();
        eeWrite(); // update EEPROM if needed while we're at it (give user time to make many adjustments)
      }
    }
    digitalWrite(HEARTBEAT, !digitalRead(HEARTBEAT));
  }
  DrawScreen();
}

int16_t ind;
bool b = false;
int o2 = 0;
long last;

void DrawScreen()
{
  // draw the screen here
  display.clear();

  if(bDisplay_on) // draw only ON indicator if screen off
  {
    display.setFontScale2x2(false);
    display.drawString( 8, 22, "Temp");
    display.drawString(80, 22, "Set");
    display.drawString(76, 55, activeTemp() ? "Night":" Day");

    String s = timeFmt(true, true);
    s += "  ";
    s += dayShortStr(weekday());
    s += " ";
    s += String(day());
    s += " ";
    s += monthShortStr(month());
    s += "  ";
    int len = s.length();
    s = s + s;

    int w = display.drawPropString(ind, 0, s );
    if( --ind < -(w)) ind = 0;

    String temp = sDec(currentTemp) + "]"; // <- that's a degree
    display.drawPropString(2, 33, temp );
    temp = sDec(ee.setTemp[activeTemp()]) + "]";
    display.drawPropString(70, 33, temp );
  }
/*
  if( (millis() - last) > 400) // blinky on indicator
  {
    last = millis();
    b = !b;
  }
  const char *xbm = (bHeater && b) ? active_bits : inactive_bits;
  display.drawXbm(2, 56, 8, 8, xbm);  // heater on indicator
*/

 if(bHeater) // wierd animated on indicator
 {
    for(int y = 54;y < 64; y++)
    {
      for(int x = o2; x < 28; x += 8)
        display.setPixel(x, y);
      if((o2++) > 6) o2 = 0;
    }
    display.drawString(1, 55, "Heat");
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

  if(!present) return;

  for ( int i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
//    Serial.print(data[i], HEX);
//    Serial.print(" ");
  }
//  Serial.println();

  if(OneWire::crc8( data, 8) != data[8])  // bad CRC
  {
    Serial.println("Invalid CRC");
    return;
  }
  Serial.print("Temp: ");
  uint16_t raw = (data[1] << 8) | data[0];
  Serial.print( raw );
  Serial.print( " " );

//  int newTemp = (( raw * 625) / 1000;  // to 10x celcius
  int newTemp = ( (raw * 1125) + 320000) / 1000; // 10x fahrenheit
  if(newTemp > currentTemp + 100)
    Serial.println("Skipping strange reading");
  else currentTemp = newTemp;
  Serial.println( currentTemp );
//  Serial.print("Temp: ");
//  Serial.println(currentTemp);

  if(currentTemp <= ee.setTemp[activeTemp()] - ee.thresh && bHeater == false)
  {
    Serial.println("Heat on");
    bHeater = true;
    digitalWrite(HEAT, !bHeater);
    ctSendLog(); // give a more precise account of changes
  }
  else if(currentTemp >= ee.setTemp[activeTemp()] && bHeater == true)
  {
    Serial.println("Heat off");
    bHeater = false;
    digitalWrite(HEAT, !bHeater);
    ctSendLog();
  }
}

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
  ee.setTemp[activeTemp()] += delta;
  checkLimits();
}

void checkLimits()
{
  ee.setTemp[0] = constrain(ee.setTemp[0], 600, 990); // sanity check
  ee.setTemp[1] = constrain(ee.setTemp[1], 600, 990); // 60 to 99F
  ee.thresh = constrain(ee.thresh, 1, 100); // 0.1 to 10.0
}

void eeWrite() // write the settings if changed
{
  uint16_t sum = Fletcher16((uint8_t *)&ee, sizeof(eeSet));

  if(sum == ee_sum){
    return; // Nothing has changed?
  }
  ee_sum = sum;
  wifi.eeWriteData(64, (uint8_t*)&ee, sizeof(ee)); // WiFiManager already has an instance open, so use that at offset 64+
}

void eeRead()
{
  uint8_t addr = 64;
  eeSet eeTest;

  wifi.eeReadData(64, (uint8_t*)&eeTest, sizeof(eeSet));
  if(eeTest.check != 0xAAAA) return; // Probably only the first read or if struct size changes
  memcpy(&ee, &eeTest, sizeof(eeSet));
  ee_sum = Fletcher16((uint8_t *)&ee, sizeof(eeSet));
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

// Returns the temp setting active in current time range
bool activeTemp()
{
  time_t t = now();
  long timeNow = (hour(t)*60) + minute(t);
  return (timeNow >= ee.timeSch[0] && timeNow < ee.timeSch[1]) ? 0:1;
}

// Send logging data to a server.  This sends JSON formatted data to my local PC, but change to anything needed.
void ctSendLog()
{
  String url = "/s?waterbedLog={\"temp\": ";
  url += sDec(currentTemp);
  url += ",\"setTemp\": ";
  url += sDec(ee.setTemp[activeTemp()]);
  url += ",\"active\": ";
  url += activeTemp() ? "\"N\"":"\"D\"";
  url += ",\"on\": ";
  url += bHeater;
  url += "}";
  ctSend(url);
}

// Send local IP on start for comm, or bad password attempt IP when caught
void ctSendIP(bool local, uint32_t ip)
{
  String s = local ? "/s?waterbedIP=\"" : "/s?waterbedHackIP=\"";
  s += ipString(ip);
  if(local)
  {
    s += ":";
    s += serverPort;
  }
  s += "\"";

  ctSend(s);
}

// Send stuff to a server.
void ctSend(String s)
{
  if(ee.dataServer[0] == 0) return;
  WiFiClient client;
  if (!client.connect(ee.dataServer, ee.dataServerPort)) {
    Serial.println("dataServer connection failed");
    delay(100);
    return;
  }
  Serial.println("dataServer connected");

  // This will send the request to the server
  client.print(String("GET ") + s + " HTTP/1.1\r\n" +
               "Host: " + ee.dataServer + "\r\n" + 
               "Connection: close\r\n\r\n");

  delay(10);
 
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
//  Serial.println("closing connection.");
  client.stop();
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
      tmElements_t tm;
      tm.Year   = line.substring(6,10).toInt() - 1970;
      tm.Month  = line.substring(11,13).toInt();
      tm.Day    = line.substring(14,16).toInt();
      tm.Hour   = line.substring(17,19).toInt();
      tm.Minute = line.substring(20,22).toInt();
      tm.Second = line.substring(23,25).toInt();

      setTime(makeTime(tm));  // set time
      breakTime(now(), tm);   // update local tm for adjusting

      tm.Hour += ee.tz + IsDST();   // time zone change
      if(tm.Hour > 23) tm.Hour -= 24;

      setTime(makeTime(tm)); // set it again

      Serial.println("Time updated");
    }
  }
 
  client.stop();
  return true;
}

bool IsDST()
{
  uint8_t m = month();
  if (m < 3 || m > 11)  return false;
  if (m > 3 && m < 11)  return true;
  int8_t previousSunday = day() - weekday();
  if(m == 2) return previousSunday >= 8;
  return previousSunday <= 0;
}
