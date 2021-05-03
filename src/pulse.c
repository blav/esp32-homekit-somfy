#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "osi/list.h"
#include "soc/rtc.h"
#include "pulse.h"

static const char* DRAM_ATTR TAG = "pulse_ctl";

static bool IRAM_ATTR pulse_ctl_alarm_handler(void* args);

struct pulse_train_t;

typedef struct {
  QueueHandle_t work_queue;
  QueueHandle_t control_queue;
  TaskHandle_t task;
  pulse_ctl_config_t config;
  struct pulse_train_t* current;
} pulse_ctl_t;

typedef enum {
  MESSAGE_TRAIN_DONE,
  MESSAGE_KILL,
} message_type_t;

typedef struct pulse_train_t {
  pulse_ctl_t* ctl;
  list_t* pulses;
  list_node_t* current;
} pulse_train_t;

void pulse_ctl_task(void*);

void pulse_ctl_kill(pulse_ctl_t* ctl);

void timer_pulse_init(pulse_ctl_t* ctl);

inline void pulse_decode(int64_t pulse, pulse_duration_t* duration, pulse_level_t* level) {
  *level = pulse >= 0 ? PULSE_HIGH : PULSE_LOW;
  *duration = abs(pulse);
}

inline esp_err_t pulse_encode(pulse_duration_t duration, pulse_level_t level, int64_t* pulse) {
  if (duration >> 63 == 1)
    return ESP_ERR_INVALID_ARG;

  *pulse = ((int64_t)duration) * (level == PULSE_LOW ? -1LL : 1LL);
  return ESP_OK;
}

pulse_ctl_handle_t pulse_ctl_new(pulse_ctl_config_t* cfg) {
  pulse_ctl_t* handle = calloc(1, sizeof(pulse_ctl_t));
  handle->current = NULL;
  handle->work_queue = xQueueCreate(cfg->max_queue_size, sizeof(pulse_train_t));
  handle->control_queue = xQueueCreate(2, sizeof(message_type_t));
  memcpy(&handle->config, cfg, sizeof(pulse_ctl_config_t));
  xTaskCreate(&pulse_ctl_task, "pulse_ctl_task", 2048, handle, 5, &handle->task);
  return handle;
}

esp_err_t pulse_train_init(pulse_ctl_handle_t ctl_handle, pulse_train_handle_t* train_handle) {
  pulse_train_t* train = calloc(1, sizeof(pulse_train_t));
  *train_handle = train;

  train->ctl = (pulse_ctl_t*)ctl_handle;
  train->current = NULL;
  train->pulses = list_new(&free);

  return ESP_OK;
}

void pulse_train_free(pulse_train_t* train) {
  list_free(train->pulses);
  free(train);
}

esp_err_t pulse_train_add_pulse(pulse_train_handle_t handle, pulse_duration_t duration, pulse_level_t pulse) {
  pulse_train_t* message = handle;
  int64_t* value = calloc(1, sizeof(int64_t));
  esp_err_t result = pulse_encode(duration, pulse, value);
  if (result != ESP_OK)
    return result;

  list_append(message->pulses, value);
  return ESP_OK;
}

esp_err_t pulse_train_send(pulse_train_handle_t handle) {
  pulse_train_t* train = handle;
  if (list_is_empty(train->pulses))
    return ESP_ERR_INVALID_ARG;

  BaseType_t result = xQueueGenericSend(
    train->ctl->work_queue, &train, 1000 / portTICK_PERIOD_MS, queueSEND_TO_BACK);

  if (result == pdTRUE)
    return ESP_OK;

  return ESP_ERR_TIMEOUT;
}

esp_err_t pulse_ctl_free(pulse_ctl_handle_t handle) {
  pulse_ctl_t* ctl = handle;
  message_type_t kill = MESSAGE_KILL;
  return xQueueGenericSend(ctl->control_queue, &kill, 1000 / portTICK_PERIOD_MS, queueSEND_TO_FRONT);
}

