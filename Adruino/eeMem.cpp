#include "eeMem.h"
#include <EEPROM.h>
#undef ee

eeSet ee = { sizeof(eeSet), 0xAAAA,
  "",  // saved SSID
  "", // router password
  640, -5, // vacaTemp, TZ
  5,       // schedCnt
  false,  // vacation mode
  true,  // average
  true,   // OLED
  false,
  {
    {840,  3*60, 3, 0, "Midnight"},
    {820,  6*60, 5, 0, "Early"},  // temp, time, thresh, wday
    {812,  8*60, 3, 0, "Morning"},
    {812, 15*60+30, 3, 0, "Day"},
    {840, 20*60+30, 3, 0, "Night"},
    {830,  0*60, 3, 0, "Sch6"},
    {830,  0*60, 3, 0, "Sch7"},
    {830,  0*60, 3, 0, "Sch8"}
  },
  1450, // ppkwh
  58, // rate
  290, // watts (mbr = 290  gbr = 352)
//  {1138,1273,1285, 1218,666,330, 222,202,475, 863,1170,1081}, // cost months 2018
  {0}, // cost months
  {0},
  {0,0}, // tAdj[2]
  0.0,
  0.0,
  {
    {0, 1000, 8*60, 0x3E,"Weekday"},
    {0},
  }, // alarms
  {1024,0}, // lightlevel
};

eeMem::eeMem()
{
  EEPROM.begin(1024);

  uint8_t data[sizeof(eeSet)];
  uint16_t *pwTemp = (uint16_t *)data;

  int addr = 0;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
  {
    data[i] = EEPROM.read( addr );
  }

  if(pwTemp[0] != sizeof(eeSet)) return; // revert to defaults if struct size changes
  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, sizeof(eeSet) );
  if(pwTemp[1] != sum) return; // revert to defaults if sum fails
  memcpy(&ee, data, sizeof(eeSet) );
}

void eeMem::update(float currCost, float currWatts) // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, sizeof(eeSet));

  if(old_sum == ee.sum)
  {
    return; // Nothing has changed?
  }
  ee.fTotalCost = currCost;
  ee.fTotalWatts = currWatts;

  uint16_t addr = 0;
  uint8_t *pData = (uint8_t *)&ee;
  for(int i = 0; i < sizeof(eeSet); i++, addr++)
  {
    EEPROM.write(addr, pData[i] );
  }
  EEPROM.commit();
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
