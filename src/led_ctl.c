#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "led_ctl.h"

typedef struct {
    gpio_num_t gpio;
    led_mode_t mode;
    TaskHandle_t task;
    QueueHandle_t queue;
} led_ctl_t;

static void led_ctl_mode_decode (led_mode_t mode, int * down, int * up) {
    switch (mode) {
        case LED_MODE_ON:
            *up = 10000;
            *down = 0;
            break;
        case LED_MODE_OFF:
            *up = 0;
            *down = 10000;
            break;
        case LED_MODE_PROG_MODE:
            *up = 20;
            *down = 480;
            break;
        case LED_MODE_SLEEP:
        default:
            *up = 10;
            *down = 4990;
            break;
    }
}

static void led_ctl_task (void * data) {
    int down;
    int up;
    led_ctl_t * ctl = (led_ctl_t *) data;
    led_ctl_mode_decode (ctl->mode, &down, &up);
    while (1) {
        if (up > 0) {
            gpio_set_level (ctl->gpio, 1);
            if (xQueueReceive(ctl->queue, &ctl->mode, up / portTICK_PERIOD_MS) == pdTRUE)
                led_ctl_mode_decode (ctl->mode, &down, &up);
        }

        if (down > 0) {
            gpio_set_level (ctl->gpio, 0);
            if (xQueueReceive(ctl->queue, &ctl->mode, down / portTICK_PERIOD_MS) == pdTRUE)
                led_ctl_mode_decode (ctl->mode, &down, &up);
        }
    }
}

esp_err_t led_ctl_init (gpio_num_t gpio, led_mode_t mode, led_ctl_handle_t * handle) {
    led_ctl_t * ctl = calloc (1, sizeof(led_ctl_t));
    ctl->gpio = gpio;
    ctl->mode = mode;
    gpio_config_t io_conf = {
        .intr_type = GPIO_PIN_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << gpio),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };

    gpio_config(&io_conf);
    gpio_set_level (ctl->gpio, 0);

    char task_name [20] = "";
    sprintf (task_name, "led_ctl_%d", gpio);
    xTaskCreate (&led_ctl_task, task_name, 4096, ctl, 1, &ctl->task);
    ctl->queue = xQueueCreate(10, sizeof(led_mode_t));
    *handle = ctl;
    return ESP_OK;
}

esp_err_t led_ctl_free (led_ctl_handle_t handle) {
    led_ctl_t * ctl = (led_ctl_t *) handle;
    vTaskDelete(ctl->task);
    vQueueDelete(ctl->queue);
    free(ctl);
    return ESP_OK;
}

esp_err_t led_ctl_set_mode (led_ctl_handle_t handle, led_mode_t mode) {
    led_ctl_t * ctl = handle;
    if (xQueueSend (ctl->queue, &mode, 10000 / portTICK_PERIOD_MS) == pdTRUE) {
        return ESP_OK;
    } else {
        return ESP_ERR_TIMEOUT;
    }
}

