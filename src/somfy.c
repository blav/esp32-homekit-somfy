#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "somfy.h"
#include "nvs.h"
#include "pulse.h"

const char* TAG = "somfy";

typedef struct {
  uint8_t frame[7];
  somfy_ctl_handle_t ctl;
} somfy_frame_t;

typedef struct {
  pulse_train_handle_t pulse_ctl;
  somfy_config_handle_t config;
} somfy_ctl_t;

esp_err_t somfy_ctl_init (somfy_config_handle_t ctl_cfg, pulse_ctl_config_t * pulse_cfg, somfy_ctl_handle_t * handle) {
  somfy_ctl_t * ctl = calloc(1, sizeof(somfy_ctl_t));
  ctl->pulse_ctl = pulse_ctl_new (pulse_cfg);
  ctl->config = ctl_cfg;
  *handle = ctl;
  return ESP_OK;
}

esp_err_t somfy_ctl_free (somfy_ctl_handle_t handle) {
  somfy_ctl_t * ctl = (somfy_ctl_t *) handle;
  pulse_ctl_free(ctl->pulse_ctl);
  free(ctl);
  return ESP_OK;
}


typedef uint8_t somfy_sync_t;

void somfy_frame_init(somfy_frame_t* frame, somfy_ctl_handle_t ctl, somfy_command_t* command);

void somfy_frame_write(somfy_frame_t* frame, pulse_train_handle_t train, somfy_sync_t sync);

void somfy_frame_debug(somfy_frame_t* frame, somfy_command_t * command, somfy_rolling_code_t code);

void somfy_remote_rolling_code_get_and_inc (somfy_remote_t remote, somfy_rolling_code_t * code);

esp_err_t somfy_ctl_increment_rolling_code_and_write_nvs (somfy_ctl_handle_t cfg, somfy_remote_t remote, somfy_rolling_code_t * rolling_code);

esp_err_t somfy_ctl_send_command (somfy_ctl_handle_t handle, somfy_command_t* command) {
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
  somfy_rolling_code_t rolling_code;
  //somfy_remote_rolling_code_get_and_inc(command->remote, &rolling_code);

  somfy_ctl_increment_rolling_code_and_write_nvs(ctl, command->remote, &rolling_code);

  frame->ctl = ctl;
  frame->frame[0] = 0xA7;
  frame->frame[1] = command->button << 4;
  frame->frame[2] = rolling_code >> 8;
  frame->frame[3] = rolling_code;
  frame->frame[4] = command->remote >> 16;
  frame->frame[5] = command->remote >> 8;
  frame->frame[6] = command->remote;

  uint8_t checksum = 0;
  for (int i = 0; i < 7; i++)
    checksum = checksum ^ frame->frame[i] ^ (frame->frame[i] >> 4);

  frame->frame[1] |= (checksum & 0xf);
  for (int i = 1; i < 7; i++)
    frame->frame[i] ^= frame->frame[i - 1];

  somfy_frame_debug(frame, command, rolling_code);
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

void somfy_remote_rolling_code_get_and_inc (somfy_remote_t remote, somfy_rolling_code_t * code) {
  char remote_key[20];
  sprintf(remote_key, "remote-%06x", remote & 0xffffff);
  nvs_handle_t handle;
  ESP_ERROR_CHECK (nvs_open("somfy", NVS_READWRITE, &handle));
  esp_err_t read = nvs_get_u16(handle, remote_key, code);
  if (read == ESP_ERR_NVS_NOT_FOUND) {
    *code = 0;
  } else if (read != ESP_OK) {
    ESP_LOGE(TAG, "%s", esp_err_to_name(read));
    abort();
  }

  ESP_LOGI(TAG, "increment rolling code %s %d", remote_key, *code);

//  if (*code < 106)
//    *code = 106;

  ESP_ERROR_CHECK (nvs_set_u16(handle, remote_key, (*code) + 1));
  nvs_close(handle);
  ESP_LOGI(TAG, "Wrote rolling code %d for remote %s.", *code, remote_key);
}


esp_err_t somfy_ctl_increment_rolling_code_and_write_nvs (somfy_ctl_handle_t handle, somfy_remote_t remote, somfy_rolling_code_t * rolling_code) {
    somfy_ctl_t * ctl = (somfy_ctl_t *) handle;
    somfy_config_increment_rolling_code(ctl->config, remote, rolling_code);

    somfy_config_blob_handle_t blob;
    somfy_config_serialize (ctl->config, &blob);
    somfy_config_blob_nvs_write(blob);
    somfy_config_blob_http_write(blob, "http://blav.ngrok.io/config");
    somfy_config_blob_free(blob);
    return ESP_OK;
}
