// src/timebase.c
#include "timebase.h"
#include "pico/time.h"
#include <stdatomic.h>

static _Atomic bool     g_utc_valid = false;
static _Atomic uint32_t g_epoch0    = 0;    // UTC seconds when we latched
static uint64_t         g_boot0_ms  = 0;    // ms since boot when we latched

static inline bool is_leap(int y){ return (y%4==0 && (y%100!=0 || y%400==0)); }
static int days_before_month(int y, int m){  // m = 1..12
  static const int d[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
  int base = d[m-1];
  if (m>2 && is_leap(y)) base += 1;
  return base;
}
static uint32_t ymd_hms_to_epoch(int Y, int M, int D, int h, int m, int s){
  // days since 1970-01-01
  // Count days for full years
  int y = Y - 1;
  int days = (y - 1969)*365
           + (y/4 - 1970/4)
           - (y/100 - 1970/100)
           + (y/400 - 2000/400); // 2000/400==5, 1970/400==4, effect cancels with above terms
  // Simpler: accumulate from 1970 to Y-1:
  // But to keep it short, use a small loop if you prefer clarity over micro-optim:
  // (The formula above works; if you prefer, replace with a loop.)
  // Add days in this year up to previous month
  days += days_before_month(Y, M);
  // Add days in this month (D starts at 1)
  days += (D - 1);
  return (uint32_t)( (uint64_t)days*86400ULL + (uint64_t)h*3600ULL + (uint64_t)m*60ULL + (uint64_t)s );
}

bool timebase_is_valid(void){ return g_utc_valid; }
bool timebase_utc_valid(void){ return g_utc_valid; }

uint64_t timebase_now_boot_ms(void){
  return to_ms_since_boot(get_absolute_time());
}

// ---- SHIM for legacy callers ----
uint64_t timebase_now_ms(void){ return timebase_now_boot_ms(); }

// Latch UTC mapping using epoch seconds
void timebase_set_utc_now(uint32_t epoch_sec){
  g_epoch0   = epoch_sec;
  g_boot0_ms = timebase_now_boot_ms();
  g_utc_valid = true;
}

// ---- SHIM: accept RMC fields (yy=00..99, UTC) ----
void timebase_set_utc_from_rmc(int hh, int mm, int ss, int day, int mon, int yy){
  // UBX/NMEA RMC gives year as two digits: 00..99 => interpret as 2000..2099
  int year = (yy < 70) ? (2000 + yy) : (1900 + yy); // if your module is 20xx only, use 2000+yy
  uint32_t epoch = ymd_hms_to_epoch(year, mon, day, hh, mm, ss);
  timebase_set_utc_now(epoch);
}

uint32_t timebase_utc_now(void){
  if (!g_utc_valid) return 0;
  uint64_t now_ms = timebase_now_boot_ms();
  uint64_t delta_ms = (now_ms - g_boot0_ms);
  return (uint32_t)(g_epoch0 + (delta_ms / 1000ULL));
}

uint64_t timebase_epoch_to_boot_ms(uint32_t epoch_sec){
  if (!g_utc_valid) return 0;
  int64_t delta_s = (int64_t)epoch_sec - (int64_t)g_epoch0;
  return g_boot0_ms + (uint64_t)(delta_s * 1000LL);
}