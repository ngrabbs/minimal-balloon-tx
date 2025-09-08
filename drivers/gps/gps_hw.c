#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "timebase.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "pico_wspr_horus.h"
#include "gps_hw.h"
#include <string.h> // strchr, strlen, memcpy, strncmp, strstr
#include <stdlib.h> // atoi, atof
#include <math.h>   // floor

static TaskHandle_t s_mon_task = NULL;   // task handle lives at file scope
static void gps_monitor_task(void *arg); // forward declaration

// --------- NMEA helpers ----------
static uint8_t hexval(char c) { return (c >= '0' && c <= '9')   ? (uint8_t)(c - '0')
                                       : (c >= 'A' && c <= 'F') ? (uint8_t)(10 + c - 'A')
                                       : (c >= 'a' && c <= 'f') ? (uint8_t)(10 + c - 'a')
                                                                : 0xFF; }

static bool nmea_checksum_ok(const char *s)
{
  // s like "$GPRMC,...*CS"
  if (!s || s[0] != '$')
    return false;
  const char *p = s + 1;
  uint8_t x = 0;
  while (*p && *p != '*' && *p != '\r' && *p != '\n')
  {
    x ^= (uint8_t)(*p++);
  }
  if (*p != '*')
    return false;
  if (!p[1] || !p[2])
    return false;
  uint8_t hi = hexval(p[1]), lo = hexval(p[2]);
  if (hi > 0x0F || lo > 0x0F)
    return false;
  uint8_t want = (uint8_t)((hi << 4) | lo);
  return x == want;
}

// --------- UART & power control ----------
void gps_uart_enable(void)
{
  uart_init(UART_GPS_ID, UART_GPS_BAUD);
  gpio_set_function(UART_GPS_TX, GPIO_FUNC_UART);
  gpio_set_function(UART_GPS_RX, GPIO_FUNC_UART);
}

void gps_uart_disable(void)
{
  // Return pins to GPIO to mirror the C++ “ReInit” behavior on TX
  gpio_set_function(UART_GPS_TX, GPIO_FUNC_SIO);
  gpio_set_dir(UART_GPS_TX, GPIO_OUT);
  gpio_put(UART_GPS_TX, 0);

  gpio_set_function(UART_GPS_RX, GPIO_FUNC_SIO);
  gpio_set_dir(UART_GPS_RX, GPIO_IN);

  uart_deinit(UART_GPS_ID);
}

void gps_power_on_battery_on(void)
{
  LOGI("GPS Power ON (load switch ON, reset high), Battery ON");

  // Configure control pins once
  static bool inited = false;
  if (!inited)
  {
    gpio_init(PIN_GPS_LOADSW);
    gpio_set_dir(PIN_GPS_LOADSW, GPIO_OUT);
    gpio_init(PIN_GPS_RESET);
    gpio_set_dir(PIN_GPS_RESET, GPIO_OUT);
    gpio_init(PIN_GPS_BATT_EN);
    gpio_set_dir(PIN_GPS_BATT_EN, GPIO_OUT);
    inited = true;
  }

  // Apply “battery on”
  gpio_put(PIN_GPS_BATT_EN, 1);

  // Load switch active-low: 0=enable power to module
  gpio_put(PIN_GPS_LOADSW, 0);

  // Release reset (1 = run)
  gpio_put(PIN_GPS_RESET, 1);

  // Give module time to boot
  vTaskDelay(pdMS_TO_TICKS(500));

  gps_uart_enable();
}

void gps_power_off_battery_on(void)
{
  LOGI("GPS Power OFF, Battery ON");
  gps_uart_disable();

  // Hold reset low so the module stops pulling current
  gpio_put(PIN_GPS_RESET, 0);

  // Disable load switch (1 = off)
  gpio_put(PIN_GPS_LOADSW, 1);

  // leave battery enabled
  gpio_put(PIN_GPS_BATT_EN, 1);
}

void gps_power_off(void)
{
  LOGI("GPS Power OFF, Battery OFF");
  gps_power_off_battery_on();
  gpio_put(PIN_GPS_BATT_EN, 0);
}

void gps_hard_reset(void)
{
  LOGI("GPS Hard Reset");
  // Pulse reset low briefly
  gpio_put(PIN_GPS_RESET, 0);
  vTaskDelay(pdMS_TO_TICKS(50));
  gpio_put(PIN_GPS_RESET, 1);
}

// ---------- UBX stubs ----------
bool gps_send_ubx(const uint8_t *payload, uint16_t len)
{
  // Replace with real UBX framing (0xB5 0x62 + payload + CK_A/CK_B)
  // For now, just write raw (safe stub)
  if (!payload || !len)
    return false;
  for (uint16_t i = 0; i < len; i++)
  {
    uart_putc_raw(UART_GPS_ID, payload[i]);
  }
  return true;
}

