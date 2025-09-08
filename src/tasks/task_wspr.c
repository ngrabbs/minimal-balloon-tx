#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "pico/time.h"        // absolute_time_t, sleep_until, get_absolute_time
#include "wspr_encoder.h"
#include "radio_hw.h"
#include <string.h>
#include <math.h>
#include <stdatomic.h>

// ===== Live config (editable at runtime via your console or GPS) =====
static wspr_cfg_t g_cfg = { "KI5YNG", "EM53", 13 };

// RF plan: lower tone (symbol 0) frequency in Hz and tone spacing in micro-Hz
static _Atomic uint32_t g_rf_base_hz = 140956000;   // EXAMPLE: set this to your band/slot later
static _Atomic uint32_t g_tone_step_uHz = 1464844;  // 1.464844 Hz in micro-Hz (standard WSPR)

// ===== Public setters (you already used some) =====
void wspr_set_callsign(const char *cs){
  if (!cs) return;
  strncpy(g_cfg.callsign, cs, sizeof(g_cfg.callsign)-1);
  g_cfg.callsign[sizeof(g_cfg.callsign)-1]=0;
}
void wspr_set_grid(const char *grid){
  if (!grid) return;
  strncpy(g_cfg.grid, grid, sizeof(g_cfg.grid)-1);
  g_cfg.grid[sizeof(g_cfg.grid)-1]=0;
}
void wspr_set_power_dbm(int dbm){ g_cfg.power_dbm = dbm; }

void wspr_set_rf_base_hz(uint32_t hz){ atomic_store(&g_rf_base_hz, hz); }
uint32_t wspr_get_rf_base_hz(void){ return atomic_load(&g_rf_base_hz); }

void wspr_set_tone_step_uHz(uint32_t uHz){ atomic_store(&g_tone_step_uHz, uHz); }
uint32_t wspr_get_tone_step_uHz(void){ return atomic_load(&g_tone_step_uHz); }

// ===== Keyer control =====
static TaskHandle_t s_keyer_task = NULL;
static _Atomic bool s_keyer_run = false;

typedef struct {
  wspr_frame_t frame;
  uint32_t f0_hz;
  uint32_t step_uHz;
} keyer_ctx_t;

static keyer_ctx_t s_ctx;

// 8192 / 12000 s per symbol = 0.6826666667 s
#define WSPR_SYMBOL_US  (uint64_t)682666

static void wspr_keyer_task(void *arg){
  (void)arg;
  LOGI("[WSPR] keyer task up");

  for(;;){
    // Wait until start is requested
    while (!atomic_load(&s_keyer_run)){
      vTaskDelay(pdMS_TO_TICKS(5));
    }

    uint32_t f0 = s_ctx.f0_hz;
    uint32_t step_uHz = s_ctx.step_uHz;

    // Precompute 4 tone freqs (integer Hz; SI5351 can do fractional via multisynth if you want later)
    // For now we round to Hz; later you can keep a fractional accumulator or use SI5351 fractional synth.
    uint32_t f_tone[4];
    for (int i=0;i<4;i++){
      // nearest Hz
      uint64_t offset_uHz = (uint64_t)i * step_uHz;
      uint32_t off_hz = (uint32_t)((offset_uHz + 500000)/1000000); // round
      f_tone[i] = f0 + off_hz;
    }

    // Align our symbol schedule from *now*
    absolute_time_t t = get_absolute_time();
    // Give a tiny setup lead (1ms) then start
    t = delayed_by_ms(t, 1);

    radio_hw_enable(true);

    for (int n=0; n<WSPR_SYMS && atomic_load(&s_keyer_run); n++){
      uint8_t sym = s_ctx.frame.symbols[n] & 3u;
      radio_hw_set_freq_hz(f_tone[sym]);

      // Next symbol deadline
      t = delayed_by_us(t, WSPR_SYMBOL_US);
      sleep_until(t);
    }

    radio_hw_enable(false);
    atomic_store(&s_keyer_run, false);
    LOGI("[WSPR] keyer done");
  }
}

// ===== Arbiter callbacks =====
static void wspr_build_now(void){
  if (!s_keyer_task){
    xTaskCreate(wspr_keyer_task, "wsprkey", 2048, NULL, tskIDLE_PRIORITY+3, &s_keyer_task);
  }
  // Build a fresh frame with current config
  wspr_frame_t f;
  if (!wspr_build_frame(&g_cfg, &f)){
    LOGE("[WSPR] build failed (cfg?)");
    memset(&s_ctx, 0, sizeof(s_ctx));
    return;
  }
  s_ctx.frame = f;
  s_ctx.f0_hz = atomic_load(&g_rf_base_hz);
  s_ctx.step_uHz = atomic_load(&g_tone_step_uHz);
}

void wspr_start(void *user){
  (void)user;
  LOGI("[WSPR] START");
  wspr_build_now();
  atomic_store(&s_keyer_run, true);
}

void wspr_stop(void *user){
  (void)user;
  LOGI("[WSPR] STOP");
  atomic_store(&s_keyer_run, false);
  // arbiter will also call radio_hw_stop_all(); that’s fine.
}