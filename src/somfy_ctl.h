#ifndef __somfy_ctl_h
#define __somfy_ctl_h

#include "somfy.h"

typedef struct {
  pulse_train_handle_t pulse_ctl;
  somfy_config_handle_t config;
} somfy_ctl_t;

esp_err_t somfy_ctl_increment_rolling_code_and_write_nvs (somfy_ctl_handle_t cfg, somfy_remote_t remote, somfy_rolling_code_t * rolling_code);


#endif//__somfy_ctl_h