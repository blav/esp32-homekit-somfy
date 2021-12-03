#ifndef __led_ctl_h
#define __led_ctl_h


#include <driver/gpio.h>

typedef void * led_ctl_handle_t;

typedef enum {
    LED_MODE_ON,
    LED_MODE_OFF,
    LED_MODE_NOTIFY,
    LED_MODE_SLEEP,
    LED_MODE_PROG_MODE
} led_mode_t;

esp_err_t led_ctl_init (gpio_num_t led, led_mode_t mode, led_ctl_handle_t * handle);

esp_err_t led_ctl_free (led_ctl_handle_t handle);

esp_err_t led_ctl_set_mode (led_ctl_handle_t handle, led_mode_t mode);

#endif//__led_ctl_h