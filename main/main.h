#pragma once

//================================================================

#define UI_PERIOD_MS 50

//================================================================

typedef enum {
    MAIN,
    HELP_1,
    HELP_2,
    LOBBY,
    GAME,
    VICTORY,
    FAILURE,
    SCORE_ENTER,
    SCORE_BOARD,
} State;

//================================================================

typedef struct Packet {
    uint8_t mac[6];
    uint8_t life;
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
    uint8_t  shake;
    uint8_t  arrowNext;
    Packet   opponent;
} Game;

#define GAME_LIMIT_LIFE    3
#define GAME_LIMIT_SCORE   9999
#define GAME_LIMIT_COMBO   99
#define GAME_SPEED         5
#define GAME_ADD_LIFE      20
#define GAME_ADD_COMBO     10

//================================================================

typedef struct ScoreEntry {
    char     name[3];
    uint16_t score;
} ScoreEntry;

#define SCORE_ENTRY_COUNT 6

//================================================================

typedef struct App {
    State      state;
    Game       game;
    uint8_t    broadcast_retry_time;
    uint8_t    address[6];
    ScoreEntry scoreList[SCORE_ENTRY_COUNT];
    uint8_t    scoreCharacter;
    uint8_t    scoreIndex;
} App;