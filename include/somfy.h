#ifndef __somfy_h
#define __somfy_h

#include <stdint.h>
#include <driver/gpio.h>
#include "pulse.h"

#include "somfy_config.h"
#include "somfy_config_nvs.h"
#include "somfy_config_http.h"

typedef void * somfy_ctl_handle_t;

esp_err_t somfy_ctl_init (somfy_config_handle_t config, pulse_ctl_config_t * pulse_cfg, somfy_ctl_handle_t * ctl);

esp_err_t somfy_ctl_free (somfy_ctl_handle_t ctl);

esp_err_t somfy_ctl_send_command(somfy_ctl_handle_t ctl, somfy_command_t *command);


#endif //__somfy_h
