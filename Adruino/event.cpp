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

#include "event.h"

void eventHandler::set(WiFiClient c, int interval)
{
  for(int i = 0; i < CLIENTS; i++) // find an unused client
    if(!ec[i].inUse())
    {
      ec[i].set(c, interval);
      ec[i].push(jsonCallback());
      break;
    }
}

void eventHandler::heartbeat()
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].beat(jsonCallback());
}

void eventHandler::push() // push to all
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].push(jsonCallback());
}

void eventHandler::print(String s) // print remote debug
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].print(s);
}

void eventHandler::alert(String s) // send alert event
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].alert(s);
}

void eventClient::set(WiFiClient cl, int t)
{
  m_client = cl;
  m_interval = t;
  m_timer = 0;
  m_keepAlive = 10;
  m_client.print(":ok\n");
}

bool eventClient::inUse()
{
  return m_client.connected();
}

void eventClient::push(String s)
{
  if(m_client.connected() == 0)
    return;
  m_client.print("event: state\n");
  m_client.print("data: " + s + "\n");
  m_keepAlive = 11;
  m_timer = 0;
}

void eventClient::print(String s)
{
  m_client.print("event: print\ndata: " + s + "\n");
}

void eventClient::alert(String s)
{
  m_client.print("event: alert\ndata: " + s + "\n");
}

void eventClient::beat(String s)
{
  if(m_client.connected() == 0)
    return;

  if(++m_timer >= m_interval)
    push(s);
 
  if(--m_keepAlive <= 0)
  {
    m_client.print("\n");
    m_keepAlive = 10;
  }
}
