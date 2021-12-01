#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <math.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include <iot_button.h>
#include <app_wifi.h>
#include <app_hap_setup_payload.h>
#include <driver/gpio.h>
#include <nvs_flash.h>
#include <osi/list.h>
#include <nvs.h>

#include "homekit_rollers.h"
#include "somfy.h"
#include "pulse.h"

static const char *TAG = "somfy_homekit";

typedef struct {
    list_t * rollers;
    somfy_ctl_handle_t ctl;
} homekit_rollers_t;

typedef struct {
    homekit_rollers_t * rollers;
    hap_serv_t * service;
    somfy_remote_t remote;
    int current_position;
    int target_position;
    roller_position_state_t position_state;
    QueueHandle_t command_queue;
    TaskHandle_t command_task;
} homekit_roller_t;

esp_err_t roller_nvs_save (homekit_roller_handle_t handle);

esp_err_t roller_nvs_load (homekit_roller_handle_t handle);

esp_err_t homekit_rollers_init (somfy_ctl_handle_t ctl, homekit_rollers_handle_t * handle) {
    homekit_rollers_t * rollers = calloc(1, sizeof (homekit_rollers_t));
    rollers->ctl = ctl;
    rollers->rollers = list_new (free);
    *handle = rollers;
    return ESP_OK;
}

void update_roller (void * data);

esp_err_t homekit_rollers_add (homekit_rollers_handle_t rollers_handle, somfy_remote_t remote, hap_serv_t * service, homekit_roller_handle_t * handle) {
    homekit_roller_t * roller = calloc(1, sizeof (homekit_roller_t));
    roller->remote = remote;
    roller->rollers = rollers_handle;
    roller->service = service;
    roller->current_position = 100;
    roller->target_position = 100;
    roller->position_state = ROLLER_POSITION_STATE_STOPPED;
    roller->command_queue = xQueueCreate(1, sizeof(int));

    char task_name [18];
    sprintf(task_name, "roller_cmd_%06x", remote);
    xTaskCreate (&update_roller, task_name, 4096, roller, 1, &roller->command_task);
    roller_nvs_load (roller);
    
    *handle = roller;

    homekit_rollers_t * rollers = (homekit_rollers_t *) rollers_handle;
    list_append (rollers->rollers, roller);
    return ESP_OK;
}

/* Reset network credentials if button is pressed for more than 3 seconds and then released */
#define RESET_NETWORK_BUTTON_TIMEOUT        3

/* Reset to factory if button is pressed and held for more than 10 seconds */
#define RESET_TO_FACTORY_BUTTON_TIMEOUT     10

/* The button "Boot" will be used as the Reset button for the example */
#define RESET_GPIO  GPIO_NUM_0

/**
 * @brief The network reset button callback handler.
 * Useful for testing the Wi-Fi re-configuration feature of WAC2
 */
static void reset_network_handler(void* arg) {
    hap_reset_network();
}
/**
 * @brief The factory reset button callback handler.
 */
static void reset_to_factory_handler(void* arg) {
    hap_reset_to_factory();
}

/**
 * The Reset button  GPIO initialisation function.
 * Same button will be used for resetting Wi-Fi network as well as for reset to factory based on
 * the time for which the button is pressed.
 */
static void reset_key_init(uint32_t key_gpio_pin) {
    button_handle_t handle = iot_button_create(key_gpio_pin, BUTTON_ACTIVE_LOW);
    iot_button_add_on_release_cb(handle, RESET_NETWORK_BUTTON_TIMEOUT, reset_network_handler, NULL);
    iot_button_add_on_press_cb(handle, RESET_TO_FACTORY_BUTTON_TIMEOUT, reset_to_factory_handler, NULL);
}

static int bridge_identify(hap_acc_t *ha) {
    ESP_LOGI(TAG, "Bridge identified");
    return HAP_SUCCESS;
}

static int accessory_identify (hap_acc_t *ha) {
    hap_serv_t *hs = hap_acc_get_serv_by_uuid(ha, HAP_SERV_UUID_ACCESSORY_INFORMATION);
    hap_char_t *hc = hap_serv_get_char_by_uuid(hs, HAP_CHAR_UUID_NAME);
    const hap_val_t *val = hap_char_get_val(hc);
    char *name = val->s;

    ESP_LOGI(TAG, "Bridged Accessory %s identified", name);
    return HAP_SUCCESS;
}

