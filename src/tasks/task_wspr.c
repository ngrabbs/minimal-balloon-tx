#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "wspr_encoder.h"
#include <string.h>

// Global live config (can be tweaked at runtime)
static wspr_cfg_t g_cfg = { "KI5YNG", "EM53", 13 };

static void wspr_demo_thread(void *arg){
  (void)arg;
  vTaskDelay(pdMS_TO_TICKS(2000)); // let console settle
  for(;;){
    wspr_frame_t frame;
    if (wspr_build_frame(&g_cfg, &frame)){
      LOGI("[WSPR] Built frame:");
      wspr_print_frame(&g_cfg, &frame);
    } else {
      LOGE("[WSPR] Build failed (check callsign/grid/power)");
    }
    // Rebuild every ~30s just to show we can; in real use, schedule to even UTC mins
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

void task_wspr_start(void){
  xTaskCreate(wspr_demo_thread, "wspr", 2048, NULL, tskIDLE_PRIORITY+1, NULL);
}

// Public setters you could call from console/JSON/etc.
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