#pragma once
#include <stdint.h>
#include <stdbool.h>

bool si5351_init(void);
bool si5351_set_freq(uint8_t channel, uint32_t freq_hz); // CLK0 for RF
bool si5351_enable(uint8_t channel, bool en);
