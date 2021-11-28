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

static const char* TAG = "outlet";

#define SOMFY_GPIO GPIO_NUM_12
#define BUTTON_GPIO GPIO_NUM_14

static somfy_ctl_handle_t ctl;
static buttons_ctl_handle_t buttons_ctl;
static somfy_config_handle_t config;

void button_pressed (button_event_t* event);

void outlet_init() {
  
  somfy_config_blob_handle_t blob;
  if (somfy_config_blob_nvs_read(&blob) == ESP_OK) {
    ESP_LOGI(TAG, "Found somfy config.");
    somfy_config_deserialize(blob, &config);
  } else {
    ESP_LOGI(TAG, "No somfy config found. Creating a new one.");
    somfy_config_new(&config);
    somfy_config_remote_t * remote1;
    somfy_config_remote_new("Bureau", 0x100000, 116, &remote1);

    somfy_config_add_remote (config, remote1);
    somfy_config_serialize (config, &blob);
    somfy_config_blob_nvs_write (blob);
  }

  ESP_LOGI(TAG, "config %x loaded", (unsigned int) config);

  somfy_config_blob_http_write (blob, "http://blav.ngrok.io/config");
  somfy_config_blob_free (blob);
  ESP_LOGI(TAG, "config sent");

  pulse_ctl_config_t pulse_cfg = {
    .gpio = SOMFY_GPIO,
    .timer_group = TIMER_GROUP_0,
    .timer_idx = TIMER_0,
    .max_queue_size = 3
  };

  somfy_ctl_init (config, &pulse_cfg, &ctl); 


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

  somfy_config_blob_handle_t blob;
  somfy_config_serialize(config,&blob);
  somfy_config_blob_http_write(blob, "http://blav.ngrok.io/config");
  somfy_config_blob_free(blob);
  somfy_ctl_send_command(ctl, &command);
  
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

  somfy_ctl_send_command (&command, SOMFY_GPIO);
  ESP_LOGI(TAG, "Somfy command sent.");
}
*/