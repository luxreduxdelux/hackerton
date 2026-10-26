#pragma once

//================================================================

#define UI_PERIOD_MS 50

//================================================================

typedef enum {
    MAIN,
    HELP,
    LOBBY,
    GAME,
    VICTORY,
    FAILURE,
} State;

//================================================================

typedef struct Packet {
    uint8_t  mac[6];
    uint16_t score;
    uint8_t  life;
} Packet;

//================================================================

typedef struct Arrow {
    int16_t position;
} Arrow;

#define ARROW_COUNT 32
#define ARROW_SCALE 16

//================================================================

typedef struct Lane {
    Arrow   arrowList[ARROW_COUNT];
    uint8_t arrowCount;
} Lane;

#define LANE_COUNT 3
#define LANE_NEXT_ARROW_LOWER_TIME 10
#define LANE_NEXT_ARROW_UPPER_TIME 5

//================================================================

typedef struct Game {
    Lane     laneList[LANE_COUNT];
    uint8_t  life;
    uint16_t score;
    uint8_t  combo;
    uint8_t  arrowNext;
    Packet   opponent;
} Game;

#define GAME_DEFAULT_LIFE  3
#define GAME_DEFAULT_SPEED 5

//================================================================

typedef struct App {
    State    state;
    Game     game;
    uint8_t  address[6];
} App;