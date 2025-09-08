// src/tasks/task_wsched.c (adapted for 2-min)
#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "timebase.h"
#include "radio_arbiter.h"
#include "wspr_encoder.h"

static void wspr_start(void *user){ /* set SI5351 base freq, symbol task kickoff */ }
static void wspr_stop(void *user){  /* disable outputs, cleanup */ }

static void wsched_task(void *arg){
  (void)arg;
  while (!timebase_is_valid()){ vTaskDelay(pdMS_TO_TICKS(500)); }
  LOGI("wsched: UTC valid; scheduling WSPR on even minutes.");

  for(;;){
    uint64_t now_ms = timebase_now_ms();
    uint32_t now_s  = (uint32_t)(now_ms/1000ULL);
    uint32_t next   = (now_s/120)*120; // floor to 2-min
    if (next <= now_s) next += 120;    // next 2-min boundary

    int32_t delta_ms = (int32_t)((int64_t)next*1000LL - (int64_t)now_ms);
    if (delta_ms > 0) vTaskDelay(pdMS_TO_TICKS(delta_ms));

    // Only schedule for even minutes
    uint32_t min = (next/60) % 60;
    if ((min % 2) == 0 && wspr_should_tx_in_minute(min)) {
      radio_req_t r = {
        .mode        = MODE_WSPR,
        .t_start_ms  = (uint64_t)next * 1000ULL,
        .duration_ms = 111000,       // 110.6s + a small guard
        .freq_hz     = 14097120,
        .start_cb    = wspr_start,
        .stop_cb     = wspr_stop,
        .user        = NULL,
        .priority    = 2             // > HORUS
      };
      if (!radio_arbiter_submit(&r)){
        LOGW("wsched: WSPR submit failed (queue full?)");
      } else {
        LOGI("wsched: queued WSPR at %u", next);
      }
    }
  }
}

void task_wsched_start(void){
  xTaskCreate(wsched_task, "wsched", 1024, NULL, tskIDLE_PRIORITY+1, NULL);
}