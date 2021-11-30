#include "freertos/FreeRTOS.h"
#include "somfy.h"
#include "somfy_ctl.h"
#include "nvs.h"
#include "pulse.h"

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
