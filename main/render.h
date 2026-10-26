#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "font.h"

#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Rect;

// A block of native RGB565 pixels that covers a screen rectangle.
typedef struct {
    uint16_t *pixels;
    Rect area;
} Canvas;

typedef enum {
    SHAPE_ELLIPSE,  // a = cx, b = cy, c = rx, d = ry
    SHAPE_ARC_UP,   // upper half ring with round caps: a = cx, b = cy, c = radius, d = thickness
    SHAPE_ARC_DOWN, // lower half ring with round caps: a = cx, b = cy, c = radius, d = thickness
    SHAPE_CAPSULE,  // a, b = start; c, d = end; e = radius
    SHAPE_TEXT,     // a = pen x, b = baseline y, text drawn with font_nunito
} ShapeKind;

typedef struct {
    ShapeKind kind;
    uint16_t color;
    uint8_t opacity;
    float a;
    float b;
    float c;
    float d;
    float e;
    const char *text;
} Shape;

#define SHAPE_LIST_CAPACITY 12

typedef struct {
    Shape items[SHAPE_LIST_CAPACITY];
    int count;
} ShapeList;

ShapeList shape_list_add(ShapeList list, Shape shape);
bool shape_equal(const Shape *a, const Shape *b);
Rect shape_bounds(const Shape *shape);

Rect rect_union(Rect a, Rect b);
Rect rect_intersect(Rect a, Rect b);
bool rect_empty(Rect r);

int text_width(const Font *font, const char *text);

void canvas_fill(Canvas canvas, uint16_t color);
void canvas_draw_shape(Canvas canvas, const Shape *shape);
void canvas_draw_text(Canvas canvas, const Font *font, int x, int baseline, const char *text, uint16_t color,
                      uint8_t opacity);
