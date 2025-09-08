// include/radio_arbiter.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum { MODE_WSPR=1, MODE_HORUS=0 } radio_mode_t;

typedef struct {
  radio_mode_t mode;
  uint64_t     t_start_ms;
  uint32_t     duration_ms;
  uint32_t     freq_hz;
  void       (*start_cb)(void*);
  void       (*stop_cb)(void*);
  void        *user;
  uint8_t      priority; // higher wins
} radio_req_t;

bool radio_arbiter_submit(const radio_req_t *req);
void task_radio_arbiter_start(void);