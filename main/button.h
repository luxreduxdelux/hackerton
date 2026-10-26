#pragma once

//================================================================

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

//================================================================

typedef enum {
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_CENTER,
    BTN_KEY1,
    BTN_KEY2,
    BTN_KEY3,
    BTN_KEY4,
    BTN_COUNT,
} button_id_t;

typedef struct {
    button_id_t id;
    bool pressed;
} button_event_t;

//================================================================

esp_err_t button_initialize(void);
QueueHandle_t button_event_queue(void);
uint32_t button_read(void);