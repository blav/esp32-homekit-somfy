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
  somfy_rolling_code_t rolling_code;
  somfy_button_t button;
} somfy_command_t;

void somfy_command_send (pulse_ctl_handle_t ctl, somfy_command_t* command);

#endif //__somfy_h