// ---------- Modes from your C++ ----------
static void send_high_altitude_mode(void)
{
  // TODO: fill with your UBX-CFG-NAV5 dynamic model for airborne
  // gps_send_ubx(ubx_nav5_payload, sizeof(ubx_nav5_payload));
  LOGI("gps: (stub) SendHighAltitudeMode");
}

static void send_msg_rate_maximal(void)
{
  // TODO: set NMEA/UBX message rates for “monitor mode” (frequent lines)
  LOGI("gps: (stub) SendModuleMessageRateConfigurationMaximal");
}

static void send_msg_rate_minimal(void)
{
  // TODO: set minimal output for flight (e.g., RMC/GGA 1 Hz)
  LOGI("gps: (stub) SendModuleMessageRateConfigurationMinimal");
}

static void send_save_configuration(void)
{
  // TODO: UBX-CFG-CFG save
  LOGI("gps: (stub) SendModuleSaveConfiguration");
}

// Public “modes”
void gps_enable_configuration_mode(void)
{
  gps_power_on_battery_on();
  send_high_altitude_mode();
  send_msg_rate_maximal();
  send_save_configuration();
}

void gps_enable_flight_mode(void)
{
  gps_power_on_battery_on();
  send_high_altitude_mode();
  send_msg_rate_minimal();
  send_save_configuration();
}

void gps_disable(void)
{
  gps_power_off_battery_on();
}

void gps_enter_monitor_mode(void)
{
  // Power on + lots of messages like the C++ version’s EnableConfigurationMode + EnterMonitorMode
  gps_enable_configuration_mode();

  if (!s_mon_task)
  {
    xTaskCreate(gps_monitor_task, "gpsmon", 1024, NULL, tskIDLE_PRIORITY + 2, &s_mon_task);
  }
}

// ---------- NMEA parse helpers ----------
static int split_csv(char *s, char *fields[], int maxf)
{
  int n = 0;
  char *p = s;
  while (n < maxf && p && *p)
  {
    fields[n++] = p;
    char *c = strchr(p, ',');
    if (!c)
      break;
    *c = 0;
    p = c + 1;
  }
  return n;
}
static int to_int(const char *s) { return (s && *s) ? atoi(s) : 0; }
static float to_float(const char *s) { return (s && *s) ? (float)atof(s) : 0.f; }

static bool parse_latlon_dm(const char *lat, const char *latH, const char *lon, const char *lonH,
                            double *outLatDeg, double *outLonDeg)
{
  if (!lat || !*lat || !lon || !*lon || !latH || !*latH || !lonH || !*lonH)
    return false;
  // lat: DDMM.mmmm, lon: DDDMM.mmmm
  double lat_d = floor(atof(lat) / 100.0);
  double lat_m = atof(lat) - lat_d * 100.0;
  double lon_d = floor(atof(lon) / 100.0);
  double lon_m = atof(lon) - lon_d * 100.0;
  double lat_deg = lat_d + lat_m / 60.0;
  double lon_deg = lon_d + lon_m / 60.0;
  if (latH[0] == 'S')
    lat_deg = -lat_deg;
  if (lonH[0] == 'W')
    lon_deg = -lon_deg;
  if (outLatDeg)
    *outLatDeg = lat_deg;
  if (outLonDeg)
    *outLonDeg = lon_deg;
  return true;
}

// keep your existing nmea_checksum_ok() helper

// ---------- Clean monitor task ----------
static TaskHandle_t s_mon_task;

