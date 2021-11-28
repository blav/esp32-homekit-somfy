#include <esp_log.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "somfy.h"
#include "nvs.h"
#include "esp_http_client.h"
const static char * TAG = "somfy_config";

typedef struct {
    size_t size;
    void * blob;
} somfy_config_blob_t;

esp_err_t somfy_config_blob_serialize (somfy_config_t * config, somfy_config_blob_t * blob);

esp_err_t somfy_config_blob_deserialize (somfy_config_blob_t * blob, somfy_config_t ** config);

esp_err_t somfy_config_blob_free (somfy_config_blob_t blob);

esp_err_t somfy_config_get(somfy_config_t *config) {
    return ESP_OK;
}

esp_err_t somfy_config_set(somfy_config_t *config) {
    return ESP_OK;
}

esp_err_t event_handle(esp_http_client_event_t *event);

esp_err_t somfy_config_save(somfy_config_t *config, char *url) {
    somfy_config_blob_t blob;
    somfy_config_blob_serialize(config, &blob);
    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = event_handle,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    esp_http_client_set_header (client, "Content-Type", "application/octet-stream");
    esp_http_client_set_post_field (client, blob.blob, blob.size);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    }

    esp_http_client_cleanup(client);
    somfy_config_blob_free(blob);

    return ESP_OK;
}

esp_err_t somfy_config_load(somfy_config_t *config, char *url) {
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

esp_err_t somfy_config_blob_serialize (somfy_config_t * config, somfy_config_blob_t * blob) {
    size_t size = sizeof(uint8_t);
    uint8_t count = 0;
    for (somfy_remote_config_t * cur = config->remotes; cur != NULL; cur = cur->next) {
        count ++;
        size += sizeof (somfy_remote_t);
        size += sizeof (somfy_rolling_code_t);
        size += sizeof (uint8_t);
        size += strlen (cur->remote_name);
    }

    void * buffer = calloc(size, sizeof(uint8_t));
    size_t i = 0;
    memcpy (buffer + i, &count, sizeof(uint8_t));
    i += sizeof(uint8_t);
    for (somfy_remote_config_t * cur = config->remotes; cur != NULL; cur = cur->next) {
        memcpy (buffer + i, &cur->remote, sizeof(somfy_remote_t));
        i += sizeof(somfy_remote_t);
        memcpy (buffer + i, &cur->rolling_code, sizeof(somfy_rolling_code_t));
        i += sizeof(somfy_rolling_code_t);
        uint8_t name_len = strlen(cur->remote_name);
        memcpy (buffer + i, &name_len, sizeof(uint8_t));
        i += sizeof(uint8_t);
        memcpy (buffer + i, cur->remote_name, name_len);
        i += name_len;
    }

    blob->size = size;
    blob->blob = buffer;
    return ESP_OK;
}


esp_err_t somfy_config_blob_free (somfy_config_blob_t blob) {
    free(blob.blob);
    return ESP_OK;
}