static int roller_write (
    hap_write_data_t write_data[], 
    int count,
    void *serv_priv, 
    void *write_priv
) {
    homekit_roller_t * priv = serv_priv;
    somfy_ctl_get(priv->rollers->ctl, NULL);
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        const char * char_uuid = hap_char_get_type_uuid(write->hc);
        if (!strcmp(char_uuid, HAP_CHAR_UUID_TARGET_POSITION)) {
            int target_position = write->val.i;
            xQueueReset(priv->command_queue);
            if (xQueueSendToBack (priv->command_queue, &target_position, 1000 / portTICK_PERIOD_MS) == errQUEUE_FULL) {
                ESP_LOGI(TAG, "queue full for remote 0x%06x", priv->remote);
                *(write->status) = HAP_STATUS_RES_BUSY;
            } else {
                *(write->status) = HAP_STATUS_SUCCESS;
            }
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    return ret;
}

static int roller_read (
    hap_char_t *hc, 
    hap_status_t *read_status, 
    void *serv_priv, 
    void *read_priv
) {
    if (hap_req_get_ctrl_id (read_priv))
        ESP_LOGI(TAG, "Received read from %s", hap_req_get_ctrl_id(read_priv));
    
    const char * char_uuid = hap_char_get_type_uuid(hc);

    homekit_roller_t * priv = serv_priv;
    if (!strcmp(char_uuid, HAP_CHAR_UUID_CURRENT_POSITION)) {
        hap_val_t new_val = {
            .i = priv->current_position
        };
        hap_char_update_val(hc, &new_val);
        *read_status = HAP_STATUS_SUCCESS;
    } else if (!strcmp(char_uuid, HAP_CHAR_UUID_POSITION_STATE)) {
        hap_val_t new_val = {
            .i = priv->position_state
        };
        hap_char_update_val(hc, &new_val);
        *read_status = HAP_STATUS_SUCCESS;
    } else if (!strcmp(char_uuid, HAP_CHAR_UUID_TARGET_POSITION)) {
        hap_val_t new_val = {
            .i = priv->target_position
        };

        hap_char_update_val(hc, &new_val);
        *read_status = HAP_STATUS_SUCCESS;
    } else {
        *read_status = HAP_STATUS_RES_ABSENT;
    }

    return HAP_SUCCESS;
}


void create_roller_accessory (somfy_config_remote_handle_t remote, void * data) {
    char * accessory_name;
    somfy_remote_t remote_id;
    somfy_config_remote_get (remote, &accessory_name, &remote_id, NULL);

    hap_acc_cfg_t bridge_cfg = {
        .name = accessory_name,
        .manufacturer = "Somfy",
        .model = "Somfy Roller",
        .serial_num = "000000001",
        .fw_rev = "0.9.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = accessory_identify,
        .cid = HAP_CID_BRIDGE,
    };

    hap_acc_t * accessory = hap_acc_create(&bridge_cfg);
    hap_serv_t * service = hap_serv_window_covering_create (0, 0, 0);

    homekit_rollers_handle_t rollers = data;
    homekit_roller_handle_t roller;
    homekit_rollers_add (rollers, remote_id, service, &roller);

    hap_serv_add_char(service, hap_char_name_create(accessory_name));
    hap_serv_set_priv(service, roller);
    hap_serv_set_write_cb(service, roller_write);
    hap_serv_set_read_cb (service, roller_read);
    hap_acc_add_serv(accessory, service);
    hap_add_bridged_accessory(accessory, hap_get_unique_aid(accessory_name));
}

esp_err_t homekit_rollers_start (somfy_ctl_handle_t ctl, homekit_rollers_handle_t * handle) {
    hap_init (HAP_TRANSPORT_WIFI);

    homekit_rollers_init(ctl, handle);
    hap_acc_cfg_t cfg = {
        .name = "Somfy",
        .manufacturer = "Somfy",
        .model = "Somfy HAP Bridge",
        .serial_num = "000000001",
        .fw_rev = "0.9.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = bridge_identify,
        .cid = HAP_CID_BRIDGE,
    };

    hap_acc_t * accessory = hap_acc_create(&cfg);

    uint8_t product_data[] = { 'S','o','m','f','y','H','A','P' };
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));
    hap_add_accessory(accessory);
    somfy_config_handle_t config;
    somfy_ctl_get (ctl, &config);
    somfy_config_remote_for_each (config, &create_roller_accessory, *handle);
    reset_key_init(RESET_GPIO);

#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
    hap_set_setup_code(CONFIG_EXAMPLE_SETUP_CODE);
    hap_set_setup_id(CONFIG_EXAMPLE_SETUP_ID);
#ifdef CONFIG_APP_WIFI_USE_WAC_PROVISIONING
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, true, cfg.cid);
#else
    app_hap_setup_payload(CONFIG_EXAMPLE_SETUP_CODE, CONFIG_EXAMPLE_SETUP_ID, false, cfg.cid);
