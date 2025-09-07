#pragma once
#include <stdint.h>
#include "telemetry.h"

// Prepare a 162‑symbol WSPR bit/symbol stream from callsign/locator/pwr.
int wspr_build_symbols(const char* call, const char* locator4, int dbm, uint8_t *out_symbols_162);
