# minimal-balloon-tx (RP2040 + FreeRTOS)

> A clean, modular starting point for a Pico-based high‑altitude balloon transmitter with WSPR + Horus Binary, GPS time/pos, SI5351 synth, optional camera + storage. No fancy logging—just a tiny printf‑style console.

---

## Directory layout

```
minimal-balloon-tx/
├─ CMakeLists.txt
├─ cmake/
│  └─ pico_toolchain.cmake           # (optional helpers)
├─ freertos/
│  ├─ FreeRTOSConfig.h               # RTOS config (small, preemptive)
│  └─ freertos_additions.cmake
├─ boards/
│  └─ pico_wspr_horus.h              # pins, uarts, i2c, spi map
├─ include/
│  ├─ app_config.h                   # feature flags, beacon cadences
│  ├─ logging.h                      # tiny console logger macros
│  ├─ msg_bus.h                      # queues, events, message structs
│  ├─ timebase.h                     # wallclock sync + monotonic
│  ├─ telemetry.h                    # packed telemetry structs
│  └─ tasks.hpp                      # forward decls for tasks
├─ src/
│  ├─ main.c                         # rtos bring‑up, scheduler
│  ├─ timebase.c
│  ├─ msg_bus.c
│  ├─ tasks/
│  │  ├─ task_console.c              # UART console + basic shell
│  │  ├─ task_gps.c                  # read NMEA, PPS sync, fix state
│  │  ├─ task_radio.c                # SI5351 control (freq, power)
│  │  ├─ task_wspr.c                 # WSPR encoder + keying plan
│  │  ├─ task_horus.c                # Horus Binary 4FSK framing
│  │  ├─ task_camera.c               # still capture (optional)
│  │  └─ task_storage.c              # flash/SD abstraction (optional)
├─ drivers/
│  ├─ si5351/
│  │  ├─ si5351.h
│  │  └─ si5351.c                    # minimal: init, set_freq, enable
│  ├─ gps/
│  │  ├─ gps_nmea.h
│  │  └─ gps_nmea.c                  # tiny NMEA GGA/RMC parser + PPS
│  ├─ storage/
│  │  ├─ storage.h                   # LittleFS(QSPI) or FatFS(SD)
│  │  └─ storage.c
│  └─ camera/
│     ├─ camera.h
│     └─ camera.c                    # thin shim over chosen lib
├─ proto/
│  ├─ wspr/
│  │  ├─ wspr_encoder.h
│  │  └─ wspr_encoder.c              # glue around WsprEncoded lib
│  └─ horus/
│     ├─ horus_encoder.h
│     └─ horus_encoder.c            # Horus Binary v2 framing + CRC
├─ third_party/
│  └─ WsprEncoded/                   # vendor drop or submodule
└─ tools/
   └─ gen_callsign_locator.py        # helper for WSPR fields
```

---

## Bring‑up order (suggested milestones)

1) **Skeleton + RTOS**: boot banner, idle LED, console task prints tick.
2) **GPS**: parse NMEA on UART, lock PPS to FreeRTOS timebase, UTC ready.
3) **SI5351**: init over I²C, set test tone, verify on SDR.
4) **WSPR**: minimal encoder → tone scheduler → SI5351 keying (2‑FSK @ ~1.46 Hz symbol).
5) **Mode scheduler**: alternate WSPR beacon / Horus Binary frames.
6) **Horus 4FSK**: symbol timing, deviation, interleaving, test pattern → GPS‑tagged payload.
7) **(Optional) Storage**: LittleFS on QSPI flash for snapshots & logs. Avoid SD unless needed.
8) **(Optional) Camera**: one-shot still to RAM/flash, later chunk via Horus.

---

## Build system

- **Pico SDK + FreeRTOS‑Kernel** via CMake.
- FreeRTOS as object library; small heap, few queues.
- USART console, I²C for SI5351, PPS interrupt (GPIO) to sync time.

### Top‑level `CMakeLists.txt` (starter)

```cmake
cmake_minimum_required(VERSION 3.24)
project(minimal_balloon_tx C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pico SDK
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
pico_sdk_init()

# FreeRTOS
add_subdirectory(freertos-kernel)  # point to your FreeRTOS-Kernel clone

# App sources
add_executable(${PROJECT_NAME}
  src/main.c
  src/timebase.c
  src/msg_bus.c
  src/tasks/task_console.c
  src/tasks/task_gps.c
  src/tasks/task_radio.c
  src/tasks/task_wspr.c
  src/tasks/task_horus.c
  drivers/si5351/si5351.c
  drivers/gps/gps_nmea.c
  proto/wspr/wspr_encoder.c
  proto/horus/horus_encoder.c
)

# Includes
target_include_directories(${PROJECT_NAME} PRIVATE
  include
  boards
  drivers/si5351
  drivers/gps
  proto/wspr
  proto/horus
  third_party/WsprEncoded/src
)

# Pico libs
target_link_libraries(${PROJECT_NAME}
  pico_stdlib
  hardware_uart
  hardware_i2c
  hardware_gpio
  hardware_irq
  hardware_timer
  freertos_kernel
)

# USB CDC console (or uart0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})
```

---

## FreeRTOS configuration (`freertos/FreeRTOSConfig.h`)

