#ifndef __somfy_config_http
#define __somfy_config_http

#include "somfy_config.h"

esp_err_t somfy_config_blob_http_write(somfy_config_blob_handle_t config, char *url);

esp_err_t somfy_config_blob_http_read(somfy_config_blob_handle_t *config, char *url);

#endif//__somfy_config_http