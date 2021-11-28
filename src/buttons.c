#include <string.h>
#include "buttons.h"
#include "osi/list.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_err.h"

typedef struct {
  list_t* buttons;
  uint64_t pins;
  SemaphoreHandle_t buttons_mutex;
  QueueHandle_t event_queue;
  TaskHandle_t event_task;
  uint8_t pressed;
} buttons_ctl_t;

typedef struct {
  buttons_ctl_t* ctl;
  int64_t pressed_instant;
  int64_t released_instant;
  button_config_t config;
} button_t;

void button_ctl_task(void*);

int64_t millis();

void button_isr_handler();

void invoke_callback_and_reset_button(button_t* btn, button_event_type_t event);

#define ESP_ERROR_CHECK_NOTNULL(x) do {     \
  void * __rc = (x);                        \
  if (__rc == NULL)                         \
    ESP_ERROR_CHECK(ESP_ERR_NO_MEM);        \
} while(0)

#define MUTEX_TIMEOUT 100

#define MUTEX_TAKE(ctl) while (xSemaphoreTake(ctl->buttons_mutex, (TickType_t) MUTEX_TIMEOUT) != pdTRUE);

#define MUTEX_GIVE(ctl) xSemaphoreGive(ctl->buttons_mutex);

#define SHORT_PRESS 50

#define LONG_PRESS 2000

esp_err_t buttons_ctl_init(buttons_ctl_handle_t* handle_ctl) {
  buttons_ctl_t* ctl = calloc(1, sizeof(buttons_ctl_t));
  ctl->pins = 0;
  ESP_ERROR_CHECK_NOTNULL(ctl->buttons = list_new(NULL));
  ESP_ERROR_CHECK_NOTNULL(ctl->buttons_mutex = xSemaphoreCreateMutex());
  ESP_ERROR_CHECK_NOTNULL(ctl->event_queue = xQueueCreate(5, sizeof(button_t*)));
  ESP_ERROR_CHECK(xTaskCreate(&button_ctl_task, "buttons_ctl", 2048, ctl, 5, &ctl->event_task));
  *handle_ctl = ctl;
  return ESP_OK;
}

esp_err_t buttons_ctl_deinit(buttons_ctl_handle_t handle_ctl) {
  buttons_ctl_t* ctl = handle_ctl;
  MUTEX_TAKE(ctl);
  for (list_node_t* node = list_begin(ctl->buttons); node != NULL; node = list_next(node)) {
    button_t* btn = list_node(node);
    gpio_isr_handler_remove(btn->config.gpio);
    free(btn);
  }

  list_free(ctl->buttons);
  MUTEX_GIVE(ctl);
  vSemaphoreDelete(ctl->buttons_mutex);
  vQueueDelete(ctl->event_queue);
  vTaskDelete(ctl->event_task);
  free(ctl);
  return ESP_OK;
}

esp_err_t button_register(buttons_ctl_handle_t handle_ctl, button_config_t* config, button_handle_t* handle_btn) {
  buttons_ctl_t* ctl = (buttons_ctl_t*)handle_ctl;
  uint64_t pin_mask = 1ULL << config->gpio;
  if ((ctl->pins & pin_mask) > 0)
    return ESP_ERR_INVALID_STATE;

  MUTEX_TAKE(ctl);
  ctl->pins |= pin_mask;
  button_t* btn = calloc(1, sizeof(button_t));
  ESP_ERROR_CHECK_NOTNULL(btn);

  btn->ctl = ctl;
  btn->pressed_instant = 0;
  btn->released_instant = 0;
  memcpy(&btn->config, config, sizeof(button_config_t));

  list_append(ctl->buttons, btn);
  gpio_config_t gpio = {
    .mode = GPIO_MODE_INPUT,
    .intr_type = GPIO_INTR_ANYEDGE,
    .pin_bit_mask = pin_mask,
  };

  ESP_ERROR_CHECK(gpio_config(&gpio));
  ESP_ERROR_CHECK(gpio_isr_handler_add(config->gpio, &button_isr_handler, btn));
  MUTEX_GIVE(ctl);
  *handle_btn = btn;
  return ESP_OK;
}

esp_err_t button_deregister(button_handle_t handle_btn) {
  button_t* btn = (button_t*)handle_btn;
  MUTEX_TAKE(btn->ctl);
  uint64_t pin_mask = 1ULL << btn->config.gpio;
  btn->ctl->pins &= (~pin_mask);
  ESP_ERROR_CHECK(gpio_isr_handler_remove(btn->config.gpio));
  list_remove(btn->ctl->buttons, btn);
  free(btn);
  MUTEX_GIVE(btn->ctl);
  return ESP_OK;
}

int64_t millis() {
  return esp_timer_get_time() / 1000;
}

void button_ctl_task(void* data) {
  buttons_ctl_t* ctl = (buttons_ctl_t*)data;
  for (;;) {
    button_t* btn;
    TickType_t idle = (ctl->pressed > 0 ? 10 : 1000) / portTICK_PERIOD_MS;
    BaseType_t received = xQueueReceive(ctl->event_queue, &btn, idle);
    if (received != pdTRUE)
      continue;

    int64_t now = millis();
    bool pressed = (gpio_get_level(btn->config.gpio) != false) ^ btn->config.inverted;
    if (pressed && !btn->pressed_instant) {
      btn->pressed_instant = now;
      ctl->pressed++;
    }
    else if (!pressed && btn->pressed_instant) {
      btn->released_instant = now;
    }

    MUTEX_TAKE(ctl);
    for (list_node_t* node = list_begin(ctl->buttons); node != NULL; node = list_next(node)) {
      button_t* btn = list_node(node);
      if (!btn->pressed_instant)
        continue;

      if (!btn->released_instant && now - btn->pressed_instant > LONG_PRESS) {
        invoke_callback_and_reset_button(btn, BUTTON_EVENT_LONGPRESS);
        btn->ctl->pressed--;
        continue;
      }

      if (!btn->released_instant)
        continue;

      if (btn->released_instant - btn->pressed_instant > SHORT_PRESS) {
        invoke_callback_and_reset_button(btn, BUTTON_EVENT_PRESS);
        btn->ctl->pressed--;
        continue;
      }
    }
    MUTEX_GIVE(ctl);
  }
}

void invoke_callback_and_reset_button(button_t* btn, button_event_type_t event_type) {
  button_event_t event = {
    .event = event_type,
    .gpio = btn->config.gpio,
    .callback_payload = btn->config.callback_payload
  };

  btn->pressed_instant = 0;
  btn->released_instant = 0;
  (*btn->config.callback)(&event);
}


IRAM_ATTR void button_isr_handler(void* data) {
  button_t* btn = (button_t*)data;
  xQueueGenericSendFromISR(btn->ctl->event_queue, &btn, NULL, queueSEND_TO_BACK);
}