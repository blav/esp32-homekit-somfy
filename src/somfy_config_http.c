#include <esp_err.h>
#include <nvs.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include "somfy_config_nvs.h"
#include "somfy_config_blob.h"


static const char * TAG = "somfy_config_http";

esp_err_t event_handle(esp_http_client_event_t *event);

esp_err_t somfy_config_blob_http_write(somfy_config_blob_handle_t blob_handle, char *url) {
    somfy_config_blob_t * blob = (somfy_config_blob_t *) blob_handle;
    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = event_handle,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    esp_http_client_set_header (client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field (client, blob->blob, blob->size);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    }

    esp_http_client_cleanup(client);
    return ESP_OK;
}

esp_err_t somfy_config_blob_http_read(somfy_config_blob_handle_t *handle, char *url) {
    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = event_handle,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    }

    esp_http_client_cleanup(client);

    return ESP_OK;
}

esp_err_t event_handle(esp_http_client_event_t *event){
        switch(event->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", event->data_len, (char*)event->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", event->data_len);
            if (!esp_http_client_is_chunked_response(event->client)) {
                printf("%.*s", event->data_len, (char*)event->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;

}