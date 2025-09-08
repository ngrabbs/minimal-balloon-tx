#pragma once
#include <stdint.h>
#include <stdbool.h>

void     timebase_init(void);

/* Set UTC from GPS (RMC). Year is two-digit (e.g., 25 for 2025). */
void     timebase_set_utc_from_rmc(int hh, int mm, int ss, int dd, int mo, int yy, int ms /*0-999*/);

/* Query time */
uint64_t timebase_now_ms(void);      // ms since Unix epoch
uint32_t timebase_now_unix(void);    // whole seconds since Unix epoch
bool     timebase_is_valid(void);    // true after first set