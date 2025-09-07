#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "logging.h"
#include "msg_bus.h"
#include "boards/pico_wspr_horus.h"

static void gps_thread(void *arg) {
  (void)arg; char line[120]; size_t idx=0;
  for(;;){
    while (uart_is_readable(UART_GPS_ID)) {
      char c = uart_getc(UART_GPS_ID);
      if (c=='\n' || idx>=sizeof(line)-1) { line[idx]=0; idx=0; /* parse NMEA GGA/RMC here */ }
      else if (c!='\r') line[idx++]=c;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void task_gps_start(void){
  uart_init(UART_GPS_ID, UART_GPS_BAUD);
  gpio_set_function(UART_GPS_TX, GPIO_FUNC_UART);
  gpio_set_function(UART_GPS_RX, GPIO_FUNC_UART);
  xTaskCreate(gps_thread, "gps", 1024, NULL, tskIDLE_PRIORITY+2, NULL);
}
