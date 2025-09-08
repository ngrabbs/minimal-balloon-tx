// src/tasks/task_hsched.c
#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "timebase.h"
#include "radio_arbiter.h"

static void horus_start(void *user){ /* set SI5351, stream 4FSK symbols */ }
static void horus_stop(void *user){  /* stop */ }

#define HORUS_BURST_MS   50000  // 50s example; ensure < 60s if you want every odd minute

static void hsched_task(void *arg){
  (void)arg;
  while (!timebase_is_valid()){ vTaskDelay(pdMS_TO_TICKS(500)); }
  LOGI("hsched: UTC valid; scheduling Horus on odd minutes.");

  for(;;){
    uint64_t now_ms = timebase_now_ms();
    uint32_t now_s  = (uint32_t)(now_ms/1000ULL);
    uint32_t next   = (now_s/60)*60 + 60;  // next minute boundary
    int32_t delta_ms = (int32_t)((int64_t)next*1000LL - (int64_t)now_ms);
    if (delta_ms > 0) vTaskDelay(pdMS_TO_TICKS(delta_ms));

    uint32_t min = (next/60) % 60;
    if ((min % 2) == 1) {  // odd minutes
      radio_req_t r = {
        .mode        = MODE_HORUS,
        .t_start_ms  = (uint64_t)next * 1000ULL,
        .duration_ms = HORUS_BURST_MS,     // must not overlap next even-minute WSPR
        .freq_hz     = 14097420,
        .start_cb    = horus_start,
        .stop_cb     = horus_stop,
        .user        = NULL,
        .priority    = 1
      };
      // Ensure this burst fits: 50s leaves ~10s guard before the next even-minute WSPR slot opens.
      (void)radio_arbiter_submit(&r);
    }
  }
}

void task_hsched_start(void){
  xTaskCreate(hsched_task, "hsched", 1024, NULL, tskIDLE_PRIORITY+1, NULL);
}