#pragma once
#include <stdint.h>
#include <stdbool.h>

#define WSPR_SYMS 162

typedef struct {
  char callsign[7];   // up to 6 chars + NUL
  char grid[5];       // 4 chars + NUL (e.g., EM53)
  int  power_dbm;     // 0..60 typical (use 13 for ~20 mW)
} wspr_cfg_t;

typedef struct {
  // Stages for inspection/printing
  uint8_t  payload50[7];         // 50-bit payload packed MSB-first across 7 bytes (top 50 bits used)
  uint8_t  conv162_bits[162];    // after convolution, before interleave (bits 0/1)
  uint8_t  interleaved_bits[162];// after interleave (data bits)
  uint8_t  sync_bits[162];       // sync vector (0/1)
  uint8_t  symbols[WSPR_SYMS];   // 0..3 tones (sync + 2*data)
} wspr_frame_t;

bool wspr_build_frame(const wspr_cfg_t *cfg, wspr_frame_t *out);

// Convenience helpers
void wspr_print_frame(const wspr_cfg_t *cfg, const wspr_frame_t *f);
uint32_t wspr_minutes_mask_get(void);
void     wspr_minutes_mask_set(uint32_t mask);
// helper to decide if this even-UTC minute is one of the enabled windows
bool wspr_should_tx_in_minute(int even_minute); // pass 0..59 (must be even)

void wspr_update_grid_from_latlon(double lat_deg, double lon_deg);

// Optional setters exposed for console/GPS integration:
void wspr_set_callsign(const char *cs);
void wspr_set_grid(const char *grid);
void wspr_set_power_dbm(int dbm);