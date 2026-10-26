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
    game.life  = GAME_LIMIT_LIFE;
    game.arrowNext = (rand() % LANE_NEXT_ARROW_UPPER_TIME) + LANE_NEXT_ARROW_LOWER_TIME;

    return game;
}

static void take_damage(Game* game) {
    if (game->life > 0) {
        game->life -= 1;
        game->combo = 0;
        game->shake = 5;
    }
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
    packet.life = game->life;
    ESP_LOGI(TAG, "sending packet to other player: life=%d", packet.life);
    network_send((const uint8_t*) &packet, sizeof(Packet));
}

static void handle_lane_arrow(App *app, Lane* lane) {
    if (lane->arrowCount > 0) {
        Arrow* arrow = &lane->arrowList[0];

        if (arrow->position >= -ARROW_SCALE && arrow->position <= ARROW_SCALE) {
            shift_lane_by_one(lane);

            if (app->game.score < GAME_LIMIT_SCORE) {
                app->game.score += 1 * ((app->game.combo / GAME_ADD_COMBO) + 1);
            }

            if (app->game.combo < GAME_LIMIT_COMBO) {
                app->game.combo += 1;

                if (app->game.combo % GAME_ADD_LIFE == 0) {
                    if (app->game.life < GAME_LIMIT_LIFE) {
                        app->game.life += 1;
                    }
                }
            }
            return;
        }
    }

    take_damage(&app->game);
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
            }

            if (len == 2) {
                if (data[0] != 'O') { return; }
                if (data[1] != 'K') { return; }

                app.state   = GAME;
                app.game    = new_game();
                memcpy(&app.address, src_mac, sizeof(app.address));
            }
            break;
        case GAME:
            if (strncmp((const char*) src_mac, (const char*) &app.address, 6) == 0 && len == sizeof(Packet)) {
                memcpy(&app.game.opponent, data, sizeof(Packet));
                Packet* packet = (Packet*) data;
                memcpy(&app.game.opponent, packet, sizeof(Packet));

                if (app.game.opponent.life <= 3) {
                    app.state = VICTORY;
                }
            }
            break;
        default:
            break;
    }
}

static int8_t can_enter_score(App* app) {
    for (uint8_t i = 0; i < SCORE_ENTRY_COUNT; i++) {
        if (app->game.score > app->scoreList[i].score) {
            return i;
        }
    }

    return -1;
}

static void shift_score_by_one(App* app, uint8_t index) {
    for (uint8_t i = SCORE_ENTRY_COUNT - 1; i > index; i--) {
        app->scoreList[i] = app->scoreList[i - 1];
    }
}

//================================================================

