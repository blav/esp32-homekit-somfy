#include <nvs_flash.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "somfy.h"
#include "homekit_rollers.h"

static const char * TAG = "main";

static void homekit_bridge_task (void * data) {
    nvs_flash_init();

    somfy_config_handle_t config;
    somfy_config_new_from_nvs (&config);
    pulse_ctl_config_t pulse_cfg = {
        .gpio = SOMFY_GPIO,
        .timer_group = TIMER_GROUP_0,
        .timer_idx = TIMER_0,
        .max_queue_size = 3
    };

    somfy_ctl_handle_t ctl;
    somfy_ctl_init (config, &pulse_cfg, &ctl); 

    homekit_rollers_handle_t rollers;
    homekit_rollers_start (ctl, &rollers);

    somfy_config_blob_handle_t blob;
    somfy_config_serialize(config, &blob);
    somfy_config_blob_http_write (blob, "http://blav.ngrok.io/config");
    somfy_config_blob_free(blob);

    /* The task ends here. The read/write callbacks will be invoked by the HAP Framework */
    vTaskDelete(NULL);
}

void app_main () {
    xTaskCreate(homekit_bridge_task, "somfy_hap_bridge", 4096, NULL, 1, NULL);
}
