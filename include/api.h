#ifndef __api_h
#define __api_h

#include "esp_http_server.h"

httpd_handle_t api_start ();

void api_stop (httpd_handle_t server);

#endif//__api_h