#include <stdio.h>
#include <string.h> // strncmp, strlen, strtok, etc.
#include <stdlib.h> // atoi, strtoul, strtol, sscanf
#include <stdint.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "wspr_encoder.h"
#include "timebase.h"
#include "radio_arbiter.h"

// Forward decls from task_wspr.c (or expose these in wspr_encoder.h; see note below)
static void wspr_start(void *user){ /* set SI5351 base freq, symbol task kickoff */ }
static void wspr_stop(void *user){  /* disable outputs, cleanup */ }

void wspr_set_callsign(const char *cs);
void wspr_set_grid(const char *grid);
void wspr_set_power_dbm(int dbm);
void wspr_set_rf_base_hz(uint32_t hz);
uint32_t wspr_get_rf_base_hz(void);
void wspr_set_tone_step_uHz(uint32_t uHz);
uint32_t wspr_get_tone_step_uHz(void);

// ---------------- console helpers ----------------

static void console_handle_wspr(char *args)
{
  // commands:
  //   wspr show
  //   wspr set call KI5YNG
  //   wspr set grid EM53
  //   wspr set pwr  13
  //   wspr win 0,2,4,6,8        (even minutes only)
  //   wspr win mask 0x1F
  //   wspr rf base 140956000     (Hz)
  //   wspr rf step 1464844       (uHz)

  if (!strncmp(args, "test", 4))
  {
    uint64_t start_ms = timebase_now_boot_ms() + 2000;
    radio_req_t r = {
        .mode = MODE_WSPR,
        .t_start_ms = start_ms,
        .duration_ms = 5000, // short test burst
        .freq_hz = wspr_get_rf_base_hz(),
        .start_cb = wspr_start,
        .stop_cb = wspr_stop,
        .user = NULL,
        .priority = 2};
    LOGI("wspr: test queue now+2s");
    radio_arbiter_submit(&r);
    return;
  }

  if (!args || !*args)
  {
    LOGI("wspr usage: show|set call <C>|set grid <G>|set pwr <dBm>|win <list>|win mask <hex>|rf base <Hz>|rf step <uHz>");
    return;
  }

  if (!strncmp(args, "show", 4))
  {
    LOGI("wspr: windows mask=0x%08lx, rf_base=%u Hz, tone_step=%u uHz",
         (unsigned long)wspr_minutes_mask_get(),
         wspr_get_rf_base_hz(),
         wspr_get_tone_step_uHz());
    return;
  }

  if (!strncmp(args, "set call ", 9))
  {
    wspr_set_callsign(args + 9);
    LOGI("wspr: call=%s", args + 9);
    return;
  }

  if (!strncmp(args, "set grid ", 9))
  {
    wspr_set_grid(args + 9);
    LOGI("wspr: grid=%s", args + 9);
    return;
  }

  if (!strncmp(args, "set pwr ", 8))
  {
    int dbm = atoi(args + 8);
    wspr_set_power_dbm(dbm);
    LOGI("wspr: pwr=%d dBm", dbm);
    return;
  }

  if (!strncmp(args, "win mask ", 9))
  {
    unsigned int tmp = 0;
    if (sscanf(args + 9, "%i", (int *)&tmp) == 1)
    { // accepts 0x.. or decimal
      wspr_minutes_mask_set((uint32_t)tmp);
      LOGI("wspr: windows mask=0x%08lx", (unsigned long)tmp);
    }
    else
    {
      LOGW("wspr: couldn't parse mask");
    }
    return;
  }

  if (!strncmp(args, "win ", 4))
  {
    // parse comma list of even minutes
    uint32_t m = 0;
    char *p = args + 4;
    while (*p)
    {
      while (*p == ' ' || *p == ',')
        p++;
      if (!*p)
        break;
      char *endp = p;
      long v = strtol(p, &endp, 10);
      p = endp;
      if (v < 0 || v > 58 || (v & 1))
      {
        LOGW("wspr: minute %ld ignored (must be even 0..58)", v);
        continue;
      }
      m |= (1u << ((int)v / 2));
    }
    wspr_minutes_mask_set(m);
    LOGI("wspr: windows mask=0x%08lx", (unsigned long)m);
    return;
  }

  if (!strncmp(args, "rf base ", 8))
  {
    uint32_t hz = (uint32_t)strtoul(args + 8, NULL, 10);
    wspr_set_rf_base_hz(hz);
    LOGI("wspr: rf base=%u Hz", hz);
    return;
  }

  if (!strncmp(args, "rf step ", 8))
  {
    uint32_t uHz = (uint32_t)strtoul(args + 8, NULL, 10);
    wspr_set_tone_step_uHz(uHz);
    LOGI("wspr: tone step=%u uHz", uHz);
    return;
  }

  LOGW("wspr: unknown subcommand");
}

static void console_handle_line(char *line)
{
  // Trim leading spaces
  while (*line == ' ' || *line == '\t')
    line++;
  if (!*line)
    return;

  if (!strncmp(line, "wspr ", 5))
  {
    console_handle_wspr(line + 5);
    return;
  }

  // Add other command namespaces here later...
  LOGI("unknown cmd: %s", line);
}

static void console_thread(void *arg)
{
  (void)arg;
  // Give USB CDC a moment to enumerate
  vTaskDelay(pdMS_TO_TICKS(500));

  char buf[128];
  size_t idx = 0;

  LOGI("console ready");

  for (;;)
  {
    int ch = getchar_timeout_us(0); // non-blocking
    if (ch >= 0)
    {
      if (ch == '\r' || ch == '\n')
      {
        if (idx)
        {
          buf[idx] = 0;
          console_handle_line(buf);
          idx = 0;
        }
      }
      else if (idx < sizeof(buf) - 1)
      {
        buf[idx++] = (char)ch;
      }
    }
    // You can remove the tick log if it's noisy:
    // LOGI("tick %lu", (unsigned long)xTaskGetTickCount());
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void task_console_start(void)
{
  xTaskCreate(
      console_thread,
      "console",
      1024, // stack words
      NULL,
      tskIDLE_PRIORITY + 1,
      NULL);
}