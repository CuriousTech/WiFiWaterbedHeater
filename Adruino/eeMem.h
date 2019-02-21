#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

struct Sched
{
  uint16_t setTemp;
  uint16_t timeSch;
  uint8_t thresh;
  uint8_t wday;  // Todo: weekday 0=any, 1-7 = day of week
  char    name[16]; // names for small display
};

struct Alarm
{
  uint16_t ms;  // 0 = inactive
  uint16_t freq;
  uint16_t timeSch;
  uint8_t wday;  // Todo: weekday 0=any, bits 0-6 = day of week
  char    name[16]; // names for small display
};

#define MAX_SCHED 8

struct eeSet // EEPROM backed data
{
  uint16_t size;          // if size changes, use defauls
  uint16_t sum;           // if sum is diiferent from memory struct, write
  char     szSSID[32];
  char     szSSIDPassword[64];
  uint16_t vacaTemp;       // vacation temp
  int8_t  tz;            // Timezone offset from your global server
  uint8_t schedCnt;    // number of active scedules
  bool    bVaca;         // vacation enabled
  bool    bAvg;         // average target between schedules
  bool    bEnableOLED;
  bool    bRes;
  Sched   schedule[MAX_SCHED];  // 22x8 bytes
  uint16_t ppkwh;
  uint16_t rate;
  uint16_t watts;
  uint16_t costs[12];
  uint16_t wh[12];    // watt hours per month
  uint16_t tAdj[2];
  Alarm   alarm[MAX_SCHED];
// end of CRC
  uint16_t lightLevel[2];
  float   fTotalCost;
  float   fTotalWatts;
};

class eeMem
{
public:
  eeMem();
  bool update(bool bForce);
  bool verify(bool bComp);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
};

extern eeMem eemem;
extern eeSet ee;
#endif // EEMEM_H
