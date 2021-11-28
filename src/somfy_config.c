#include <esp_log.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "somfy.h"
#include "nvs.h"
#include "esp_http_client.h"
#include "osi/list.h"


const static char * TAG = "somfy_config";

typedef struct {
  list_t * remotes;
} somfy_config_t;

typedef struct {
    size_t size;
    void * blob;
} somfy_config_blob_t;


void somfy_config_remote_free_cb (void * data) {
    somfy_config_remote_t * cfg = (somfy_config_remote_t *) data;
    somfy_config_remote_free (cfg);
}

esp_err_t somfy_config_new(somfy_config_handle_t * handle) {
    somfy_config_t * cfg = calloc(1, sizeof(somfy_config_t));
    cfg->remotes = list_new(&somfy_config_remote_free_cb);
    *handle = cfg;
    return ESP_OK;
}

esp_err_t somfy_config_remote_new(const char * remote_name, somfy_remote_t remote, somfy_rolling_code_t code, somfy_config_remote_t ** handle) {
    *handle = calloc (1, sizeof (somfy_config_remote_t));
    (*handle)->remote = remote;
    (*handle)->rolling_code = code;
    if (remote_name != NULL) {
        (*handle)->remote_name = calloc (strlen(remote_name + 1), sizeof(char));
        strcpy ((*handle)->remote_name, remote_name);
    }

    return ESP_OK;
}


esp_err_t somfy_config_free(somfy_config_handle_t handle) {
    somfy_config_t * cfg = (somfy_config_t *) handle;
    list_free(cfg->remotes);
    free(cfg);
    return ESP_OK;
}

esp_err_t somfy_config_remote_free (somfy_config_remote_t * node) {
    if (node->remote_name != NULL)
        free(node->remote_name);

    free(node);
    return ESP_OK;
}

esp_err_t somfy_config_add_remote(somfy_config_handle_t handle, somfy_config_remote_t * config) {
    somfy_config_t * cfg = (somfy_config_t *) handle;
    list_append(cfg->remotes, config);
    return ESP_OK;
}

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

esp_err_t somfy_config_serialize (somfy_config_handle_t cfg, somfy_config_blob_handle_t * handle) {
    somfy_config_t * config = (somfy_config_t *) cfg;
    size_t size = sizeof(uint8_t);
    uint8_t count = 0;
    for (list_node_t * node = list_begin (config->remotes); node != NULL; node = list_next (node)) {
        somfy_config_remote_t * cur = list_node(node);
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
    for (list_node_t * node = list_begin (config->remotes); node != NULL; node = list_next (node)) {
        somfy_config_remote_t * cur = list_node(node);
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

    somfy_config_blob_t * blob = calloc(1, sizeof(somfy_config_blob_t));
    blob->size = size;
    blob->blob = buffer;
    *handle = blob;
    return ESP_OK;
}

esp_err_t somfy_config_deserialize (somfy_config_blob_handle_t handle, somfy_config_handle_t * cfg) {
    somfy_config_new(cfg);
    somfy_config_blob_t * blob = (somfy_config_blob_t *) handle;
    size_t offset = 0;
    uint8_t remote_count;
    memcpy(&remote_count, blob->blob + offset, sizeof(uint8_t));
    offset += sizeof(uint8_t);
    for (int i = 0; i < remote_count; i ++) {
        somfy_config_remote_t * remote;
        somfy_config_remote_new (NULL, 0, 0, &remote);
        memcpy(&remote->remote, blob->blob + offset, sizeof(somfy_remote_t));
        offset += sizeof(somfy_remote_t);
        memcpy(&remote->rolling_code, blob->blob + offset, sizeof(somfy_rolling_code_t));
        offset += sizeof(somfy_rolling_code_t);
        uint8_t name_len;
        memcpy(&name_len, blob->blob + offset, sizeof(uint8_t));
        offset += sizeof(uint8_t);
        remote->remote_name = calloc(name_len + 1, sizeof(char));
        memcpy(remote->remote_name, blob->blob + offset, name_len);
        offset += name_len;
        somfy_config_add_remote(cfg, remote);
    }

    return ESP_OK;
}


esp_err_t somfy_config_blob_free (somfy_config_blob_handle_t handle) {
    somfy_config_blob_t * blob = (somfy_config_blob_t *) handle;
    free(blob->blob);
    free(blob);
    return ESP_OK;
}

esp_err_t somfy_config_blob_nvs_write (somfy_config_blob_handle_t handle) {
    somfy_config_blob_t * blob = (somfy_config_blob_t *) handle;
    nvs_handle_t nvs;
    ESP_ERROR_CHECK (nvs_open("somfy-cfg", NVS_READWRITE, &nvs));
    nvs_set_blob (nvs, "config_data", blob->blob, blob->size);
    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t somfy_config_blob_nvs_read (somfy_config_blob_handle_t * handle) {
    nvs_handle_t nvs;
    ESP_ERROR_CHECK (nvs_open("somfy-cfg", NVS_READWRITE, &nvs));
    size_t size = 0;
    esp_err_t read = nvs_get_blob(nvs, "config_data", NULL, &size);
    if (read == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs entry not found %s", esp_err_to_name(read));
        *handle = NULL;
        return ESP_ERR_NOT_FOUND;
    } else if (read != ESP_OK) {
        ESP_LOGE(TAG, "%s", esp_err_to_name(read));
        abort();
    }

    somfy_config_blob_t * blob = calloc (1, sizeof(somfy_config_blob_t));
    blob->size = size;
    blob->blob = calloc(size, sizeof(uint8_t));

    read = nvs_get_blob(nvs, "config_data", blob->blob, &size);
    if (read != ESP_OK) {
        ESP_LOGE(TAG, "failed to get config data %s", esp_err_to_name(read));
        abort();
    }

    nvs_close(nvs);
    *handle = blob;
    return ESP_OK;
}

esp_err_t somfy_config_increment_rolling_code (somfy_config_handle_t handle, somfy_remote_t remote, somfy_rolling_code_t * rolling_code) {
    somfy_config_t * cfg = (somfy_config_t *) handle;
    somfy_config_remote_t * found = NULL;
    for (list_node_t * node = list_begin(cfg->remotes); node != NULL; node = list_next(node)) {
        somfy_config_remote_t * cur = list_node(node);
        if (cur->remote == remote) {
            found = cur;
            break;
        }
    }

    if (found == NULL)
        return ESP_ERR_NOT_FOUND;

    found->rolling_code = found->rolling_code + 1;
    if (rolling_code != NULL)
        *rolling_code = found->rolling_code;

    return ESP_OK;
}