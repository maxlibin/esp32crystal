#pragma once

#include <stdint.h>

#define LCD_WIDTH 320
#define LCD_HEIGHT 240

// Largest block lcd_push accepts in one call (a full-width 40-row strip).
#define LCD_MAX_PUSH_PIXELS (LCD_WIDTH * 40)

void lcd_init(int brightness_percent);

// Sends a w*h block of native RGB565 pixels to the panel at (x, y).
// The buffer must be DMA-capable; it is byte-swapped in place for the panel,
// so its contents are consumed by the call.
void lcd_push(int x, int y, int w, int h, uint16_t *pixels);
