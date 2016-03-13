//missing file in SDK

struct tm
{
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

extern "C" {
  #include "user_interface.h"
  char* asctime(const tm *t);
  tm* localtime(const time_t *clock);
  time_t time(time_t * t);
  time_t mktime(struct tm *t);
}
