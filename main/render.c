#include "render.h"

#include <math.h>
#include <stdlib.h>

#include "esp_log.h"

static const char *TAG = "CRYSTAL_RENDER";

// Extra pixels around shape bounds so anti-aliased edges are never clipped.
#define BOUNDS_MARGIN 2

ShapeList shape_list_add(ShapeList list, Shape shape)
{
    if (list.count >= SHAPE_LIST_CAPACITY) {
        ESP_LOGE(TAG, "Shape list full: capacity=%d", SHAPE_LIST_CAPACITY);
        abort();
    }
    list.items[list.count++] = shape;
    return list;
}

bool shape_equal(const Shape *a, const Shape *b)
{
    return a->kind == b->kind && a->color == b->color && a->opacity == b->opacity && a->a == b->a &&
           a->b == b->b && a->c == b->c && a->d == b->d && a->e == b->e && a->text == b->text;
}

static Rect rect_from_extent(float x0, float y0, float x1, float y1)
{
    int left = (int)floorf(x0) - BOUNDS_MARGIN;
    int top = (int)floorf(y0) - BOUNDS_MARGIN;
    int right = (int)ceilf(x1) + BOUNDS_MARGIN;
    int bottom = (int)ceilf(y1) + BOUNDS_MARGIN;
    return (Rect){left, top, right - left, bottom - top};
}

Rect shape_bounds(const Shape *s)
{
    switch (s->kind) {
    case SHAPE_ELLIPSE:
        return rect_from_extent(s->a - s->c, s->b - s->d, s->a + s->c, s->b + s->d);
    case SHAPE_ARC_UP:
        return rect_from_extent(s->a - s->c - s->d / 2, s->b - s->c - s->d / 2, s->a + s->c + s->d / 2,
                                s->b + s->d / 2);
    case SHAPE_ARC_DOWN:
        return rect_from_extent(s->a - s->c - s->d / 2, s->b - s->d / 2, s->a + s->c + s->d / 2,
                                s->b + s->c + s->d / 2);
    case SHAPE_CAPSULE:
        return rect_from_extent(fminf(s->a, s->c) - s->e, fminf(s->b, s->d) - s->e, fmaxf(s->a, s->c) + s->e,
                                fmaxf(s->b, s->d) + s->e);
    case SHAPE_TEXT:
        return rect_from_extent(s->a, s->b - font_nunito.ascent, s->a + text_width(&font_nunito, s->text),
                                s->b - font_nunito.ascent + font_nunito.line_height);
    }
    ESP_LOGE(TAG, "Unknown shape kind: kind=%d", (int)s->kind);
    abort();
}

Rect rect_union(Rect a, Rect b)
{
    if (rect_empty(a)) return b;
    if (rect_empty(b)) return a;
    int left = a.x < b.x ? a.x : b.x;
    int top = a.y < b.y ? a.y : b.y;
    int right = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w;
    int bottom = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    return (Rect){left, top, right - left, bottom - top};
}

Rect rect_intersect(Rect a, Rect b)
{
    int left = a.x > b.x ? a.x : b.x;
    int top = a.y > b.y ? a.y : b.y;
    int right = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int bottom = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    if (right <= left || bottom <= top) return (Rect){0, 0, 0, 0};
    return (Rect){left, top, right - left, bottom - top};
}

bool rect_empty(Rect r)
{
    return r.w <= 0 || r.h <= 0;
}

