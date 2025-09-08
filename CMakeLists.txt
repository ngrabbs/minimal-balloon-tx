cmake_minimum_required(VERSION 3.13)

# If you want to vendor the SDK with a local path, keep this:
set(PICO_SDK_PATH "/Users/nick/Documents/main_backup/MSU_CubeSat/pico-sdk")
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# Project
project(minimal_balloon_tx C CXX ASM)

# --- Pico SDK ---
# Force RP2040 so nothing tries to hunt for RP2350 ports.
set(PICO_PLATFORM rp2040)
set(PICO_BOARD pico)

pico_sdk_init()

# ---- FreeRTOS config shim (required by FreeRTOS-Kernel CMake) ----
# Point this to the folder that contains your FreeRTOSConfig.h
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/freertos   # <--- adjust if your config lives elsewhere
)
target_compile_definitions(freertos_config INTERFACE
  projCOVERAGE_TEST=0
  configUSE_NEWLIB_REENTRANT=1           # printf from multiple tasks
)

# Select port & heap BEFORE adding the kernel
set(FREERTOS_PORT GCC_RP2040)
set(FREERTOS_HEAP 4)                     # use heap_4.c (good default)

# Bring kernel from your submodule at lib/FreeRTOS-Kernel
add_subdirectory(lib/FreeRTOS-Kernel)

# --- App sources ---
add_executable(${PROJECT_NAME}
  src/main.c
  src/timebase.c
  src/msg_bus.c
  src/tasks/task_console.c
  src/tasks/task_gps.c
  src/tasks/task_radio_arbiter.c
  src/tasks/task_wsched.c
  src/tasks/task_hsched.c
#  src/tasks/task_radio.c
  src/tasks/task_wspr.c
#  src/tasks/task_horus.c
#  drivers/si5351/si5351.c
#  drivers/gps/gps_nmea.c
  drivers/gps/gps_hw.c
  proto/wspr/wspr_encoder.c
#  proto/horus/horus_encoder.c
)

# --- Includes ---
target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/boards
  ${CMAKE_CURRENT_LIST_DIR}/drivers/si5351
  ${CMAKE_CURRENT_LIST_DIR}/drivers/gps
  ${CMAKE_CURRENT_LIST_DIR}/proto/wspr
  ${CMAKE_CURRENT_LIST_DIR}/proto/horus
  ${CMAKE_CURRENT_LIST_DIR}/third_party/WsprEncoded/src
  ${CMAKE_CURRENT_LIST_DIR}/freetros        # where your FreeRTOSConfig.h lives (adjust if different)
)

# --- Link ---
target_link_libraries(${PROJECT_NAME}
  pico_stdlib
  hardware_uart
  hardware_i2c
  hardware_gpio
  hardware_irq
  hardware_timer
  freertos_kernel
  m
)

# Console
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})