#include "eeMem.h"
#include <EEPROM.h>

eeSet ee = { sizeof(eeSet), 0xAAAA,
  "",  // saved SSID
  "", // router password
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
  1457,0,
};

eeMem::eeMem()
{
  EEPROM.begin(512);

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

void eeMem::update() // write the settings if changed
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)&ee, sizeof(eeSet));

  if(old_sum == ee.sum)
    return; // Nothing has changed?

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
