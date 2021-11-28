#ifndef __somfy_h
#define __somfy_h

#include <stdint.h>
#include "pulse.h"

typedef uint32_t somfy_remote_t;

typedef uint16_t somfy_rolling_code_t;

typedef enum {
  BUTTON_STOP = 1,
  BUTTON_UP = 2,
  BUTTON_DOWN = 4,
  BUTTON_PROG = 8
} somfy_button_t;

typedef struct {
  somfy_remote_t remote;
  somfy_button_t button;
} somfy_command_t;

typedef struct somfy_remote_config_t somfy_remote_config_t;
struct somfy_remote_config_t {
  char *remote_name;
  somfy_remote_t remote;
  somfy_rolling_code_t rolling_code;
  struct somfy_remote_config_t *next;
};

typedef struct {
  somfy_remote_config_t *remotes;
} somfy_config_t;

void somfy_command_send(pulse_ctl_handle_t ctl, somfy_command_t *command);

esp_err_t somfy_config_get(somfy_config_t *config);

esp_err_t somfy_config_set(somfy_config_t *config);

esp_err_t somfy_config_save(somfy_config_t *config, char *url);

esp_err_t somfy_config_load(somfy_config_t *config, char *url);

#endif //__somfy_h
