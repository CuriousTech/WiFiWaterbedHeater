#include "eeMem.h"
#include <EEPROM.h>

extern void eSend(String s);

eeSet ee = {
  sizeof(eeSet), 0xAAAA,
  "",  // saved SSID
  "", // router password
  800, -5, // vacaTemp, TZ
  5,       // schedCnt
  false,  // vacation mode
  true,  // average
  true,   // OLED
  false,
  {
    {825,  3*60, 3, 0, "Midnight"},
    {815,  6*60, 5, 0, "Early"},  // temp, time, thresh, wday
    {810,  8*60, 3, 0, "Morning"},
    {810, 16*60, 3, 0, "Day"},
    {825, 21*60, 3, 0, "Night"},
    {830,  0*60, 3, 0, "Sch6"},
    {830,  0*60, 3, 0, "Sch7"},
    {830,  0*60, 3, 0, "Sch8"}
  },
  1450, // ppkwh (0.145)
  58, // rate
  290, // watts (mbr = 290  gbr = 352)
//  {1138,1273,1285, 1218,666,330, 222,202,475, 863,1170,1081}, // cost months 2018
  {0}, // cost months
  {0},
  {0,0}, // tAdj[2]
  {
    {0, 1000, 8*60, 0x3E,"Weekday"},
    {0},
  }, // alarms
//end of CRC (low priority below)
  {1024,0}, // lightlevel
  0.0,
  0.0,
};

eeMem::eeMem()
{
  EEPROM.begin(1024);
  verify(false);
}

bool eeMem::update(bool bForce) // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, offsetof(eeSet, lightLevel) );

  if(bForce == false && old_sum == ee.sum)
  {
    return false; // Nothing has changed?
  }

  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, offsetof(eeSet, lightLevel) );

  uint8_t *pData = (uint8_t *)&ee;
  for(int addr = 0; addr < sizeof(eeSet); addr++)
  {
    EEPROM.write(addr, pData[addr] );
  }
  EEPROM.commit();
  return true;
}

bool eeMem::verify(bool bComp)
{
  uint8_t data[sizeof(eeSet)];
  uint16_t *pwTemp = (uint16_t *)data;

  for(int addr = 0; addr < sizeof(eeSet); addr++)
  {
    data[addr] = EEPROM.read( addr );
  }
  if(pwTemp[0] != sizeof(eeSet))
    return false; // revert to defaults if struct size changes
  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, offsetof(eeSet, lightLevel) );
  if(pwTemp[1] != sum) return false; // revert to defaults if sum fails

  if(bComp)
    return ( !memcmp(&ee, data, sizeof(eeSet) ) ); // don't load
  memcpy(&ee, data, sizeof(eeSet) );
  return true;
}

uint16_t eeMem::Fletcher16( uint8_t* data, int count)
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
