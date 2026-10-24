// stdlib
#include <stdio.h>
#include <unistd.h>
// freeRTOS/ESP32
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// local
#include "audio.h"
#include "buttons.h"
#include "display.h"
#include "network.h"


//================================================================

#define UI_PERIOD_MS 50
#define SCREEN_SIZE_X 128
#define SCREEN_SIZE_Y 64

static const char *TAG = "demo";
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

//================================================================

typedef enum {
    MAIN,
    LOBBY,
    GAME,
    VICTORY,
    FAILURE,
} State;

typedef struct Packet {
    uint8_t  mac[6];
    uint16_t score;
    uint8_t  life;
} Packet;

typedef struct Arrow {
    int16_t position;
} Arrow;

#define ARROW_COUNT 32
#define ARROW_SCALE 16

typedef struct Lane {
    Arrow   arrowList[ARROW_COUNT];
    uint8_t arrowCount;
} Lane;

#define LANE_COUNT 3
#define LANE_NEXT_ARROW_LOWER_TIME 10
#define LANE_NEXT_ARROW_UPPER_TIME 5

typedef struct Game {
    Lane     laneList[LANE_COUNT];
    uint8_t  life;
    uint8_t  speed;
    uint16_t score;
    uint8_t  arrowNext;
    Packet   opponent;
} Game;

#define GAME_DEFAULT_LIFE  3
#define GAME_DEFAULT_SPEED 5

static Game new_game() {
    Game game  = { 0 };
    game.life  = GAME_DEFAULT_LIFE;
    game.speed = GAME_DEFAULT_SPEED;
    game.arrowNext = (rand() % LANE_NEXT_ARROW_UPPER_TIME) + LANE_NEXT_ARROW_LOWER_TIME;

    return game;
}

typedef struct App {
    State    state;
    Game     game;
    uint8_t  address[6];
} App;

static void shift_lane_by_one(Lane* lane) {
    if (lane->arrowCount > 0) {
        for (uint8_t i = 0; i < lane->arrowCount; i++) {
            lane->arrowList[i] = lane->arrowList[i + 1];
        }

        lane->arrowCount -= 1;
    }
}

static void insert_lane_arrow(Lane* lane) {
    if (lane->arrowCount < ARROW_COUNT) {
        lane->arrowList[lane->arrowCount] = (Arrow) {
            .position = SCREEN_SIZE_X
        };
        lane->arrowCount += 1;
    }
}

static void update_adversary_packet(Game* game) {
    Packet packet = {0};
    memcpy(&packet.mac, &game->opponent.mac, sizeof(uint8_t) * 6);
    packet.score  = game->score;
    packet.life   = game->life;
    ESP_LOGI(TAG, "sending packet to other player: score=%d, life=%d", packet.score, packet.life);
    comm_send((const uint8_t*) &packet, sizeof(Packet));
}

static void handle_lane_arrow(App *app, Lane* lane) {
    if (lane->arrowCount > 0) {
        Arrow* arrow = &lane->arrowList[0];

        if (arrow->position >= -ARROW_SCALE && arrow->position <= ARROW_SCALE) {
            shift_lane_by_one(lane);
            app->game.score += 1;
            return;
        }
    }

    if (app->game.life > 0) {
        app->game.life -= 1;
        Game* game = &app->game;
        //update_adversary_packet(game);
    }
}

//================================================================

static void display_draw_arrow(uint8_t x, uint8_t y, unsigned char* spriteE, unsigned char* spriteF, bool fill) {
    if (fill) {
        display_draw_sprite(x, y, spriteF);
    } else {
        display_draw_sprite(x, y, spriteE);
    }
}

static void display_draw_rect_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill) {
    if (fill) {
        display_fill_rect(x, y, w, h, true);
    } else {
        display_draw_rect(x, y, w, h);
    }
}

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
    draw_key(4,  39, 10, pressed & (1u << BTN_LEFT));
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

