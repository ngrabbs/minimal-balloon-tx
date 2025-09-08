// src/tasks/task_radio_arbiter.c
#include "radio_arbiter.h"
#include "timebase.h"
#include "logging.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define RADIO_Q_LEN 8

typedef struct {
  radio_req_t req;
} q_item_t;

static QueueHandle_t q_reqs;
static SemaphoreHandle_t m_radio;

static void si5351_init_once(void){ /* TODO */ }
static void si5351_stop_all(void){ /* TODO */ }

static int cmp_req(const radio_req_t *a, const radio_req_t *b){
  if (a->t_start_ms < b->t_start_ms) return -1;
  if (a->t_start_ms > b->t_start_ms) return 1;
  // Same start: higher priority first
  if (a->priority > b->priority) return -1;
  if (a->priority < b->priority) return 1;
  return 0;
}

// naive small-array scheduler
#define MAX_CAL 6
static radio_req_t cal[MAX_CAL];
static int cal_n = 0;

static void cal_insert(const radio_req_t *r){
  if (cal_n >= MAX_CAL) return;
  int i=cal_n++;
  cal[i]=*r;
  // insertion sort
  for (int j=i; j>0 && cmp_req(&cal[j], &cal[j-1])<0; --j){
    radio_req_t t = cal[j]; cal[j]=cal[j-1]; cal[j-1]=t;
  }
}

static bool overlaps(const radio_req_t *a, const radio_req_t *b){
  uint64_t a_end = a->t_start_ms + a->duration_ms;
  uint64_t b_end = b->t_start_ms + b->duration_ms;
  return !(a_end <= b->t_start_ms || b_end <= a->t_start_ms);
}

bool radio_arbiter_submit(const radio_req_t *req){
  if (!q_reqs) return false;
  q_item_t it = { .req = *req };
  return xQueueSend(q_reqs, &it, 0) == pdPASS;
}

static void radio_task(void *arg){
  (void)arg;
  si5351_init_once();
  si5351_stop_all();

  for(;;){
    // 1) drain queue
    q_item_t it;
    while (xQueueReceive(q_reqs, &it, 0) == pdPASS){
      // check overlap policy vs calendar
      bool ok = true;
      for (int i=0;i<cal_n;i++){
        if (overlaps(&it.req, &cal[i])){
          // priority rule
          if (it.req.priority > cal[i].priority){
            // preempt existing (simple: drop the old one)
            // In production, you might signal the old client it's been rejected.
            for (int k=i;k<cal_n-1;k++) cal[k]=cal[k+1];
            cal_n--;
          } else {
            ok = false; break;
          }
        }
      }
      if (ok) cal_insert(&it.req);
    }

    // 2) wake for the next event
    if (cal_n == 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    radio_req_t *r = &cal[0];
    int32_t ms_until = (int32_t)((int64_t)r->t_start_ms - (int64_t)timebase_now_ms());
    if (ms_until > 0){
      int chunk = ms_until > 50 ? 50 : ms_until; // fine-grain wait
      vTaskDelay(pdMS_TO_TICKS(chunk));
      continue;
    }

    // 3) execute current window
    // lock hardware
    if (m_radio && xSemaphoreTake(m_radio, pdMS_TO_TICKS(500)) == pdTRUE){
      if (r->start_cb) r->start_cb(r->user);

      uint32_t dur = r->duration_ms;
      while ((int32_t)dur > 0){
        uint32_t sl = dur > 20 ? 20 : dur;
        vTaskDelay(pdMS_TO_TICKS(sl));
        dur -= sl;
      }

      if (r->stop_cb) r->stop_cb(r->user);
      si5351_stop_all();
      xSemaphoreGive(m_radio);
    } else {
      LOGE("radio: failed to lock HW");
    }

    // 4) remove it
    for (int k=0;k<cal_n-1;k++) cal[k]=cal[k+1];
    cal_n--;
  }
}

void task_radio_arbiter_start(void){
  q_reqs = xQueueCreate(RADIO_Q_LEN, sizeof(q_item_t));
  m_radio = xSemaphoreCreateMutex();
  xTaskCreate(radio_task, "radioarb", 1536, NULL, tskIDLE_PRIORITY+3, NULL);
}