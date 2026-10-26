#pragma once

#include <stdint.h>

// Code the chat app sends in place of any heart emoji.
#define FONT_HEART_CODE 127

// One 4-bit alpha glyph. Offsets are relative to the pen position on the
// baseline; y_offset is negative above the baseline.
typedef struct {
    uint32_t bitmap_offset;
    uint8_t width;
    uint8_t height;
    int8_t x_offset;
    int8_t y_offset;
    uint8_t advance;
} FontGlyph;

typedef struct {
    const uint8_t *bitmap;
    const FontGlyph *glyphs;
    uint8_t first_code;
    uint8_t last_code;
    uint8_t ascent;
    uint8_t line_height;
} Font;

extern const Font font_nunito;
