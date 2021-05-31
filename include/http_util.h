#ifndef __http_util_h
#define __http_util_h

#include "esp_http_server.h"
#include "osi/hash_map.h"

typedef void * http_query_params_handle_t;

esp_err_t http_query_params_init (httpd_req_t * req, http_query_params_handle_t * handle);

esp_err_t http_query_params_parse (http_query_params_handle_t handle, const char* param, size_t value_size, bool mandatory);

hash_map_t * http_query_params_done (http_query_params_handle_t handle);

#endif//__http_util_h