void integer_to_string(int N, char *str) {
    int i = 0;

    int sign = N;

    if (N < 0)
        N = -N;

    while (N > 0) {
        str[i++] = N % 10 + '0';
        N /= 10;
    }

    if (sign < 0) {
        str[i++] = '-';
    }

    //str[i] = '\0';

    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

App app = {0};

void game_rx_callback(const uint8_t *data, size_t len, const uint8_t *src_mac) {
    switch (app.state) {
        case LOBBY:
            if (len == 4) {
                if (data[0] != 'H') { return; }
                if (data[1] != 'A') { return; }
                if (data[2] != 'C') { return; }
                if (data[3] != 'K') { return; }

                const uint8_t buffer[] = {'O', 'K'};
                comm_send((const uint8_t*) &buffer, sizeof(buffer));
                app.state   = GAME;
                app.game    = new_game();
                memcpy(&app.address, src_mac, sizeof(app.address));
                printf("lobby hand-shake HACK\n");
            }

            if (len == 2) {
                if (data[0] != 'O') { return; }
                if (data[1] != 'K') { return; }

                app.state   = GAME;
                app.game    = new_game();
                memcpy(&app.address, src_mac, sizeof(app.address));
                printf("lobby hand-shake OK\n");
            }
            break;
        case GAME:
            if (strncmp((const char*) src_mac, (const char*) &app.address, 6) == 0 && len == sizeof(Packet)) {
                memcpy(&app.game.opponent, data, sizeof(Packet));
                printf("game packet\n");
                Packet* packet = (Packet*) data;
                memcpy(&app.game.opponent, packet, sizeof(Packet));
                ESP_LOGI(TAG, "game packet from other player: score=%d, life=%d", packet->score, packet->life);
                if (app.game.opponent.life <= 3) {
                    app.state = VICTORY;
                }
            }
            break;
        default:
            break;
    }

    // TO-DO logica para pasar de LOBBY a GAME
}



void app_main(void) {
    ESP_LOGI(TAG, "Hackathon board demo starting");

    bool display_ok = display_init() == ESP_OK;
    if (!display_ok) {
        ESP_LOGE(TAG, "display init failed - continuing without screen");
    }
    comm_rx_callback_t rx_cb = game_rx_callback;
    bool comm_ok = comm_init(rx_cb) == true;

    if (!comm_ok) {
        ESP_LOGE(TAG, "comm init failed - continuing without network");
    }

    ESP_ERROR_CHECK(buttons_init());

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
    int broadcast_time_out = 0;

    while (1) {
        button_event_t ev;
        if (xQueueReceive(events, &ev, pdMS_TO_TICKS(UI_PERIOD_MS)) == pdTRUE) {
            uint32_t bRead = buttons_read();
            ESP_LOGI(TAG, "button %s %s", buttons_name(ev.id), ev.pressed ? "pressed" : "released");

            if (bRead & (1u << BTN_KEY1)) { handle_lane_arrow(&app, &app.game.laneList[0]); }
            if (bRead & (1u << BTN_KEY2)) { handle_lane_arrow(&app, &app.game.laneList[1]); }
            if (bRead & (1u << BTN_KEY3)) { handle_lane_arrow(&app, &app.game.laneList[2]); }

            if (ev.pressed && audio_ok) {
                audio_play_tone(s_button_freq[ev.id], 100);
            }
        }

        uint32_t bRead = buttons_read();

        if (display_ok) {
            switch (app.state) {
                case MAIN:
                    display_clear();
                    display_text(0,  0, "Hack Hack Revolution", false);
                    display_text(0,  8, "[B1] Jugar", false);
                    display_flush();

                    if (buttons_read() & (1u << BTN_KEY1)) {
                        app.state = LOBBY;
                    }
                    break;
                case LOBBY:
                    display_clear();
                    display_text(0,  0, "Esperando conexion...", false);
                    display_flush();

                    if (broadcast_time_out <= 0) {
                        const uint8_t buffer[] = {'H', 'A', 'C', 'K'};
                        comm_send((const uint8_t*) &buffer, sizeof(buffer));
                        broadcast_time_out = 25;
                        printf("Enviando packet broad-cast...\n");
                    }

                    broadcast_time_out -= 1;
                    break;
                case GAME:
                    if (app.game.life <= 0) {
                        app.state = FAILURE;
                        update_adversary_packet(&app.game);
                        continue;
                    }

                    char life[4]  = {0};
                    char score[4] = {0};
                    integer_to_string(app.game.score, (char*) &score);
                    integer_to_string(app.game.life, (char*) &life);

                    display_clear();
                    display_draw_number( 0, 0, app.game.score);
                    display_draw_number(96, 0, app.game.life);
                    display_draw_arrow(0, ARROW_SCALE * 1, arrowDE, arrowDF, bRead & (1u << BTN_KEY1));
                    display_draw_arrow(0, ARROW_SCALE * 2, arrowLE, arrowLF, bRead & (1u << BTN_KEY2));
                    display_draw_arrow(0, ARROW_SCALE * 3, arrowRE, arrowRF, bRead & (1u << BTN_KEY3));

                    if (app.game.arrowNext == 0) {
                        insert_lane_arrow(&app.game.laneList[rand() % LANE_COUNT]);
                        app.game.arrowNext = (rand() % LANE_NEXT_ARROW_UPPER_TIME) + LANE_NEXT_ARROW_LOWER_TIME;
                    } else {
                        app.game.arrowNext -= 1;
                    }

                    for (uint8_t i = 0; i < LANE_COUNT; i++) {
                        Lane* lane = &app.game.laneList[i];

                        for (uint8_t a = 0; a < lane->arrowCount; a++) {
                            Arrow* arrow = &lane->arrowList[a];
                            arrow->position -= app.game.speed;

                            if (i == 0) display_draw_arrow(arrow->position, ARROW_SCALE * (i + 1), arrowDE, arrowDE, false);
                            if (i == 1) display_draw_arrow(arrow->position, ARROW_SCALE * (i + 1), arrowLE, arrowLE, false);
                            if (i == 2) display_draw_arrow(arrow->position, ARROW_SCALE * (i + 1), arrowRE, arrowRE, false);

                            if (arrow->position < -ARROW_SCALE) {
                                shift_lane_by_one(lane);
                                if (app.game.life > 0) {
                                    app.game.life -= 1;
                                    Game* game = &app.game;
                                }
                            }
                        }

                    }
                    display_flush();
                    break;
                case VICTORY:
                    display_clear();
                    display_text(0,  0, "Ya-Hoo!!!!!!!!!!!!!!", false);
                    display_text(0,  8, "Puntaje:", false);
                    display_draw_number(0, 16, app.game.score);
                    display_text(0, 32, "Pulsa [RESET].", false);
                    display_flush();
                    break;
                case FAILURE:
                    display_clear();
                    display_text(0,  0, "#owned",   false);
                    display_text(0,  8, "Puntaje:", false);
                    display_draw_number(0, 16, app.game.score);
                    display_text(0, 32, "Pulsa [RESET].", false);
                    display_flush();
                    break;
            }

            buttons_read();
        }
    }
}
