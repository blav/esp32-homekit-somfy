#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include <iot_button.h>

#include <app_wifi.h>
#include <app_hap_setup_payload.h>
#include <driver/gpio.h>
#include <nvs_flash.h>

#include "somfy_homekit.h"
#include "somfy.h"
#include "pulse.h"

static const char *TAG = "somfy_homkit";


#define NUM_BRIDGED_ACCESSORIES 3

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

typedef enum  {
    ROLLER_POSITION_STATE_DECREASING = 0,
    ROLLER_POSITION_STATE_INCREASING = 1,
    ROLLER_POSITION_STATE_STOPPED = 2
} roller_position_state_t;

void roller_target_position_set (int target_position) {
    ESP_LOGI(TAG, "write roller target position %d", target_position);
}

int roller_target_position_get (int current_target_position) {
    ESP_LOGI(TAG, "read roller target_position. current target_position is %d", current_target_position);
    return 0;
}

int roller_current_position_get (int current_current_position) {
    ESP_LOGI(TAG, "read roller current_position. current current_position is %d", current_current_position);
    return 0;
}

int roller_position_state_get (roller_position_state_t current_position_state) {
    ESP_LOGI(TAG, "read roller position_state. current position_state is %d", (int) current_position_state);
    return ROLLER_POSITION_STATE_STOPPED;
}

static int roller_write (
    hap_write_data_t write_data[], 
    int count,
    void *serv_priv, 
    void *write_priv
) {
    ESP_LOGI(TAG, "write called for remote 0x%08x", *(somfy_remote_t *)serv_priv);
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        const char * char_uuid = hap_char_get_type_uuid(write->hc);
        if (!strcmp(char_uuid, HAP_CHAR_UUID_TARGET_POSITION)) {
            int target_position = write->val.i;
            roller_target_position_set (target_position);
            hap_char_update_val(write->hc, &(write->val));
            *(write->status) = HAP_STATUS_SUCCESS;
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
    ESP_LOGI(TAG, "reading characteristic %s", char_uuid);
    if (!strcmp(char_uuid, HAP_CHAR_UUID_CURRENT_POSITION)) {
        const hap_val_t *cur_val = hap_char_get_val (hc);
        hap_val_t new_val = {
            .i = roller_current_position_get (cur_val->i)
        };
        hap_char_update_val(hc, &new_val);
        *read_status = HAP_STATUS_SUCCESS;
    } else if (!strcmp(char_uuid, HAP_CHAR_UUID_POSITION_STATE)) {
        const hap_val_t *cur_val = hap_char_get_val (hc);
        hap_val_t new_val = {
            .i = roller_position_state_get ((roller_position_state_t) cur_val->i)
        };
        hap_char_update_val(hc, &new_val);
        *read_status = HAP_STATUS_SUCCESS;
    } else if (!strcmp(char_uuid, HAP_CHAR_UUID_TARGET_POSITION)) {
        const hap_val_t *cur_val = hap_char_get_val (hc);
        hap_val_t new_val = {
            .i = roller_target_position_get (cur_val->i)
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
    somfy_remote_t * remote_id = calloc (1, sizeof(somfy_remote_t));
    somfy_config_remote_get (remote, &accessory_name, remote_id, NULL);
    hap_acc_cfg_t bridge_cfg = {
        .name = accessory_name,
        .manufacturer = "Espressif",
        .model = "EspFan01",
        .serial_num = "abcdefg",
        .fw_rev = "0.9.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = accessory_identify,
        .cid = HAP_CID_BRIDGE,
    };

    hap_acc_t * accessory = hap_acc_create(&bridge_cfg);
    hap_serv_t * roller = hap_serv_window_covering_create (0, 0, 0);
    hap_serv_add_char(roller, hap_char_name_create(accessory_name));
    hap_serv_set_priv(roller, remote_id);
    hap_serv_set_write_cb(roller, roller_write);
    hap_serv_set_read_cb (roller, roller_read);
    hap_acc_add_serv(accessory, roller);
    hap_add_bridged_accessory(accessory, hap_get_unique_aid(accessory_name));
}

esp_err_t somfy_homekit_start (somfy_config_handle_t config) {
    hap_init (HAP_TRANSPORT_WIFI);
    hap_acc_cfg_t cfg = {
        .name = "Esp-Bridge",
        .manufacturer = "Espressif",
        .model = "EspBridge01",
        .serial_num = "001122334455",
        .fw_rev = "0.9.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = bridge_identify,
        .cid = HAP_CID_BRIDGE,
    };

    hap_acc_t * accessory = hap_acc_create(&cfg);

    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));
    hap_add_accessory(accessory);
    somfy_config_remote_for_each (config, &create_roller_accessory, NULL);
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

