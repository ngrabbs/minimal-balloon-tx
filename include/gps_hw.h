#pragma once
#include <stdbool.h>
#include <stdint.h>

void gps_power_on_battery_on(void);
void gps_power_off_battery_on(void);
void gps_power_off(void);
void gps_hard_reset(void);

void gps_uart_enable(void);
void gps_uart_disable(void);

bool gps_send_ubx(const uint8_t *payload, uint16_t len); // stub-safe

// “Modes” approximating your C++ API:
void gps_enable_configuration_mode(void);  // high-alt mode + maximal NMEA (stub)
void gps_enable_flight_mode(void);         // high-alt mode + minimal NMEA (stub)
void gps_enter_monitor_mode(void);         // start streaming NMEA to console
void gps_disable(void);                    // stop monitor + power off(batt on)