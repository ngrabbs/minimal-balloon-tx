#include "msg_bus.h"

QueueHandle_t q_gps_fixes;
QueueHandle_t q_tx_jobs;
EventGroupHandle_t eg_system;

void msg_bus_init(void) {
  // Create only what your console task might touch (or nothing yet)
  eg_system = xEventGroupCreate();
  q_gps_fixes = xQueueCreate(4, sizeof(int)); // placeholder type
  q_tx_jobs   = xQueueCreate(4, sizeof(int));
}