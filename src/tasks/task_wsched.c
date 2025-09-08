// src/tasks/task_wsched.c (adapted for 2-min)
#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "timebase.h"
#include "radio_arbiter.h"
#include "wspr_encoder.h"

static void wspr_start(void *user){ /* set SI5351 base freq, symbol task kickoff */ }
static void wspr_stop(void *user){  /* disable outputs, cleanup */ }

extern uint32_t wspr_get_rf_base_hz(void);
extern void wspr_start(void *user);
extern void wspr_stop(void *user);

static uint32_t next_even_boundary(uint32_t now){
  uint32_t m = (now/60)%60, s = now%60;
  uint32_t base = now - s;
  if ((m & 1)==0 && s==0) return now;      // exactly on even minute
  return now + ((m & 1) ? (60 - s) : (120 - s));
}

static void queue_wspr_epoch(uint32_t start_epoch){
  uint32_t min = (start_epoch/60)%60;
  if (!wspr_should_tx_in_minute((int)min)) {
    LOGI("wsched: skip %02u:%02u (mask off)", (start_epoch/3600)%24, min);
    return;
  }

  uint64_t start_boot_ms = timebase_epoch_to_boot_ms(start_epoch);
  if (!start_boot_ms){
    LOGW("wsched: UTC not mapped yet, skipping");
    return;
  }

  radio_req_t r = {
    .mode        = MODE_WSPR,
    .t_start_ms  = start_boot_ms,
    .duration_ms = 111000,
    .freq_hz     = wspr_get_rf_base_hz(),
    .start_cb    = wspr_start,
    .stop_cb     = wspr_stop,
    .user        = NULL,
    .priority    = 2
  };

  if (radio_arbiter_submit(&r)) {
    LOGI("wsched: queued WSPR %02u:%02u:%02u (boot_ms=%llu)",
         (start_epoch/3600)%24, (start_epoch/60)%60, start_epoch%60,
         (unsigned long long)start_boot_ms);
  } else {
    LOGW("wsched: submit failed for epoch %u (boot_ms=%llu)",
         start_epoch, (unsigned long long)start_boot_ms);
  }
}

static void wsched_task(void *arg){
  (void)arg;

  // wait until UTC valid (your existing code likely already does this)
  while (!timebase_utc_valid()) vTaskDelay(pdMS_TO_TICKS(200));

  LOGI("wsched: UTC valid; scheduling WSPR on even minutes.");
  LOGI("wsched: mask 0x%08lx", (unsigned long)wspr_minutes_mask_get());

  for(;;){
    uint32_t now = timebase_utc_now();
    uint32_t start = next_even_boundary(now);

    LOGI("wsched: now=%u next=%u (%02u:%02u:%02u)",
         now, start,
         (start/3600)%24, (start/60)%60, start%60);

    queue_wspr_epoch(start);

    // Sleep ~2 minutes; if you want to be exact, compute precise delay:
    vTaskDelay(pdMS_TO_TICKS(120000));
  }
}

void task_wsched_start(void){
  xTaskCreate(wsched_task, "wsched", 1024, NULL, tskIDLE_PRIORITY+1, NULL);
}