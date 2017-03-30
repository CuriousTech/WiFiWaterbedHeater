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
  uint16_t costs[12];
  uint32_t res[2];
};

extern eeSet ee;

class eeMem
{
public:
  eeMem();
  void update(void);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
};

extern eeMem eemem;

#endif // EEMEM_H