#endif
#endif

    hap_enable_mfi_auth (HAP_MFI_AUTH_HW);
    esp_netif_init ();
    app_wifi_init ();
    hap_start ();
    app_wifi_start (portMAX_DELAY);

    return ESP_OK;
}

esp_err_t homekit_roller_set (
    homekit_roller_handle_t handle, 
    int * current_position, 
    int * target_position, 
    roller_position_state_t * position_state
) {
    homekit_roller_t * roller = handle;
    if (current_position != NULL) {
        hap_char_t * hap_char = hap_serv_get_char_by_uuid (roller->service, HAP_CHAR_UUID_CURRENT_POSITION);
        hap_val_t val = {
            .i = *current_position
        };

        hap_char_update_val(hap_char, &val);
    }

    if (position_state != NULL) {
        hap_char_t * hap_char = hap_serv_get_char_by_uuid (roller->service, HAP_CHAR_UUID_POSITION_STATE);
        hap_val_t val = {
            .i = *position_state
        };

        hap_char_update_val(hap_char, &val);
    }

    if (target_position != NULL) {
        hap_char_t * hap_char = hap_serv_get_char_by_uuid (roller->service, HAP_CHAR_UUID_TARGET_POSITION);
        hap_val_t val = {
            .i = *target_position
        };

        hap_char_update_val(hap_char, &val);
    }

    return ESP_OK;
}

void update_roller (void * data) {
    homekit_roller_t * roller = data;
    while (1) {
        int target_position;
        BaseType_t result = xQueueReceive(roller->command_queue, &target_position, 10000 / portTICK_PERIOD_MS);
        if (result != pdTRUE)
            continue;

        roller->target_position = target_position;
        homekit_roller_set(roller, NULL, &target_position, NULL);

        if (roller->target_position == roller->current_position) {
            roller->position_state = ROLLER_POSITION_STATE_STOPPED;
            homekit_roller_set(roller, &roller->current_position, NULL, &roller->position_state);
            continue;
        }

        int delta = roller->target_position - roller->current_position;
        roller->position_state = delta > 0 ? 
            ROLLER_POSITION_STATE_INCREASING : 
            ROLLER_POSITION_STATE_DECREASING;

        homekit_roller_set(roller, NULL, NULL, &roller->position_state);

        somfy_ctl_send_command (roller->rollers->ctl, roller->remote, 
            delta > 0 ? BUTTON_UP : BUTTON_DOWN);

        TickType_t delay = 23000 * abs(delta) / 100 / portTICK_PERIOD_MS;
        vTaskDelay (delay);

        if (roller->target_position != 0 && roller->target_position != 100)
            somfy_ctl_send_command (roller->rollers->ctl, roller->remote, BUTTON_STOP);

        roller->position_state = ROLLER_POSITION_STATE_STOPPED;
        roller->current_position = roller->target_position;
        homekit_roller_set(roller, &roller->current_position, NULL, &roller->position_state);
        roller_nvs_save (roller);

        ESP_LOGI(TAG, "roller current_position %d updated to target_position %d", 
            roller->current_position, 
            roller->target_position);
    }
}

esp_err_t roller_nvs_save (homekit_roller_handle_t handle) {
    homekit_roller_t * roller = handle;
    nvs_handle_t nvs;
    char key [30];
    sprintf (key, "current_position_%06x", roller->remote);

    ESP_ERROR_CHECK (nvs_open("somfy-hap", NVS_READWRITE, &nvs));
    nvs_set_i32 (nvs, key, (int32_t) roller->current_position);
    nvs_close(nvs);
    ESP_LOGI(TAG, "remote 0x%06x saved current_position at %d", roller->remote, roller->current_position);
    return ESP_OK;
}

esp_err_t roller_nvs_load (homekit_roller_handle_t handle) {
    homekit_roller_t * roller = handle;
    nvs_handle_t nvs;
    ESP_ERROR_CHECK (nvs_open("somfy-hap", NVS_READWRITE, &nvs));

    char key [30];
    sprintf (key, "current_position_%06x", roller->remote);

    int32_t current_position;
    esp_err_t read = nvs_get_i32 (nvs, key, &current_position);
    if (read == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs entry %s not found %s", key, esp_err_to_name(read));
        return ESP_OK;
    } else if (read != ESP_OK) {
        ESP_LOGE(TAG, "%s", esp_err_to_name(read));
        abort ();
    }

    ESP_LOGI(TAG, "remote 0x%06x restored at current_position %d", roller->remote, roller->current_position);
    roller->current_position = current_position;
    roller->target_position = current_position;
    nvs_close (nvs);
    return ESP_OK;
}

