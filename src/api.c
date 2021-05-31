#include "api.h"

esp_err_t set_rolling_code_handler(httpd_req_t* req) {
  size_t qs_length = httpd_req_get_url_query_len(req) + 1;
  char* qs = calloc(qs_length, sizeof(char));
  httpd_req_get_url_query_str(req, qs, qs_length);

  char remote[20];
  esp_err_t result = httpd_query_key_value(qs, "r", remote, 20);
  if (result == ESP_ERR_NOT_FOUND) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "r");
    goto finally;
  } else if (result != ESP_OK) {
    httpd_resp_send_500(req);
    goto finally;
  }

  

  const char resp[] = "URI GET Response";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

  finally:
  free(qs);
  return ESP_OK;
}


httpd_uri_t set_rolling_code_uri = {
    .uri = "/rolling-code",
    .method = HTTP_GET,
    .handler = set_rolling_code_handler,
    .user_ctx = NULL
};

httpd_handle_t api_start() {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &set_rolling_code_uri);
  }

  return server;
}

void api_stop(httpd_handle_t server) {
  if (server)
    httpd_stop(server);
}
