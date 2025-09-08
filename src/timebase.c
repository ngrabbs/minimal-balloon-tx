#include "timebase.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pico/stdlib.h"
#include <time.h>

static uint64_t s_base_unix_ms = 0;     // epoch ms at the moment we latched
static TickType_t s_base_tick = 0;      // RTOS ticks at that same moment
static bool s_valid = false;

static uint64_t ms_from_ymd_hms_ms(int Y, int M, int D, int h, int m, int s, int ms){
    struct tm tmv;
    tmv.tm_year = Y - 1900;  // years since 1900
    tmv.tm_mon  = M - 1;     // 0..11
    tmv.tm_mday = D;
    tmv.tm_hour = h;
    tmv.tm_min  = m;
    tmv.tm_sec  = s;
    tmv.tm_isdst = 0;
    // mktime assumes localtime; RP2040 newlib defaults to UTC if TZ not set. We’ll treat it as UTC.
    time_t secs = mktime(&tmv);
    return (uint64_t)secs * 1000ULL + (uint64_t)(ms >= 0 ? ms : 0);
}

void timebase_init(void){
    s_valid = false;
    s_base_unix_ms = 0;
    s_base_tick = xTaskGetTickCount();
}

/* RMC gives ddmmyy + hhmmss.sss. Convert to full year, build epoch, and latch. */
void timebase_set_utc_from_rmc(int hh, int mm, int ss, int dd, int mo, int yy, int ms){
    int fullY = (yy >= 70 ? 1900 + yy : 2000 + yy);  // handle 00..69 => 2000..2069
    uint64_t new_ms = ms_from_ymd_hms_ms(fullY, mo, dd, hh, mm, ss, ms);
    taskENTER_CRITICAL();
    s_base_unix_ms = new_ms;
    s_base_tick = xTaskGetTickCount();
    s_valid = true;
    taskEXIT_CRITICAL();
}

uint64_t timebase_now_ms(void){
    taskENTER_CRITICAL();
    uint64_t base = s_base_unix_ms;
    TickType_t bt = s_base_tick;
    bool ok = s_valid;
    taskEXIT_CRITICAL();

    if (!ok) return 0;
    TickType_t now = xTaskGetTickCount();
    uint32_t elapsed_ms = (uint32_t)((now - bt) * (1000 / configTICK_RATE_HZ));
    return base + elapsed_ms;
}

uint32_t timebase_now_unix(void){
    return (uint32_t)(timebase_now_ms() / 1000ULL);
}

bool timebase_is_valid(void){
    return s_valid;
}