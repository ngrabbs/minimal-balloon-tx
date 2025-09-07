cmake_minimum_required(VERSION 3.24)
project(minimal_balloon_tx C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pico SDK
set(PICO_SDK_PATH "/Users/nick/Documents/main_backup/MSU_CubeSat/pico-sdk")
set(PICO_PLATFORM "rp2040")
set(PICO_BOARD "pico")
#set(PICO_PLATFORM "rp2350")
#set(PICO_BOARD "pico2")
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
