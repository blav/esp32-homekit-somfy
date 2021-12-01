#include <esp_log.h>
#include "somfy.h"
#include "somfy_ctl.h"

static const char * TAG = "somfy_frame";

typedef struct {
  uint8_t frame[7];
  somfy_ctl_handle_t ctl;
} somfy_frame_t;

typedef uint8_t somfy_sync_t;

void somfy_frame_init(somfy_frame_t* frame, somfy_ctl_handle_t ctl, somfy_command_t* command);

void somfy_frame_write(somfy_frame_t* frame, pulse_train_handle_t train, somfy_sync_t sync);

void somfy_frame_debug(somfy_frame_t* frame, somfy_command_t * command, somfy_rolling_code_t code);

esp_err_t somfy_ctl_send_frames (somfy_ctl_handle_t handle, somfy_command_t* command) {
  somfy_ctl_t * c = (somfy_ctl_t *) handle;
  pulse_ctl_handle_t ctl = c->pulse_ctl;
  pulse_train_handle_t train;
  pulse_train_init(ctl, &train);

  somfy_frame_t frame;
  somfy_frame_init(&frame, handle, command);
  somfy_frame_write(&frame, train, 2);
  somfy_frame_write(&frame, train, 7);
  somfy_frame_write(&frame, train, 7);

  pulse_train_send(train);
  ESP_LOGI(TAG, "Frame sent!");
  return ESP_OK;
}

void somfy_frame_init(somfy_frame_t* frame, somfy_ctl_handle_t ctl, somfy_command_t* command) {
  frame->ctl = ctl;
  frame->frame[0] = 0xA7;
  frame->frame[1] = command->button << 4;
  frame->frame[2] = command->rolling_code >> 8;
  frame->frame[3] = command->rolling_code;
  frame->frame[4] = command->remote >> 16;
  frame->frame[5] = command->remote >> 8;
  frame->frame[6] = command->remote;

  uint8_t checksum = 0;
  for (int i = 0; i < 7; i++)
    checksum = checksum ^ frame->frame[i] ^ (frame->frame[i] >> 4);

  frame->frame[1] |= (checksum & 0xf);
  for (int i = 1; i < 7; i++)
    frame->frame[i] ^= frame->frame[i - 1];

  somfy_frame_debug(frame, command, command->rolling_code);
}

#define SYMBOL 640

void somfy_frame_write(somfy_frame_t* frame, pulse_train_handle_t train, somfy_sync_t sync) {
  // Only with the first frame.
  if (sync == 2) { 
    // Wake-up pulse & Silence
    pulse_train_add_pulse(train, 9415, PULSE_HIGH);
    pulse_train_add_pulse(train, 89565, PULSE_LOW);
  }

  // Hardware sync: two sync for the first frame, seven for the following ones.
  for (int i = 0; i < sync; i++) {
    pulse_train_add_pulse(train, 4 * SYMBOL, PULSE_HIGH);
    pulse_train_add_pulse(train, 4 * SYMBOL, PULSE_LOW);
  }

  // Software sync
  pulse_train_add_pulse(train, 4550, PULSE_HIGH);
  pulse_train_add_pulse(train, SYMBOL, PULSE_LOW);

  // Data: bits are sent one by one, starting with the MSB.
  for (uint8_t i = 0; i < 56; i++) {
    if (((frame->frame[i / 8] >> (7 - (i % 8))) & 1) == 1) {
      pulse_train_add_pulse(train, SYMBOL, PULSE_LOW);
      pulse_train_add_pulse(train, SYMBOL, PULSE_HIGH);
    }
    else {
      pulse_train_add_pulse(train, SYMBOL, PULSE_HIGH);
      pulse_train_add_pulse(train, SYMBOL, PULSE_LOW);
    }
  }

  pulse_train_add_pulse(train, 30415, PULSE_LOW);
}

void somfy_frame_debug(somfy_frame_t * frame, somfy_command_t * command, somfy_rolling_code_t code) {
  ESP_LOGI(TAG, "Built frame %04x%04x%04x%02x (remote = %06x, button = %d, code = %d)",
    frame->frame[0] << 8 | frame->frame [1], 
    frame->frame[2] << 8 | frame->frame [3], 
    frame->frame[4] << 8 | frame->frame [5], 
    frame->frame[6] << 8,
    command->remote & 0xffffff,
    command->button,
    code);
}