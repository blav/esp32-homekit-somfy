#include <esp_log.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "somfy.h"
#include "nvs.h"
#include "osi/list.h"
#include "somfy_config_blob.h"
#include "mutex.h"

typedef struct {
  list_t * remotes;
  SemaphoreHandle_t remotes_mutex;
} somfy_config_t;

void somfy_config_remote_free_cb (void * data) {
    somfy_config_remote_handle_t cfg = (somfy_config_remote_handle_t) data;
    somfy_config_remote_free (cfg);
}

esp_err_t somfy_config_new(somfy_config_handle_t * handle) {
    somfy_config_t * cfg = calloc(1, sizeof(somfy_config_t));
    cfg->remotes = list_new(&somfy_config_remote_free_cb);
    cfg->remotes_mutex = xSemaphoreCreateMutex();
    *handle = cfg;
    return ESP_OK;
}

esp_err_t somfy_config_remote_new(const char * remote_name, somfy_remote_t remote, somfy_rolling_code_t code, somfy_config_remote_handle_t * handle) {
    somfy_config_remote_t * remote_cfg = calloc (1, sizeof (somfy_config_remote_t));
    *handle = remote_cfg;
    remote_cfg->remote = remote;
    remote_cfg->rolling_code = code;
    if (remote_name != NULL) {
        remote_cfg->remote_name = calloc (strlen(remote_name) + 1, sizeof(char));
        memcpy (remote_cfg->remote_name, remote_name, strlen(remote_name));
    }

    return ESP_OK;
}

esp_err_t somfy_config_free (somfy_config_handle_t handle) {
    somfy_config_t * cfg = (somfy_config_t *) handle;
    list_free(cfg->remotes);
    vSemaphoreDelete (cfg->remotes_mutex);
    free(cfg);
    return ESP_OK;
}

esp_err_t somfy_config_remote_free (somfy_config_remote_handle_t handle) {
    somfy_config_remote_t * remote = (somfy_config_remote_t *) handle;
    if (remote->remote_name != NULL)
        free(remote->remote_name);

    free(remote);
    return ESP_OK;
}

esp_err_t somfy_config_add_remote(somfy_config_handle_t handle, somfy_config_remote_handle_t config) {
    somfy_config_t * cfg = (somfy_config_t *) handle;
    MUTEX_TAKE(cfg->remotes_mutex);
    list_append(cfg->remotes, config);
    MUTEX_GIVE(cfg->remotes_mutex);
    return ESP_OK;
}


esp_err_t somfy_config_serialize (somfy_config_handle_t cfg, somfy_config_blob_handle_t * handle) {
    somfy_config_t * config = (somfy_config_t *) cfg;
    MUTEX_TAKE(config->remotes_mutex);
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

    MUTEX_GIVE(config->remotes_mutex);

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
        somfy_config_remote_handle_t remote_handle;

        somfy_config_remote_new (NULL, 0, 0, &remote_handle);
        somfy_config_remote_t * remote = (somfy_config_remote_t *) remote_handle;

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

        somfy_config_add_remote(*cfg, remote_handle);
    }

    return ESP_OK;
}

esp_err_t somfy_config_blob_free (somfy_config_blob_handle_t handle) {
    somfy_config_blob_t * blob = (somfy_config_blob_t *) handle;
    free(blob->blob);
    free(blob);
    return ESP_OK;
}

esp_err_t somfy_config_increment_rolling_code (somfy_config_handle_t handle, somfy_remote_t remote, somfy_rolling_code_t * rolling_code) {
    somfy_config_t * cfg = (somfy_config_t *) handle;
    MUTEX_TAKE(cfg->remotes_mutex);
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

    MUTEX_GIVE(cfg->remotes_mutex);

    return ESP_OK;
}