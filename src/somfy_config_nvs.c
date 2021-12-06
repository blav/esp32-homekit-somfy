#include <esp_err.h>
#include <nvs.h>
#include <esp_log.h>
#include "somfy_config_nvs.h"
#include "somfy_config_blob.h"

static const char * TAG = "somfy_config_nvs";

#define NVS_SOMFY_NAMESPACE "somfy-cfg"
#define NVS_SOMFY_CONFIG_KEY "config_data"

esp_err_t somfy_config_blob_nvs_write (somfy_config_blob_handle_t handle) {
    somfy_config_blob_t * blob = (somfy_config_blob_t *) handle;
    nvs_handle_t nvs;
    ESP_ERROR_CHECK (nvs_open(NVS_SOMFY_NAMESPACE, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK (nvs_set_blob (nvs, NVS_SOMFY_CONFIG_KEY, blob->blob, blob->size));
    ESP_ERROR_CHECK (nvs_commit(nvs));
    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t somfy_config_blob_nvs_read (somfy_config_blob_handle_t * handle) {
    nvs_handle_t nvs;
    ESP_ERROR_CHECK (nvs_open(NVS_SOMFY_NAMESPACE, NVS_READWRITE, &nvs));
    size_t size = 0;
    esp_err_t read = nvs_get_blob(nvs, NVS_SOMFY_CONFIG_KEY, NULL, &size);
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

    read = nvs_get_blob(nvs, NVS_SOMFY_CONFIG_KEY, blob->blob, &size);
    if (read != ESP_OK) {
        ESP_LOGE(TAG, "failed to get config data %s", esp_err_to_name(read));
        abort();
    }

    nvs_close(nvs);
    *handle = blob;
    return ESP_OK;
}

