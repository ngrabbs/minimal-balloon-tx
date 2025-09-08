#pragma once
#include <stdint.h>
#include <stdbool.h>

void     timebase_init(void);

/* Set UTC from GPS (RMC). Year is two-digit (e.g., 25 for 2025). */
void     timebase_set_utc_from_rmc(int hh, int mm, int ss,
                                   int day, int mon, int yy); // <-- shim (yy=00..99)

/* Query time */
uint64_t timebase_now_ms(void);      // ms since Unix epoch
uint32_t timebase_now_unix(void);    // whole seconds since Unix epoch
bool     timebase_is_valid(void);    // true after first set
bool     timebase_utc_valid(void); 
uint32_t timebase_utc_now(void);              // seconds since epoch (UTC)
void     timebase_set_utc_now(uint32_t epoch);// call when GPS gives you valid UTC
uint64_t timebase_epoch_to_boot_ms(uint32_t epoch_sec); // <-- add this
uint64_t timebase_now_boot_ms(void);          // convenience