static uint16_t blend(uint16_t bg, uint16_t fg, int alpha)
{
    int inverse = 255 - alpha;
    int r = (((fg >> 11) & 0x1F) * alpha + ((bg >> 11) & 0x1F) * inverse) / 255;
    int g = (((fg >> 5) & 0x3F) * alpha + ((bg >> 5) & 0x3F) * inverse) / 255;
    int b = ((fg & 0x1F) * alpha + (bg & 0x1F) * inverse) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static float clamp01(float value)
{
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

// Signed distance (pixels, negative inside) from a point to a shape outline.
static float shape_distance(const Shape *s, float px, float py)
{
    switch (s->kind) {
    case SHAPE_ELLIPSE: {
        float dx = (px - s->a) / s->c;
        float dy = (py - s->b) / s->d;
        return (sqrtf(dx * dx + dy * dy) - 1.0f) * fminf(s->c, s->d);
    }
    case SHAPE_ARC_UP:
    case SHAPE_ARC_DOWN: {
        bool on_half = s->kind == SHAPE_ARC_UP ? py <= s->b : py >= s->b;
        float ring = on_half ? fabsf(hypotf(px - s->a, py - s->b) - s->c) - s->d / 2 : INFINITY;
        float left_cap = hypotf(px - (s->a - s->c), py - s->b) - s->d / 2;
        float right_cap = hypotf(px - (s->a + s->c), py - s->b) - s->d / 2;
        return fminf(ring, fminf(left_cap, right_cap));
    }
    case SHAPE_CAPSULE: {
        float vx = s->c - s->a;
        float vy = s->d - s->b;
        float length_sq = vx * vx + vy * vy;
        float t = length_sq > 0.0f ? clamp01(((px - s->a) * vx + (py - s->b) * vy) / length_sq) : 0.0f;
        return hypotf(px - (s->a + t * vx), py - (s->b + t * vy)) - s->e;
    }
    case SHAPE_TEXT:
        break;
    }
    ESP_LOGE(TAG, "Shape kind has no distance field: kind=%d", (int)s->kind);
    abort();
}

int text_width(const Font *font, const char *text)
{
    int width = 0;
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        width += font->glyphs[*c - font->first_code].advance;
    }
    return width;
}

void canvas_fill(Canvas canvas, uint16_t color)
{
    for (int i = 0; i < canvas.area.w * canvas.area.h; i++) {
        canvas.pixels[i] = color;
    }
}

void canvas_draw_shape(Canvas canvas, const Shape *shape)
{
    if (shape->kind == SHAPE_TEXT) {
        canvas_draw_text(canvas, &font_nunito, (int)shape->a, (int)shape->b, shape->text, shape->color,
                         shape->opacity);
        return;
    }
    Rect clip = rect_intersect(shape_bounds(shape), canvas.area);
    for (int y = clip.y; y < clip.y + clip.h; y++) {
        uint16_t *row = canvas.pixels + (y - canvas.area.y) * canvas.area.w;
        for (int x = clip.x; x < clip.x + clip.w; x++) {
            float coverage = clamp01(0.5f - shape_distance(shape, x + 0.5f, y + 0.5f));
            int alpha = (int)(coverage * shape->opacity);
            if (alpha > 0) {
                row[x - canvas.area.x] = blend(row[x - canvas.area.x], shape->color, alpha);
            }
        }
    }
}

void canvas_draw_text(Canvas canvas, const Font *font, int x, int baseline, const char *text, uint16_t color,
                      uint8_t opacity)
{
    int pen = x;
    int canvas_right = canvas.area.x + canvas.area.w;
    for (const unsigned char *c = (const unsigned char *)text; *c && pen < canvas_right; c++) {
        const FontGlyph *glyph = &font->glyphs[*c - font->first_code];
        Rect glyph_area = {pen + glyph->x_offset, baseline + glyph->y_offset, glyph->width, glyph->height};
        Rect clip = rect_intersect(glyph_area, canvas.area);
        for (int y = clip.y; y < clip.y + clip.h; y++) {
            for (int gx = clip.x; gx < clip.x + clip.w; gx++) {
                int index = (y - glyph_area.y) * glyph->width + (gx - glyph_area.x);
                uint8_t packed = font->bitmap[glyph->bitmap_offset + index / 2];
                int nibble = index % 2 == 0 ? packed >> 4 : packed & 0x0F;
                int alpha = nibble * 17 * opacity / 255;
                if (alpha > 0) {
                    uint16_t *pixel = &canvas.pixels[(y - canvas.area.y) * canvas.area.w + (gx - canvas.area.x)];
                    *pixel = blend(*pixel, color, alpha);
                }
            }
        }
        pen += glyph->advance;
    }
}
