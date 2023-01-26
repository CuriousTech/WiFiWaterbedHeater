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

#define EESIZE (offsetof(eeMem, end) - offsetof(eeMem, size) )

class eeMem
{
public:
  eeMem();
  bool update(bool bForce);
  bool verify(bool bComp);
private:
  uint16_t Fletcher16( uint8_t* data, int count);
public:
  uint16_t size = EESIZE;          // if size changes, use defauls
  uint16_t sum = 0xAAAA;           // if sum is diiferent from memory struct, write
  char     szSSID[32];
  char     szSSIDPassword[64];
  uint16_t vacaTemp = 700;       // vacation temp
  int8_t  tz = -5;            // Timezone offset from your global server
  uint8_t schedCnt = 5;    // number of active scedules
  bool    bVaca = false;         // vacation enabled
  bool    bAvg = true;         // average target between schedules
  bool    bEnableOLED = true;
  bool    bEco = true;         // eco mode
  Sched   schedule[MAX_SCHED] =  // 22x8 bytes
  {
    {825,  3*60, 3, 0, "Midnight"},
    {815,  6*60, 2, 0, "Early"},  // temp, time, thresh, wday
    {810,  8*60, 3, 0, "Morning"},
    {810, 16*60, 3, 0, "Day"},
    {825, 21*60, 3, 0, "Night"},
    {830,  0*60, 3, 0, "Sch6"},
    {830,  0*60, 3, 0, "Sch7"},
    {830,  0*60, 3, 0, "Sch8"}
  };
  uint16_t ppkwh = 152;
  uint16_t rate = 58;
  uint16_t watts =  290; // watts (mbr = 290  GBR = 352);
  uint16_t ppkwm[12] = {0};
  uint32_t tSecsMon[12] = {0};    // total secwatt hours per month
  int16_t tAdj[2] = {0};
  int16_t pids[3] = {60*3,60*1, 5}; //  {60*5,60*10, 20} // GBR pids
  int8_t res[8] = {0};
  Alarm   alarm[MAX_SCHED] =
  {
    {0, 1000, 8*60, 0x3E,"Weekday"},
    {0},
  };

  uint8_t end;
};

extern eeMem ee;
#endif // EEMEM_H
