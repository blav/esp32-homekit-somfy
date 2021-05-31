#ifndef __buttons_h
#define __buttons_h

#include <stdint.h>
#include "driver/gpio.h"

typedef enum {
  BUTTON_EVENT_PRESS,
  BUTTON_EVENT_LONGPRESS
} button_event_type_t;

typedef struct {
  button_event_type_t event;
  gpio_num_t gpio;
  void* callback_payload;
} button_event_t;

typedef void (*button_callback_t) (button_event_t* event);

typedef struct {
  gpio_num_t gpio;
  bool inverted;
  button_callback_t callback;
  void* callback_payload;
} button_config_t;


typedef void* button_handle_t;

typedef void* buttons_ctl_handle_t;

esp_err_t buttons_ctl_init(buttons_ctl_handle_t* ctl);

esp_err_t buttons_ctl_deinit(buttons_ctl_handle_t ctl);

esp_err_t button_register(buttons_ctl_handle_t ctl, button_config_t* config, button_handle_t* button);

esp_err_t button_deregister(button_handle_t button);

#endif//__buttons_h