```c
#pragma once
#include <stdint.h>

#define configUSE_PREEMPTION            1
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0
#define configCPU_CLOCK_HZ              ( 125000000UL )
#define configTICK_RATE_HZ              ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES            ( 5 )
#define configMINIMAL_STACK_SIZE        ( 256 )
#define configTOTAL_HEAP_SIZE           ( ( size_t ) ( 40 * 1024 ) )
#define configMAX_TASK_NAME_LEN         ( 16 )
#define configUSE_MUTEXES               1
#define configQUEUE_REGISTRY_SIZE       8
#define configCHECK_FOR_STACK_OVERFLOW  2
#define configUSE_MALLOC_FAILED_HOOK    1
#define configUSE_TIMERS                1
#define configTIMER_TASK_STACK_DEPTH    ( 512 )
#define configTIMER_TASK_PRIORITY       ( 3 )

/* RP2040 port specifics */
#define configSUPPORT_STATIC_ALLOCATION 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1

/* Logging */
#define configASSERT(x) if(!(x)) { taskDISABLE_INTERRUPTS(); for(;;); }
```

---

## Minimal console logger (`include/logging.h`)

```c
#pragma once
#include <stdio.h>
#define LOG_TAG(tag, fmt, ...) printf("[%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#define LOGI(fmt, ...) LOG_TAG("INFO", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) LOG_TAG("WARN", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) LOG_TAG("ERR",  fmt, ##__VA_ARGS__)
```

---

## Message bus & telemetry (`include/msg_bus.h`)

```c
#pragma once
#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"

typedef struct {
  int fix_valid; double lat, lon; float alt_m;
  uint32_t unix_time; uint8_t sats; float hdop;
} gps_fix_t;

typedef struct {
  gps_fix_t gps;
  float vbatt_v;
  int8_t temp_c;
} telemetry_t;

extern QueueHandle_t q_gps_fixes;      // gps_fix_t
extern QueueHandle_t q_tx_jobs;        // union of wspr/horus jobs
extern EventGroupHandle_t eg_system;   // bits: PPS_LOCK, GPS_LOCK

void msg_bus_init(void);
```

---

## Board mapping (`boards/pico_wspr_horus.h`)

```c
#pragma once
#define PIN_LED           25
#define UART_GPS_ID       uart1
#define UART_GPS_TX       8
#define UART_GPS_RX       9
#define UART_GPS_BAUD     9600
#define PIN_GPS_PPS       10
#define I2C_SI_ID         i2c0
#define PIN_I2C_SDA       4
#define PIN_I2C_SCL       5
```

---

## SI5351 driver slice (`drivers/si5351/si5351.h`)

```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

bool si5351_init(void);
bool si5351_set_freq(uint8_t channel, uint32_t freq_hz); // CLK0 for RF
bool si5351_enable(uint8_t channel, bool en);
```

---

## Main bring‑up (`src/main.c`)

```c
#include <stdio.h>
#include "pico/stdlib.h"
#include "FreeRTOS.h"
#include "task.h"
#include "logging.h"
#include "msg_bus.h"
#include "boards/pico_wspr_horus.h"

extern void task_console_start(void);
extern void task_gps_start(void);
extern void task_radio_start(void);
extern void task_wspr_start(void);
extern void task_horus_start(void);

int main() {
  stdio_init_all();
  sleep_ms(100);
  LOGI("minimal-balloon-tx boot");

  msg_bus_init();

  task_console_start();
  task_gps_start();
  task_radio_start();
  task_wspr_start();
  task_horus_start();

  vTaskStartScheduler();
  while (1) { }
}
```

---

## GPS task stub (`src/tasks/task_gps.c`)

```c
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
```

---

## WSPR glue idea (`proto/wspr/wspr_encoder.h`)

```c
#pragma once
#include <stdint.h>
#include "telemetry.h"

// Prepare a 162‑symbol WSPR bit/symbol stream from callsign/locator/pwr.
int wspr_build_symbols(const char* call, const char* locator4, int dbm, uint8_t *out_symbols_162);
```

**Scheduler**: a simple state machine alternates `WSPR` (on even minutes) and `HORUS` (odd minutes), triggered by PPS‑locked UTC.

---

## Horus Binary (4FSK) outline

- Implement framing + CRC per Horus Binary v2.
- Symbol mapper → 4FSK (±Δf) → radio task programs SI5351 at symbol rate.
- Start with short payload (GPS, vbatt, temp). Extend to image chunks later.

---

## Storage strategy (no SD by default)

- Use **QSPI on‑board flash** with LittleFS for temporary image chunks and logs.
- Add SD (FatFS) behind `drivers/storage/` only if images exceed flash budget.

---

## Next steps checklist

- [ ] Clone Pico SDK + FreeRTOS‑Kernel submodule
- [ ] Fill `si5351.c` (init, PLL setup, CLK0 out; start with fixed tone)
- [ ] Tiny NMEA parser for GGA/RMC; set `gps_fix_t` + PPS interrupt
- [ ] Insert WsprEncoded as `third_party/` and wrap with `wspr_encoder.c`
- [ ] Radio keying loop for WSPR symbol schedule (timing from PPS)
- [ ] Horus 4FSK framer + mapper + keying loop
- [ ] Optional: LittleFS mount + write/read smoke test
- [ ] Optional: camera shim (single still → flash)
```




