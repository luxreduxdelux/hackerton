#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

//================================================================

#include "board.h"
#include "button.h"
#include "display.h"
#include "network.h"
#include "main.h"

//================================================================

static const char *TAG = "main";

//================================================================

App app = {0};

static Game new_game() {
    Game game  = { 0 };
    game.life  = GAME_DEFAULT_LIFE;
    game.arrowNext = (rand() % LANE_NEXT_ARROW_UPPER_TIME) + LANE_NEXT_ARROW_LOWER_TIME;

    return game;
}

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
            .position = BOARD_LCD_W
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
    network_send((const uint8_t*) &packet, sizeof(Packet));
}

static void handle_lane_arrow(App *app, Lane* lane) {
    if (lane->arrowCount > 0) {
        Arrow* arrow = &lane->arrowList[0];

        if (arrow->position >= -ARROW_SCALE && arrow->position <= ARROW_SCALE) {
            shift_lane_by_one(lane);

            if (app->game.score < 10000 - 1) {
                app->game.score += 1 * ((app->game.combo / 10) + 1);
            }

            if (app->game.combo < 100 - 1) {
                app->game.combo += 1;

                if (app->game.combo % 20 == 0) {
                    if (app->game.life < 3) {
                        app->game.life += 1;
                    }
                }
            }
            return;
        }
    }

    if (app->game.life > 0) {
        app->game.life -= 1;
        app->game.combo = 0;
    }
}

static void display_draw_arrow(uint8_t x, uint8_t y, const Sprite* spriteE, const Sprite* spriteF, bool fill) {
    if (fill) {
        display_draw_sprite(x, y, spriteF);
    } else {
        display_draw_sprite(x, y, spriteE);
    }
}

static void game_rx_callback(const uint8_t *data, size_t len, const uint8_t *src_mac) {
    switch (app.state) {
        case LOBBY:
            if (len == 4) {
                if (data[0] != 'H') { return; }
                if (data[1] != 'A') { return; }
                if (data[2] != 'C') { return; }
                if (data[3] != 'K') { return; }

                const uint8_t buffer[] = {'O', 'K'};
                network_send((const uint8_t*) &buffer, sizeof(buffer));
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
}

//================================================================

void app_main(void) {
    display_initialize();
    network_initialize(game_rx_callback);
    button_initialize();

    QueueHandle_t events = button_event_queue();
    int broadcast_time_out = 0;

    //================================

    while (true) {
        button_event_t ev;
        if (xQueueReceive(events, &ev, pdMS_TO_TICKS(UI_PERIOD_MS)) == pdTRUE) {
            uint32_t bRead = button_read();

            if (app.state == GAME) {
                if (bRead & (1u << BTN_KEY1)) { handle_lane_arrow(&app, &app.game.laneList[0]); }
                if (bRead & (1u << BTN_KEY2)) { handle_lane_arrow(&app, &app.game.laneList[1]); }
                if (bRead & (1u << BTN_KEY3)) { handle_lane_arrow(&app, &app.game.laneList[2]); }
            }
        }

        uint32_t bRead = button_read();

        switch (app.state) {
            case MAIN:
                display_clear();
                display_draw_sprite((BOARD_LCD_W - comCom.size_x) / 2, 0, &comCom);
                display_text((BOARD_LCD_W / 2) - (5 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 2), "[B1] Jugar", false);
                display_text((BOARD_LCD_W / 2) - (5 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B2] Ayuda", false);
                display_flush();

                if (bRead & (1u << BTN_KEY1)) {
                    app.state   = GAME;
                    app.game    = new_game();
                    //app.state = LOBBY;
                }
                if (bRead & (1u << BTN_KEY2)) {
                    app.state = HELP;
                }
                break;
            case HELP:
                display_clear();
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), DISPLAY_FONT_H * 0, "@santuchoagus",  false);
                display_text((BOARD_LCD_W / 2) - (8 * DISPLAY_FONT_W), DISPLAY_FONT_H * 1, "@EzequielTrigo", false);
                display_text((BOARD_LCD_W / 2) - (4 * DISPLAY_FONT_W), DISPLAY_FONT_H * 2, "@blatth",        false);
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), DISPLAY_FONT_H * 3, "@luxreduxdelux", false);
                display_text((BOARD_LCD_W / 2) - (5 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B1] Volver", false);
                display_flush();

                if (bRead & (1u << BTN_KEY1)) {
                    app.state = MAIN;
                }
                break;
            case LOBBY:
                display_clear();
                display_draw_sprite((BOARD_LCD_W - lobby.size_x) / 2, 0, &lobby);
                display_text((BOARD_LCD_W / 2) - (10 * DISPLAY_FONT_W), BOARD_LCD_H - DISPLAY_FONT_H, "Esperando conexion...", false);
                display_flush();

                if (broadcast_time_out <= 0) {
                    const uint8_t buffer[] = {'H', 'A', 'C', 'K'};
                    network_send((const uint8_t*) &buffer, sizeof(buffer));
                    broadcast_time_out = 25;
                }

                broadcast_time_out -= 1;
                break;
            case GAME:
                if (app.game.life <= 0) {
                    app.state = FAILURE;
                    update_adversary_packet(&app.game);
                    continue;
                }

                display_clear();
                display_draw_number( 0, 0, app.game.score);
                display_draw_sprite( 64, 0, &heartF);
                display_draw_number( 80, 0, app.game.life);
                display_draw_sprite( 96, 0, &heartF);
                display_draw_number(112, 0, app.game.combo / 10);

                display_draw_arrow(0, ARROW_SCALE * 1, &arrowDE, &arrowDF, bRead & (1u << BTN_KEY1));
                display_draw_arrow(0, ARROW_SCALE * 2, &arrowLE, &arrowLF, bRead & (1u << BTN_KEY2));
                display_draw_arrow(0, ARROW_SCALE * 3, &arrowRE, &arrowRF, bRead & (1u << BTN_KEY3));

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
                        arrow->position -= GAME_DEFAULT_SPEED + (app.game.combo / 10);

                        if (i == 0) display_draw_arrow(arrow->position, ARROW_SCALE * 1, &arrowDE, &arrowDE, false);
                        if (i == 1) display_draw_arrow(arrow->position, ARROW_SCALE * 2, &arrowLE, &arrowLE, false);
                        if (i == 2) display_draw_arrow(arrow->position, ARROW_SCALE * 3, &arrowRE, &arrowRE, false);

                        if (arrow->position < -ARROW_SCALE) {
                            shift_lane_by_one(lane);

                            if (app.game.life > 0) {
                                app.game.life -= 1;
                                app.game.combo = 0;
                            }
                        }
                    }
                }
                display_flush();
                break;
            case VICTORY:
                display_clear();
                display_draw_sprite((BOARD_LCD_W - victory.size_x) / 2, 0, &victory);
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 2), "Puntaje: .....", false);
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "Pulsa [RESET].", false);
                display_flush();
                break;
            case FAILURE:
                //display_draw_number(0, 16, app.game.score);
                display_clear();
                display_draw_sprite((BOARD_LCD_W - failure.size_x) / 2, 0, &failure);
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 2), "Puntaje: ....", false);
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "Pulsa [RESET].", false);
                display_flush();
                break;
        }
    }
}