static void gps_monitor_task(void *arg)
{
  (void)arg;
  LOGI("gps: monitor task started");
  vTaskDelay(pdMS_TO_TICKS(200)); // settle

  char line[160];
  size_t idx = 0;
  uint32_t last_report = 0;

  for (;;)
  {
    while (uart_is_readable(UART_GPS_ID))
    {
      char c = uart_getc(UART_GPS_ID);
      if (c == '\r')
        continue;
      if (c == '\n' || idx >= sizeof(line) - 1)
      {
        line[idx] = 0;
        idx = 0;

        if (line[0] == '$' && nmea_checksum_ok(line))
        {
          // make a working copy after '*'
          char buf[160];
          // copy up to but not including '*'
          char *star = strchr(line, '*');
          size_t ncopy = (star && (size_t)(star - line) < sizeof(buf) - 1) ? (size_t)(star - line) : strlen(line);
          memcpy(buf, line, ncopy);
          buf[ncopy] = 0;

          // identify sentence
          const char *id = buf + 1; // skip '$'
          char *fields[32];
          int nf = split_csv(buf + 7, fields, 32); // after "$GxXXX,"
          // ids: GPRMC/GNRMC, GPGGA/GNGGA
          if (!strncmp(id, "GPRMC", 5) || !strncmp(id, "GNRMC", 5))
          {
            // RMC: time, status, lat, N/S, lon, E/W, speed(kn), course, date, ...
            // fields[0]=hhmmss.sss, 1=status(A/V), 2=lat,3=N/S,4=lon,5=E/W, 6=speed,7=course,8=ddmmyy
            const char *t = fields[0];
            const char *st = fields[1];

            // parse h,m,s,ms
            int hh = 0, mm = 0, ss = 0, ms = 0;
            if (t && strlen(t) >= 6)
            {
              hh = (t[0] - '0') * 10 + (t[1] - '0');
              mm = (t[2] - '0') * 10 + (t[3] - '0');
              ss = (t[4] - '0') * 10 + (t[5] - '0');
              const char *dot = strchr(t, '.');
              if (dot && dot[1])
              {
                // take 3 digits of fractional seconds (pad as needed)
                int d1 = dot[1] ? dot[1] - '0' : 0;
                int d2 = dot[2] ? dot[2] - '0' : 0;
                int d3 = dot[3] ? dot[3] - '0' : 0;
                ms = d1 * 100 + d2 * 10 + d3;
              }
            }

            // parse ddmmyy
            int dd = 1, mo = 1, yy = 0;
            if (nf > 8 && fields[8] && strlen(fields[8]) == 6)
            {
              const char *dmy = fields[8];
              dd = (dmy[0] - '0') * 10 + (dmy[1] - '0');
              mo = (dmy[2] - '0') * 10 + (dmy[3] - '0');
              yy = (dmy[4] - '0') * 10 + (dmy[5] - '0');
            }

            bool valid = (st && *st == 'A');

            // discipline clock whenever we have a valid RMC
            if (valid)
            {
              timebase_set_utc_from_rmc(hh, mm, ss, dd, mo, yy, ms);
            }

            double latd = 0, lond = 0;
            bool have_ll = false;
            if (nf > 5)
              have_ll = parse_latlon_dm(fields[2], fields[3], fields[4], fields[5], &latd, &lond);
            if (xTaskGetTickCount() - last_report > pdMS_TO_TICKS(1000))
            {
              last_report = xTaskGetTickCount();
              if (valid && have_ll) {
                LOGI("[RMC] %c @ %c%c:%c%c:%c%c lat=%.6f lon=%.6f",
                     'A',
                     t ? t[0] : '?', t ? t[1] : '?', t ? t[2] : '?', t ? t[3] : '?',
                     t ? t[4] : '?', t ? t[5] : '?', t ? t[6] : '?', latd, lond);
                // Optional: only update when you cross into a new 4-char grid
                static char last_grid[5] = "";
                char g[5];
                // compute current grid
                {
                  extern void wspr_update_grid_from_latlon(double lat, double lon);
                  // small helper: reuse the same converter here or expose a getter
                  // For simplicity, call the encoder API:
                  wspr_update_grid_from_latlon(latd, lond); // sets internal cfg.grid
                }
                // if you want to log the change, you can recompute and compare,
                // or expose a wspr_get_grid() function. Minimal version:
                LOGI("[WSPR] grid updated from GPS");

              }
              else LOGI("[RMC] V (no fix yet)");
            }
          }
          else if (!strncmp(id, "GPGGA", 5) || !strncmp(id, "GNGGA", 5))
          {
            // GGA: time, lat, N/S, lon, E/W, fix(0/1/2), sats, HDOP, alt(m), …
            double latd = 0, lond = 0;
            bool have_ll = false;
            int fix = 0, sats = 0;
            float hdop = 0, alt = 0;
            if (nf >= 9)
            {
              have_ll = parse_latlon_dm(fields[1], fields[2], fields[3], fields[4], &latd, &lond);
              fix = to_int(fields[5]);
              sats = to_int(fields[6]);
              hdop = to_float(fields[7]);
              alt = to_float(fields[8]);
            }
            if (xTaskGetTickCount() - last_report > pdMS_TO_TICKS(1000))
            {
              last_report = xTaskGetTickCount();
              LOGI("[GGA] fix=%d sats=%d hdop=%.1f alt=%.1fm%s%s",
                   fix, sats, hdop, alt,
                   have_ll ? "" : " (no lat/lon)",
                   fix == 0 ? " (searching)" : "");
            }
          }
          else if (!strncmp(id, "GNZDA", 5) || !strncmp(id, "GPZDA", 5))
          {
            // Optional: time/date from ZDA
          }
          else if (strstr(line, "ANTENNA OPEN"))
          {
            LOGW("[GPS] ANTENNA OPEN (check antenna/connectors)");
          }
        }
        // else drop bad checksum lines silently
      }
      else
      {
        line[idx++] = c;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}