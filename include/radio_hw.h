#pragma once
#include <stdint.h>
#include <stdbool.h>

// Turn the RF path on/off (PA/buffer etc.). For SI5351 this might just enable/disable CLKx.
void radio_hw_enable(bool on);

// Set RF output frequency in Hz. For SI5351 you’ll set CLKx to this freq.
void radio_hw_set_freq_hz(uint32_t hz);

// Optional: called once on boot by the arbiter
void radio_hw_init(void);

// Optional: stop all outputs (called by arbiter after a window ends)
void radio_hw_stop_all(void);