void pulse_ctl_task(void* data) {
  pulse_ctl_t* ctl = data;
  pulse_ctl_config_t* cfg = &ctl->config;
  gpio_config_t gpio = {
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = BIT(cfg->gpio),
    .intr_type = GPIO_INTR_DISABLE,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_ENABLE
  };

  gpio_config(&gpio);

  uint32_t divider = rtc_clk_apb_freq_get () / 1000000;
  ESP_LOGI(TAG, "Initializing timer with divider %d.", divider);
  timer_config_t timer = {
    .divider = divider,
    .alarm_en = TIMER_ALARM_DIS,
    .auto_reload = TIMER_AUTORELOAD_EN,
    .counter_dir = TIMER_COUNT_UP,
    .counter_en = TIMER_PAUSE
  };

  timer_init(cfg->timer_group, cfg->timer_idx, &timer);
  timer_isr_callback_add(cfg->timer_group, cfg->timer_idx, pulse_ctl_alarm_handler, ctl, 0);
  timer_set_alarm_value(cfg->timer_group, cfg->timer_idx, 500 * 1000);

  ESP_LOGI(TAG, "Pulse Controller Task started. GPIO = %d, Timer group= %d, Timer = %d",
    cfg->gpio, cfg->timer_group, cfg->timer_idx);

  int wait = 0;
  while (1) {
    message_type_t control;
    BaseType_t result = xQueueReceive(ctl->control_queue, &control, wait / portTICK_PERIOD_MS);
    if (result == pdTRUE) {
      if (control == MESSAGE_KILL) {
        ESP_LOGI(TAG, "Killing controller.");
        pulse_ctl_kill(ctl);
        return;
      }
      else if (control == MESSAGE_TRAIN_DONE) {
        ESP_LOGI(TAG, "Pulse train completed.");
        pulse_train_free(ctl->current);
        ctl->current = NULL;
      }
      else {
        ESP_LOGE(TAG, "Unknown control message %d.", control);
      }
    }

    if (ctl->current != NULL) {
      wait = 1000;
      continue;
    }
    else {
      wait = 0;
    }

    pulse_train_t* train;
    result = xQueueReceive(ctl->work_queue, &train, 1000 / portTICK_PERIOD_MS);
    if (result != pdTRUE)
      continue;

    ESP_LOGI(TAG, "Pulse train received (%d pulses).", list_length(train->pulses));
    ctl->current = train;
    timer_pulse_init(ctl);
  }
}

void pulse_ctl_kill(pulse_ctl_t* ctl) {
  ESP_LOGI(TAG, "Pulse Controller Task killed.");
  vQueueDelete(ctl->work_queue);
  vQueueDelete(ctl->control_queue);
  vTaskDelete(ctl->task);
  free(ctl);
}

void timer_pulse_init(pulse_ctl_t* ctl) {
  pulse_train_t* train = ctl->current;
  pulse_ctl_config_t* cfg = &ctl->config;
  train->current = list_begin(train->pulses);
  int64_t pulse = *(int64_t*)list_node(train->current);
  pulse_duration_t alarm;
  pulse_level_t level;
  pulse_decode(pulse, &alarm, &level);
  timer_set_alarm_value(cfg->timer_group, cfg->timer_idx, alarm);
  timer_set_alarm(cfg->timer_group, cfg->timer_idx, TIMER_ALARM_EN);
  gpio_set_level(cfg->gpio, level);
  timer_start(cfg->timer_group, cfg->timer_idx);
}

static bool IRAM_ATTR pulse_ctl_alarm_handler(void* args) {
  pulse_ctl_t* ctl = (pulse_ctl_t*)args;
  pulse_train_t* train = ctl->current;
  pulse_ctl_config_t* cfg = &ctl->config;
  train->current = list_next(train->current);
  if (train->current == NULL) {
    gpio_set_level(cfg->gpio, PULSE_LOW);
    timer_group_set_counter_enable_in_isr(cfg->timer_group, cfg->timer_idx, TIMER_PAUSE);
    BaseType_t priority;
    message_type_t done = MESSAGE_TRAIN_DONE;
    xQueueGenericSendFromISR(ctl->control_queue, &done, &priority, queueSEND_TO_FRONT);
    return true;
  }

  int64_t pulse = *(int64_t*)list_node(train->current);
  pulse_duration_t alarm;
  pulse_level_t level;
  pulse_decode(pulse, &alarm, &level);
  timer_group_set_alarm_value_in_isr(cfg->timer_group, cfg->timer_idx, alarm);
  gpio_set_level(cfg->gpio, level);
  return true;
}
