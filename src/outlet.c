#include <esp_log.h>
#include "esp_err.h"
#include "outlet.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "somfy.h"
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "pulse.h"
#include "buttons.h"

static const char* TAG = "Outlet";

#define SOMFY_GPIO GPIO_NUM_12
#define BUTTON_GPIO GPIO_NUM_14

static pulse_ctl_handle_t ctl;
static buttons_ctl_handle_t buttons_ctl;

void button_pressed (button_event_t* event);

void outlet_init() {
  pulse_ctl_config_t cfg = {
    .gpio = SOMFY_GPIO,
    .timer_group = TIMER_GROUP_0,
    .timer_idx = TIMER_0,
    .max_queue_size = 3
  };

  ctl = pulse_ctl_new (&cfg); 

  somfy_remote_config_t remote2 = {
    .remote = 0x123457,
    .remote_name = "name 2",
    .rolling_code = 43,
    .next = NULL,
  };

  somfy_remote_config_t remote1 = {
    .remote = 0x123456,
    .remote_name = "name 1",
    .rolling_code = 42,
    .next = &remote2,
  };

  somfy_config_t config = {
    .remotes = &remote1,
  };

  somfy_config_save(&config, "http://blav.ngrok.io/config");

/*
  gpio_install_isr_service(0);

  button_handle_t button_handle;
  button_config_t button_config = {
    .gpio = BUTTON_GPIO,
    .inverted = false,
    .callback = &button_pressed,
  };

  ESP_ERROR_CHECK(buttons_ctl_init(&buttons_ctl));
  button_register(buttons_ctl, &button_config, &button_handle);
  */
}

void outlet_set_state(bool state) {
  somfy_command_t command = {
    .button = state ? BUTTON_UP : BUTTON_DOWN,
    .remote = 0x100000,
  };

  somfy_command_send(ctl, &command);
}

void button_pressed (button_event_t* event) {
  ESP_LOGI(TAG, "button %d pressed %d", event->gpio, event->event);
}

/*
void outlet_init() {
  ESP_LOGI(TAG, "Somfy GPIO init complete");
  somfy_gpio_init (SOMFY_GPIO);
}

void outlet_set_state(bool state) {
  ESP_LOGI(TAG, "Received Write. Outlet %s", state ? "On" : "Off");
  somfy_command_t command = {
    .rolling_code = 20,
    .remote = 0x10000,
    .button = BUTTON_UP
  };

  somfy_command_send (&command, SOMFY_GPIO);
  ESP_LOGI(TAG, "Somfy command sent.");
}
*/