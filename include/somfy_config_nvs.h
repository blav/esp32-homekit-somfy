#ifndef __somfy_config_nvs
#define __somfy_config_nvs

#include "somfy_config.h"

esp_err_t somfy_config_blob_nvs_write (somfy_config_blob_handle_t blob);

esp_err_t somfy_config_blob_nvs_read (somfy_config_blob_handle_t * blob);

#endif//__somfy_config_nvs