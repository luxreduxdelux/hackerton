#pragma once

//================================================================

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

//================================================================

#define DISPLAY_FONT_W  6
#define DISPLAY_FONT_H  8

//================================================================

typedef struct Sprite {
    uint8_t* data;
    uint8_t size_x;
    uint8_t size_y;
} Sprite;

//================================================================

extern Sprite arrowDE;
extern Sprite arrowLE;
extern Sprite arrowRE;
extern Sprite arrowDF;
extern Sprite arrowLF;
extern Sprite arrowRF;
extern Sprite heartF;
extern Sprite number[10];
extern Sprite comCom;
extern Sprite victory;
extern Sprite failure;
extern Sprite lobby;

//================================================================

esp_err_t display_initialize(void);
void display_clear(void);
void display_set_pixel(int x, int y, bool on);
void display_draw_sprite(int x, int y, const Sprite* sprite);
void display_draw_number(int x, int y, int num);
void display_text(int x, int y, const char *str, bool invert);
esp_err_t display_flush(void);