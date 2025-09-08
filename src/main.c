#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "msg_bus.h"
//#include "boards/pico_wspr_horus.h"

extern void task_console_start(void);
extern void task_gps_start(void);
extern void radio_hw_init(void);
//extern void task_radio_start(void);
//extern void task_wspr_start(void);
//extern void task_horus_start(void);

int main() {
  sleep_ms(10000);
  stdio_init_all();
  LOGI("minimal-balloon-tx boot");

  msg_bus_init();

  radio_hw_init();
  task_console_start();
  task_radio_arbiter_start();
  task_gps_start();
  task_wsched_start();   // start scheduler that waits for 10-min marks
//  task_wspr_start();
//  task_radio_start();
//  task_wspr_start();
//  task_horus_start();

  vTaskStartScheduler();
  while (1) { }
}
