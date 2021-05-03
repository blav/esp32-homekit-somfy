#include <esp_log.h>
#include "outlet.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "somfy.h"
#include "freertos/freertos.h"
#include "freertos/task.h"
#include "pulse.h"

static const char* TAG = "Outlet";

#define SOMFY_GPIO GPIO_NUM_2

static pulse_ctl_handle_t ctl;

void outlet_init() {
  pulse_ctl_config_t cfg = {
    .gpio = SOMFY_GPIO,
    .timer_group = TIMER_GROUP_0,
    .timer_idx = TIMER_0,
    .max_queue_size = 3
  };

  ctl = pulse_ctl_new (&cfg); 
}

void outlet_set_state(bool state) {
  somfy_command_t command = {
    .button = state ? BUTTON_UP : BUTTON_DOWN,
    .remote = 0x10000,
    .rolling_code = 15
  };

  somfy_command_send(ctl, &command);
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