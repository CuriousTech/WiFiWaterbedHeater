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

#include <WiFiClient.h>

#define CLIENTS 4

class eventClient
{
public:
  eventClient(){}
  void set(WiFiClient cl, int t);
  bool inUse();
  void push(String s);
  void print(String s);
  void beat(String s);
  void alert(String s);
private:
  WiFiClient m_client;
  int8_t m_keepAlive;
  uint16_t m_interval;
  uint16_t m_timer;
};

class eventHandler
{
public:
  eventHandler(String (*callback)(void))
  {
    jsonCallback = callback;
  }
  void set(WiFiClient c, int interval);
  void heartbeat();
  void push(); // push to all
  void print(String s); // print remote debug
  void alert(String s); // send alert
private:
  String (*jsonCallback)();
  eventClient ec[CLIENTS];
};
