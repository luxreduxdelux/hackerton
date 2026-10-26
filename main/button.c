#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

//================================================================

#include "board.h"
#include "button.h"

//================================================================

#define DEBOUNCE_MS     20
#define EVENT_QUEUE_LEN 16

//================================================================

static const char *TAG = "button";
static const struct {
    gpio_num_t gpio;
    const char *name;
} s_button[BTN_COUNT] = {
    [BTN_UP]    = {BOARD_KEY_UP_IO, "UP"},
    [BTN_DOWN]  = {BOARD_KEY_DOWN_IO, "DOWN"},
    [BTN_LEFT]  = {BOARD_KEY_LEFT_IO, "LEFT"},
    [BTN_RIGHT] = {BOARD_KEY_RIGHT_IO, "RIGHT"},
    [BTN_CENTER] = {BOARD_KEY_CENTER_IO, "CENTER"},
    [BTN_KEY1] = {BOARD_KEY_1_IO, "KEY1"},
    [BTN_KEY2] = {BOARD_KEY_2_IO, "KEY2"},
    [BTN_KEY3] = {BOARD_KEY_3_IO, "KEY3"},
    [BTN_KEY4] = {BOARD_KEY_4_IO, "KEY4"},
};
static QueueHandle_t s_isr_queue;
static QueueHandle_t s_event_queue;
static volatile uint32_t s_state;

//================================================================

static inline bool level_is_pressed(button_id_t id) {
    return gpio_get_level(s_button[id].gpio) == 0;
}

static void IRAM_ATTR button_isr(void *arg) {
    button_id_t id = (button_id_t)(uintptr_t)arg;
    BaseType_t woken = pdFALSE;

    gpio_intr_disable(s_button[id].gpio);
    xQueueSendFromISR(s_isr_queue, &id, &woken);

    // Woken dice: Escribir a la queue desperto una task de mayor prioridad
    //              a la task interrumpida
    // Esta nada mas para evitar un priority inversion.
    // Si vieron SO lo entienden de una
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

static void button_task(void *arg) {
    button_id_t id;

    while (1) {
        xQueueReceive(s_isr_queue, &id, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));

        gpio_intr_enable(s_button[id].gpio);

        const uint32_t bit = 1u << id;
        bool pressed = level_is_pressed(id);
        if (pressed == !!(s_state & bit)) {
            continue;
        }
        s_state = pressed ? (s_state | bit) : (s_state & ~bit);

        const button_event_t ev = {.id = id, .pressed = pressed};
        xQueueSend(s_event_queue, &ev, 0);
    }
}

esp_err_t button_initialize(void) {
    s_isr_queue = xQueueCreate(BTN_COUNT, sizeof(button_id_t));
    s_event_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(button_event_t));
    ESP_RETURN_ON_FALSE(s_isr_queue && s_event_queue, ESP_ERR_NO_MEM, TAG, "no mem for queues");

    ESP_RETURN_ON_FALSE(xTaskCreate(button_task, "button", 3072, NULL, 10, NULL) == pdPASS, ESP_ERR_NO_MEM, TAG,
                        "task create failed");

    esp_err_t err = gpio_install_isr_service(0);
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "isr service install failed");

    for (int i = 0; i < BTN_COUNT; i++) {
        const gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << s_button[i].gpio,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "config of %s failed", s_button[i].name);
        // Por cada boton agregamos el isr de los botones como el handler.
        ESP_RETURN_ON_ERROR(gpio_isr_handler_add(s_button[i].gpio, button_isr, (void *)(uintptr_t)i), TAG,
                            "isr add of %s failed", s_button[i].name);
    }

    for (int i = 0; i < BTN_COUNT; i++) {
        if (level_is_pressed(i)) {
            s_state |= 1u << i;
        }
    }
    return ESP_OK;
}

QueueHandle_t button_event_queue(void) {
    return s_event_queue;
}

uint32_t button_read(void) {
    return s_state;
}