#include "http_util.h"
#include "osi/hash_functions.h"

typedef struct {
  char * query_string;
  hash_map_t * result;
  httpd_req_t * request;
} http_query_params_t;

void free_key (void * key) {
}

bool key_compare (const void * a, const void * b) {
  return strcmp ((char *) a, (char *) b);
}

esp_err_t http_query_params_init (httpd_req_t * request, http_query_params_handle_t * handle) {
  http_query_params_t * params = calloc (1, sizeof(http_query_params_t));
  params->request = request;

  // query_string
  size_t length = httpd_req_get_url_query_len(request) + 1;
  params->query_string = calloc(length, sizeof(char));
  httpd_req_get_url_query_str(request, params->query_string, length);

  // result
  params->result = hash_map_new(2, hash_function_string, &free_key, &free, &key_compare);
  *handle = params;
  return ESP_OK;
}

esp_err_t http_query_params_parse (http_query_params_handle_t handle, const char* param, size_t value_size, bool mandatory) {
  char * value = calloc(value_size + 1, sizeof(char));
  if (value == NULL)
    return ESP_ERR_NO_MEM;

  http_query_params_t * params = handle;
  esp_err_t result = httpd_query_key_value(params->query_string, param, value, value_size);
  esp_err_t retcode;
  if (result == ESP_OK) {
    if (! hash_map_set (params->result, param, value)) {
      return ESP_OK;
    } else {
      retcode = ESP_ERR_NO_MEM;
      goto onerror;
    }
  }

  if (result == ESP_ERR_NOT_FOUND) {
    httpd_resp_send_err(params->request, HTTPD_400_BAD_REQUEST, param);
    retcode = ESP_ERR_NOT_FOUND;
  } else if (result != ESP_OK) {
    httpd_resp_send_500(params->request);
    retcode = ESP_OK;
  } else {

  }

  onerror:
  free(value);
  return retcode;
}

hash_map_t * http_query_params_done (http_query_params_handle_t handle) {
  http_query_params_t * params = handle;
  free(params->query_string);
  free(params);
  return params->result;
}
