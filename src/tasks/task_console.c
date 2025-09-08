#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"

static void console_thread(void *arg) {
  (void)arg;
  // Give USB CDC time to enumerate
  vTaskDelay(pdMS_TO_TICKS(500));

  for (;;) {
    LOGI("tick %lu", (unsigned long)xTaskGetTickCount());
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void task_console_start(void) {
  xTaskCreate(
    console_thread,
    "console",
    1024,          // stack words (adjust if you add prints/parsers)
    NULL,
    tskIDLE_PRIORITY + 1,
    NULL
  );
}