#ifndef __somfy_config_h
#define __somfy_config_h

#include <stdint.h>
#include <esp_err.h>


typedef uint32_t somfy_remote_t;

typedef uint16_t somfy_rolling_code_t;

typedef enum {
  BUTTON_STOP = 1,
  BUTTON_UP = 2,
  BUTTON_DOWN = 4,
  BUTTON_PROG = 8
} somfy_button_t;

typedef struct  {
  char *remote_name;
  somfy_remote_t remote;
  somfy_rolling_code_t rolling_code;
} somfy_config_remote_t;


typedef void * somfy_config_handle_t;

typedef void * somfy_config_remote_handle_t;

typedef void * somfy_config_blob_handle_t;

esp_err_t somfy_config_new(somfy_config_handle_t * handle);

esp_err_t somfy_config_new_from_nvs (somfy_config_handle_t * handle);

esp_err_t somfy_config_free(somfy_config_handle_t handle);

esp_err_t somfy_config_remote_new(const char * remote_name, somfy_remote_t remote, somfy_rolling_code_t code, somfy_config_remote_handle_t * handle);

esp_err_t somfy_config_remote_get(somfy_config_remote_handle_t handle, char ** remote_name, somfy_remote_t * remote, somfy_rolling_code_t * code);

esp_err_t somfy_config_remote_free (somfy_config_remote_handle_t handle);

esp_err_t somfy_config_add_remote(somfy_config_handle_t cfg, somfy_config_remote_handle_t remote_cfg);

esp_err_t somfy_config_increment_rolling_code (somfy_config_handle_t cfg, somfy_remote_t remote, somfy_rolling_code_t * rolling_code);

esp_err_t somfy_config_serialize (somfy_config_handle_t cfg, somfy_config_blob_handle_t * blob);

esp_err_t somfy_config_deserialize (somfy_config_blob_handle_t blob, somfy_config_handle_t * cfg);

esp_err_t somfy_config_blob_free (somfy_config_blob_handle_t blob);

typedef void (*somfy_config_remote_cb_t)(somfy_config_remote_handle_t, void *);

void somfy_config_remote_for_each (somfy_config_handle_t handle, somfy_config_remote_cb_t callback, void * data);

#endif//__somfy_config_h