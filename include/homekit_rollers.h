#ifndef __homekit_rollers_h
#define __homekit_rollers_h

#include "somfy.h"

typedef void * homekit_rollers_handle_t;

typedef void * homekit_roller_handle_t;

typedef enum  {
    ROLLER_POSITION_STATE_DECREASING = 0,
    ROLLER_POSITION_STATE_INCREASING = 1,
    ROLLER_POSITION_STATE_STOPPED = 2
} roller_position_state_t;

esp_err_t homekit_rollers_start (somfy_ctl_handle_t ctl, homekit_rollers_handle_t * handle);

esp_err_t homekit_rollers_set (
    homekit_rollers_handle_t roller, 
    somfy_remote_t remote, 
    int * current_position, 
    roller_position_state_t * position_state);

esp_err_t homekit_roller_set (
    homekit_roller_handle_t roller, 
    int * current_position, 
    int * target_position, 
    roller_position_state_t * position_state);


#endif//__homekit_rollers_h