void app_main(void) {
    display_initialize();
    network_initialize(game_rx_callback);
    button_initialize();

    QueueHandle_t event_queue = button_event_queue();

    //================================

    while (true) {
        button_event_t event = {0};
        xQueueReceive(event_queue, &event, pdMS_TO_TICKS(UI_PERIOD_MS));
        uint32_t button_state = button_read();

        switch (app.state) {
            case MAIN:
                display_clear();
                display_draw_sprite((BOARD_LCD_W - comCom.size_x) / 2, 0, &comCom);
                display_text((BOARD_LCD_W / 2) - (5 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 2), "[B4] Jugar", false);
                display_text((BOARD_LCD_W / 2) - (5 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B1] Ayuda", false);
                display_flush();

                if (event.id == BTN_KEY4 && event.pressed == false) {
                    app.state   = GAME;
                    app.game    = new_game();
                    //app.state = LOBBY;
                }
                if (event.id == BTN_KEY1 && event.pressed == false) {
                    app.state = HELP_1;
                }

                rand();
                break;
            case HELP_1:
                display_clear();
                display_text((BOARD_LCD_W / 2) - (9  * DISPLAY_FONT_W), DISPLAY_FONT_H * 0, "[B1] Flecha Abajo",    false);
                display_text((BOARD_LCD_W / 2) - (8  * DISPLAY_FONT_W), DISPLAY_FONT_H * 1, "[B2] Flecha Izq.",     false);
                display_text((BOARD_LCD_W / 2) - (8  * DISPLAY_FONT_W), DISPLAY_FONT_H * 2, "[B3] Flecha Der.",     false);
                display_text((BOARD_LCD_W / 2) - (10 * DISPLAY_FONT_W), DISPLAY_FONT_H * 3, "10 flechas seguidas:", false);
                display_text((BOARD_LCD_W / 2) - (8  * DISPLAY_FONT_W), DISPLAY_FONT_H * 4, "subis el combo.",      false);
                display_text((BOARD_LCD_W / 2) - (10 * DISPLAY_FONT_W), DISPLAY_FONT_H * 5, "20 flechas seguidas:", false);
                display_text((BOARD_LCD_W / 2) - (7  * DISPLAY_FONT_W), DISPLAY_FONT_H * 6, "subis la vida.",       false);
                display_text((BOARD_LCD_W / 2) - (7  * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B1] Siguiente", false);
                display_flush();

                if (event.id == BTN_KEY1 && event.pressed == false) {
                    app.state = HELP_2;
                }
                break;
            case HELP_2:
                display_clear();
                display_text((BOARD_LCD_W / 2) - (9 * DISPLAY_FONT_W), DISPLAY_FONT_H * 0, "CC Revolution por",  false);
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), DISPLAY_FONT_H * 1, "@santuchoagus",      false);
                display_text((BOARD_LCD_W / 2) - (8 * DISPLAY_FONT_W), DISPLAY_FONT_H * 2, "@EzequielTrigo",     false);
                display_text((BOARD_LCD_W / 2) - (4 * DISPLAY_FONT_W), DISPLAY_FONT_H * 3, "@blatth",            false);
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), DISPLAY_FONT_H * 4, "@luxreduxdelux",     false);
                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B1] Siguiente", false);
                display_flush();

                if (event.id == BTN_KEY1 && event.pressed == false) {
                    app.state = MAIN;
                }
                break;
            case LOBBY:
                display_clear();
                display_draw_sprite((BOARD_LCD_W - lobby.size_x) / 2, 0, &lobby);
                display_text((BOARD_LCD_W / 2) - (10 * DISPLAY_FONT_W), BOARD_LCD_H - DISPLAY_FONT_H, "Esperando conexion...", false);
                display_flush();

                if (app.broadcast_retry_time <= 0) {
                    const uint8_t buffer[] = {'H', 'A', 'C', 'K'};
                    network_send((const uint8_t*) &buffer, sizeof(buffer));
                    app.broadcast_retry_time = 25;
                }

                app.broadcast_retry_time -= 1;
                break;
            case GAME:
                if (app.game.life <= 0) {
                    app.state = FAILURE;
                    update_adversary_packet(&app.game);
                    continue;
                }

                int8_t shake_x = app.game.shake > 0 ? (rand() % (app.game.shake + app.game.shake + 1)) - app.game.shake : 0;
                int8_t shake_y = app.game.shake > 0 ? (rand() % (app.game.shake + app.game.shake + 1)) - app.game.shake : 0;

                display_clear();
                display_draw_number(shake_x + 0,   shake_y, app.game.score);

                if (app.game.combo >= GAME_ADD_COMBO) {
                    display_draw_sprite(shake_x + 64, shake_y, &combo);
                    display_draw_number(shake_x + 80, shake_y, (app.game.combo / GAME_ADD_COMBO) + 1);
                }

                display_draw_sprite(shake_x + 96,  shake_y, &heart);
                display_draw_number(shake_x + 112, shake_y, app.game.life);
                display_draw_arrow(shake_x, shake_y + (ARROW_SCALE * 1), &arrowDE, &arrowDF, button_state & (1u << BTN_KEY1));
                display_draw_arrow(shake_x, shake_y + (ARROW_SCALE * 2), &arrowLE, &arrowLF, button_state & (1u << BTN_KEY2));
                display_draw_arrow(shake_x, shake_y + (ARROW_SCALE * 3), &arrowRE, &arrowRF, button_state & (1u << BTN_KEY3));

                if (app.game.arrowNext == 0) {
                    insert_lane_arrow(&app.game.laneList[rand() % LANE_COUNT]);
                    app.game.arrowNext = (rand() % LANE_NEXT_ARROW_UPPER_TIME) + LANE_NEXT_ARROW_LOWER_TIME;
                } else {
                    app.game.arrowNext -= 1;
                }

                if (app.game.shake > 0) {
                    app.game.shake -= 1;
                }

                if (event.id == BTN_KEY1 && event.pressed) { handle_lane_arrow(&app, &app.game.laneList[0]); }
                if (event.id == BTN_KEY2 && event.pressed) { handle_lane_arrow(&app, &app.game.laneList[1]); }
                if (event.id == BTN_KEY3 && event.pressed) { handle_lane_arrow(&app, &app.game.laneList[2]); }

                for (uint8_t i = 0; i < LANE_COUNT; i++) {
                    Lane* lane = &app.game.laneList[i];

                    for (uint8_t a = 0; a < lane->arrowCount; a++) {
                        Arrow* arrow = &lane->arrowList[a];
                        arrow->position -= GAME_SPEED + (app.game.combo / GAME_ADD_COMBO);

                        if (i == 0) display_draw_arrow(shake_x + arrow->position, shake_y + ARROW_SCALE * 1, &arrowDE, &arrowDE, false);
                        if (i == 1) display_draw_arrow(shake_x + arrow->position, shake_y + ARROW_SCALE * 2, &arrowLE, &arrowLE, false);
                        if (i == 2) display_draw_arrow(shake_x + arrow->position, shake_y + ARROW_SCALE * 3, &arrowRE, &arrowRE, false);

                        if (arrow->position < -ARROW_SCALE) {
                            shift_lane_by_one(lane);
                            take_damage(&app.game);
                        }
                    }
                }
                display_flush();
                break;
            case VICTORY:
                char score_text[13];
                char score_size = snprintf((char*) &score_text, sizeof(score_text), "Puntaje: %d", app.game.score) / 2;

                display_clear();
                display_draw_sprite((BOARD_LCD_W - victory.size_x) / 2, 0, &victory);
                display_text((BOARD_LCD_W / 2) - (score_size * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 2), score_text, false);
                display_text((BOARD_LCD_W / 2) - (5 * DISPLAY_FONT_W),          BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B4] Menu", false);
                display_flush();

                if (event.id == BTN_KEY4 && event.pressed == false) {
                    int8_t canEnter = can_enter_score(&app);

                    if (canEnter >= 0) {
                        app.state = SCORE_ENTER;
                        shift_score_by_one(&app, canEnter);
                        app.scoreList[canEnter] = (ScoreEntry) {0};
                        app.scoreCharacter = 65;
                        app.scoreIndex     = 0;
                    } else {
                        app.state = SCORE_BOARD;
                    }
                }
                break;
            case FAILURE:
                score_size = snprintf((char*) &score_text, sizeof(score_text), "Puntaje: %d", app.game.score) / 2;

                display_clear();
                display_draw_sprite((BOARD_LCD_W - failure.size_x) / 2, 0, &failure);
                display_text((BOARD_LCD_W / 2) - (score_size * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 2), score_text, false);
                display_text((BOARD_LCD_W / 2) - (5 * DISPLAY_FONT_W),          BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B4] Menu", false);
                display_flush();

                if (event.id == BTN_KEY4 && event.pressed == false) {
                    int8_t canEnter = can_enter_score(&app);

                    if (canEnter >= 0) {
                        app.state = SCORE_ENTER;
                        shift_score_by_one(&app, canEnter);
                        app.scoreList[canEnter] = (ScoreEntry) {0};
                        app.scoreCharacter = 65;
                        app.scoreIndex     = 0;
                    } else {
                        app.state = SCORE_BOARD;
                    }
                }
                break;
            case SCORE_ENTER:
                char score_name[9];
                ScoreEntry* entry = &app.scoreList[can_enter_score(&app)];

                if (app.scoreIndex == 0) snprintf((char*) &score_name, sizeof(score_name), "[%c]%c%c", app.scoreCharacter, entry->name[1], entry->name[2]);
                if (app.scoreIndex == 1) snprintf((char*) &score_name, sizeof(score_name), "%c[%c]%c", entry->name[0], app.scoreCharacter, entry->name[2]);
                if (app.scoreIndex == 2) snprintf((char*) &score_name, sizeof(score_name), "%c%c[%c]", entry->name[0], entry->name[1], app.scoreCharacter);

                display_clear();
                display_text((BOARD_LCD_W / 2) - (9  * DISPLAY_FONT_W), (DISPLAY_FONT_H * 0), "Ingresa tu nombre:",                  false);
                display_text((BOARD_LCD_W / 2) - (5  * DISPLAY_FONT_W), (DISPLAY_FONT_H * 1), score_name,                            false);
                display_text((BOARD_LCD_W / 2) - (10 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 2), "[B1/B2] Cambiar Letra", false);
                display_text((BOARD_LCD_W / 2) - (7  * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B4] Siguiente",        false);
                display_flush();

                if (event.id == BTN_KEY1 && event.pressed == false) {
                    app.scoreCharacter -= 1;
                    if (app.scoreCharacter < 65) app.scoreCharacter = 90;
                }

                if (event.id == BTN_KEY2 && event.pressed == false) {
                    app.scoreCharacter += 1;
                    if (app.scoreCharacter > 90) app.scoreCharacter = 65;
                }

                if (event.id == BTN_KEY4 && event.pressed == false) {
                    entry->name[app.scoreIndex] = app.scoreCharacter;
                    app.scoreCharacter = 65;
                    app.scoreIndex    += 1;

                    if (app.scoreIndex > 2) {
                        app.state = SCORE_BOARD;
                        entry->score = app.game.score;
                    }
                }
                break;
            case SCORE_BOARD:
                char score_entry[11];
                display_clear();
                display_text((BOARD_LCD_W / 2) - (9 * DISPLAY_FONT_W), 0, "Ultra-ComComistas", false);

                for (uint8_t i = 0; i < SCORE_ENTRY_COUNT; i++) {
                    if (app.scoreList[i].name[0] == 0) {
                        display_text((BOARD_LCD_W / 2) - (5 * DISPLAY_FONT_W), DISPLAY_FONT_H * (i + 1), "< vacio >", false);
                    } else {
                        score_size = snprintf((char*) &score_entry, sizeof(score_entry), "%s - %d", app.scoreList[i].name, app.scoreList[i].score) / 2;
                        display_text((BOARD_LCD_W / 2) - (score_size * DISPLAY_FONT_W), DISPLAY_FONT_H * (i + 1), score_entry, false);
                    }
                }

                display_text((BOARD_LCD_W / 2) - (7 * DISPLAY_FONT_W), BOARD_LCD_H - (DISPLAY_FONT_H * 1), "[B4] Siguiente", false);
                display_flush();

                if (event.id == BTN_KEY4 && event.pressed == false) {
                    app.state = MAIN;
                }
                break;
        }
    }
}