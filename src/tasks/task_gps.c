#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "gps_hw.h"

static bool started = false;

static void gps_boot_task(void *arg){
  (void)arg;
  LOGI("gps: boot task running");
  // Now it's safe to use vTaskDelay() inside any called functions
  gps_enter_monitor_mode();
  LOGI("gps: monitor mode requested");
  vTaskDelete(NULL); // done
}

void task_gps_start(void){
  if (started) { LOGW("gps: task_gps_start called twice; ignoring"); return; }
  started = true;

  LOGI("gps: scheduling boot task");
  BaseType_t ok = xTaskCreate(
    gps_boot_task, "gps_boot", 1024, NULL, tskIDLE_PRIORITY+2, NULL);
  if (ok != pdPASS) {
    LOGE("gps: FAILED to create boot task");
  }
}