#pragma once
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"

typedef struct {
  int fix_valid; double lat, lon; float alt_m;
  uint32_t unix_time; uint8_t sats; float hdop;
} gps_fix_t;

typedef struct {
  gps_fix_t gps;
  float vbatt_v;
  int8_t temp_c;
} telemetry_t;

extern QueueHandle_t q_gps_fixes;      // gps_fix_t
extern QueueHandle_t q_tx_jobs;        // union of wspr/horus jobs
extern EventGroupHandle_t eg_system;   // bits: PPS_LOCK, GPS_LOCK

void msg_bus_init(void);
