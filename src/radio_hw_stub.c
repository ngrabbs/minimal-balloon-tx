#include "radio_hw.h"
#include "logging.h"

static bool s_on = false;
static uint32_t s_freq = 0;

void radio_hw_init(void){
  LOGI("[RADIO] init (stub)");
}

void radio_hw_enable(bool on){
  s_on = on;
  LOGI("[RADIO] %s", on ? "EN" : "DIS");
}

void radio_hw_set_freq_hz(uint32_t hz){
  s_freq = hz;
  LOGI("[RADIO] f=%u Hz", hz);
}

void radio_hw_stop_all(void){
  s_on = false;
  s_freq = 0;
  LOGI("[RADIO] stop_all");
}