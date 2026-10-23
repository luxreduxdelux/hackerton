#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "audio.h"
#include "buttons.h"
#include "display.h"

static const char *TAG = "demo";

#define UI_PERIOD_MS 50

static const int s_button_freq[BTN_COUNT] = {
    [BTN_UP]     = 659,
    [BTN_DOWN]   = 440,
    [BTN_LEFT]   = 494,
    [BTN_RIGHT]  = 587,
    [BTN_CENTER] = 784,
    [BTN_KEY1]   = 262,
    [BTN_KEY2]   = 330,
    [BTN_KEY3]   = 392,
    [BTN_KEY4]   = 523,
};

static void draw_key(int x, int y, int size, bool pressed)
{
    if (pressed) {
        display_fill_rect(x, y, size, size, true);
    } else {
        display_fill_rect(x, y, size, size, false);
        display_draw_rect(x, y, size, size);
    }
}

static void draw_ui(uint32_t pressed, int mic_level, bool audio_ok)
{
    char line[24];

    display_clear();
    display_text(2, 1, "HACKATHON BOARD", true);

    display_text(0, 14, "MIC", false);
    display_draw_rect(22, 13, 82, 10);
    display_fill_rect(24, 15, mic_level * 78 / 100, 6, true);
    if (audio_ok) {
        snprintf(line, sizeof(line), "%3d", mic_level);
        display_text(107, 14, line, false);
    } else {
        display_text(107, 14, "ERR", false);
    }

    draw_key(15, 28, 10, pressed & (1u << BTN_UP));
    draw_key(4, 39, 10, pressed & (1u << BTN_LEFT));
    draw_key(15, 39, 10, pressed & (1u << BTN_CENTER));
    draw_key(26, 39, 10, pressed & (1u << BTN_RIGHT));
    draw_key(15, 50, 10, pressed & (1u << BTN_DOWN));

    display_text(50, 29, "KEYS", false);
    for (int i = 0; i < 4; i++) {
        bool on = pressed & (1u << (BTN_KEY1 + i));
        int x = 50 + i * 15;
        draw_key(x, 40, 13, on);
        char num[2] = {'1' + i, 0};
        display_text(x + 4, 43, num, on);
    }
}

typedef enum {
    MAIN,
    GAME,
    FAIL,
} State;

typedef struct App {
    State state;
    int16_t playerPosition;
    int16_t playerScore;
    int16_t obstaclePosition;
    int16_t obstacleHoleSize;
    int16_t obstacleHolePosition;
} App;

#define SCREEN_SIZE_X 128
#define SCREEN_SIZE_Y 64

void app_main(void)
{
    App app = { 0 };

    ESP_LOGI(TAG, "Hackathon board demo starting");

    bool display_ok = display_init() == ESP_OK;
    if (!display_ok) {
        ESP_LOGE(TAG, "display init failed - continuing without screen");
    }

    ESP_ERROR_CHECK(buttons_init());

    if (display_ok) {
        display_clear();
        display_text(10, 8, "Hello, Hackathon!", false);
        display_text(10, 24, "ESP32-S3 dev board", false);
        display_text(10, 40, "Self test...", false);
        display_flush();
    }

    bool audio_ok = audio_init() == ESP_OK;
    if (audio_ok) {
        const int notes[] = {523, 659, 784};
        for (int i = 0; i < 3; i++) {
            audio_play_tone(notes[i], 120);
        }
    } else {
        ESP_LOGE(TAG, "audio init failed - continuing without sound");
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    QueueHandle_t events = buttons_event_queue();
    int log_div = 0;

    while (1) {
        button_event_t ev;
        if (xQueueReceive(events, &ev, pdMS_TO_TICKS(UI_PERIOD_MS)) == pdTRUE) {
            ESP_LOGI(TAG, "button %s %s", buttons_name(ev.id), ev.pressed ? "pressed" : "released");
            if (ev.pressed && audio_ok) {
                audio_play_tone(s_button_freq[ev.id], 100);
            }
        }

        // sizeX = 128
        // sizeY = 64
        int mic_level = audio_ok ? audio_mic_level() : 0;
        if (display_ok) {
            switch (app.state) {
                case MAIN:
                    display_clear();
                    display_text(8,  8, "Hacky Bird", false);
                    display_text(8, 24, "[A] Comenzar", false);
                    display_flush();

                    if (buttons_read() & (1u << BTN_KEY1)) {
                        app.state = GAME;
                    }
                    break;
                case GAME:
                    if (mic_level >= 50) {
                        app.playerPosition -= mic_level - 50;
                    }

                    app.playerPosition   += 1;
                    app.obstaclePosition -= 2;

                    if (app.obstaclePosition < -16)  {
                        app.obstaclePosition     = SCREEN_SIZE_X + 16;
                        app.obstacleHoleSize     = 16;
                        app.obstacleHolePosition = 32;
                    }

                    if (app.playerPosition < 0) {
                        app.playerPosition = 0;
                    }
                    if (app.playerPosition > 48) {
                        app.state = FAIL;
                    }

                    display_clear();
                    display_draw_rect(56, app.playerPosition, 16, 16);
                    display_draw_rect(app.obstaclePosition, 0,                         16, SCREEN_SIZE_Y - app.obstacleHoleSize);
                    display_draw_rect(app.obstaclePosition, SCREEN_SIZE_Y - app.obstacleHoleSize, 16, app.obstacleHoleSize);
                    display_flush();
                    break;
                case FAIL:
                    display_clear();
                    display_text(8,  8, "Game Over", false);
                    display_text(8, 24, "Puntaje: ", false);
                    // TO-DO pasar app.playerScore a string
                    display_text(8 + DISPLAY_FONT_W * 9, 24, "TO-DO", false);
                    display_text(8, 48, "[A] Comenzar", false);
                    display_flush();

                    if (buttons_read() & (1u << BTN_KEY1)) {
                        app = (App) { 0 };
                        app.state = GAME;
                    }
                    break;
            }

            buttons_read();
            //draw_ui(buttons_read(), mic_level, audio_ok);
            //display_flush();
        }

        /*
        if (audio_ok && ++log_div >= 1000 / UI_PERIOD_MS) {
            log_div = 0;
            ESP_LOGI(TAG, "mic level: %d", mic_level);
        }
        */
    }
}
