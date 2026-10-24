#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define DISPLAY_FONT_W  6
#define DISPLAY_FONT_H  8

extern unsigned char arrowDE[];
extern unsigned char arrowLE[];
extern unsigned char arrowRE[];
extern unsigned char arrowDF[];
extern unsigned char arrowLF[];
extern unsigned char arrowRF[];
extern unsigned char heartF[];

esp_err_t display_init(void);

void display_clear(void);

void display_set_pixel(int x, int y, bool on);
void display_fill_rect(int x, int y, int w, int h, bool on);
void display_draw_rect(int x, int y, int w, int h);
void display_draw_sprite(int x, int y, unsigned char* sprite);
void display_draw_number(int x, int y, int num);

void display_text(int x, int y, const char *str, bool invert);

esp_err_t display_flush(void);
