#ifndef __pulse_h
#define __pulse_h

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/timer.h"

typedef enum {
  PULSE_LOW = 0,
  PULSE_HIGH = 1
} pulse_level_t;

typedef uint64_t pulse_duration_t;

typedef struct {
  timer_group_t timer_group;
  timer_idx_t timer_idx;
  gpio_num_t gpio;
  uint8_t max_queue_size;
} pulse_ctl_config_t;

typedef void * pulse_ctl_handle_t;

typedef void * pulse_train_handle_t;

pulse_ctl_handle_t pulse_ctl_new (pulse_ctl_config_t * cfg);

esp_err_t pulse_ctl_free (pulse_ctl_handle_t handle);

esp_err_t pulse_train_init (pulse_ctl_handle_t handle, pulse_train_handle_t * message);

esp_err_t pulse_train_add_pulse (pulse_train_handle_t handle, pulse_duration_t duration, pulse_level_t pulse);

esp_err_t pulse_train_send (pulse_train_handle_t handle);

#endif //__pulse_h