#include "face.h"

#include "lcd.h"

#define COLOR_FACE_BG RGB565(0xFF, 0xE9, 0xF1)
#define COLOR_EYE RGB565(0x3B, 0x2A, 0x3A)
#define COLOR_SHINE RGB565(0xFF, 0xFF, 0xFF)
#define COLOR_BLUSH RGB565(0xFF, 0x9F, 0xBF)
#define COLOR_MOUTH RGB565(0x3B, 0x2A, 0x3A)
#define COLOR_ZZZ RGB565(0xB0, 0x6A, 0x8A)

#define LEFT_EYE_X 114.0f
#define RIGHT_EYE_X 206.0f
#define EYE_Y 100.0f
#define EYE_RX 22.0f
#define EYE_RY 28.0f
#define MOUTH_X 160.0f
#define MOUTH_Y 142.0f

#define BLINK_MS 160
#define GLANCE_MS 1400
#define GLANCE_EASE_MS 220
#define GLANCE_PIXELS 10
#define TALK_PERIOD_MS 280
#define ZZZ_PERIOD_MS 3000

static const Rect face_area = {0, 0, LCD_WIDTH, FACE_HEIGHT};

typedef struct {
    float look_x;
    float look_y;
    float open; // 0..1 eyelid opening
} EyePose;

// Triangle wave 0 -> 1 -> 0 over period_ms.
static float triangle(int64_t elapsed_ms, int period_ms)
{
    float t = (float)(elapsed_ms % period_ms) / period_ms;
    return t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
}

static float blink_open(const BuddyState *state, int64_t now_ms)
{
    int64_t elapsed = now_ms - state->blink_start_ms;
    if (elapsed >= BLINK_MS) return 1.0f;
    // Quantised so the eye only redraws on visibly different frames.
    return 1.0f - 0.9f * (float)((int)(triangle(elapsed, BLINK_MS) * 8.0f)) / 8.0f;
}

static float glance_offset(const BuddyState *state, int64_t now_ms)
{
    int64_t elapsed = now_ms - state->glance_start_ms;
    if (elapsed >= GLANCE_MS) return 0.0f;
    int64_t ramp = elapsed < GLANCE_EASE_MS ? elapsed : (elapsed > GLANCE_MS - GLANCE_EASE_MS ? GLANCE_MS - elapsed : GLANCE_EASE_MS);
    return (float)(state->glance_direction * (int)(GLANCE_PIXELS * ramp / GLANCE_EASE_MS));
}

static ShapeList add_open_eye(ShapeList list, float cx, EyePose pose)
{
    float x = cx + pose.look_x;
    float y = EYE_Y + pose.look_y;
    float ry = EYE_RY * pose.open;
    list = shape_list_add(list, (Shape){.kind = SHAPE_ELLIPSE, .color = COLOR_EYE, .opacity = 255, .a = x, .b = y, .c = EYE_RX, .d = ry});
    if (pose.open > 0.6f) {
        list = shape_list_add(list, (Shape){.kind = SHAPE_ELLIPSE, .color = COLOR_SHINE, .opacity = 255, .a = x - 7.0f, .b = y - ry * 0.4f, .c = 6.5f, .d = 6.5f});
    }
    return list;
}

static ShapeList add_blush(ShapeList list)
{
    list = shape_list_add(list, (Shape){.kind = SHAPE_ELLIPSE, .color = COLOR_BLUSH, .opacity = 210, .a = 85.0f, .b = 150.0f, .c = 23.0f, .d = 10.0f});
    return shape_list_add(list, (Shape){.kind = SHAPE_ELLIPSE, .color = COLOR_BLUSH, .opacity = 210, .a = 235.0f, .b = 150.0f, .c = 23.0f, .d = 10.0f});
}

static ShapeList add_smile(ShapeList list)
{
    return shape_list_add(list, (Shape){.kind = SHAPE_ARC_DOWN, .color = COLOR_MOUTH, .opacity = 255, .a = MOUTH_X, .b = MOUTH_Y, .c = 10.0f, .d = 4.5f});
}

static ShapeList idle_face(ShapeList list, const BuddyState *state, int64_t now_ms)
{
    EyePose pose = {glance_offset(state, now_ms), 0.0f, blink_open(state, now_ms)};
    list = add_open_eye(list, LEFT_EYE_X, pose);
    list = add_open_eye(list, RIGHT_EYE_X, pose);
    return add_smile(list);
}

static ShapeList thinking_face(ShapeList list, const BuddyState *state, int64_t now_ms)
{
    EyePose pose = {-10.0f, -12.0f, 0.85f * blink_open(state, now_ms)};
    list = add_open_eye(list, LEFT_EYE_X, pose);
    list = add_open_eye(list, RIGHT_EYE_X, pose);
    return shape_list_add(list, (Shape){.kind = SHAPE_CAPSULE, .color = COLOR_MOUTH, .opacity = 255, .a = MOUTH_X - 7.0f, .b = MOUTH_Y + 6.0f, .c = MOUTH_X + 7.0f, .d = MOUTH_Y + 6.0f, .e = 2.5f});
}

static ShapeList talking_face(ShapeList list, int64_t now_ms)
{
    list = shape_list_add(list, (Shape){.kind = SHAPE_ARC_UP, .color = COLOR_EYE, .opacity = 255, .a = LEFT_EYE_X, .b = EYE_Y + 8.0f, .c = 17.0f, .d = 7.0f});
    list = shape_list_add(list, (Shape){.kind = SHAPE_ARC_UP, .color = COLOR_EYE, .opacity = 255, .a = RIGHT_EYE_X, .b = EYE_Y + 8.0f, .c = 17.0f, .d = 7.0f});
    float mouth_ry = (float)(3 + (int)(triangle(now_ms, TALK_PERIOD_MS) * 6.0f));
    return shape_list_add(list, (Shape){.kind = SHAPE_ELLIPSE, .color = COLOR_MOUTH, .opacity = 255, .a = MOUTH_X, .b = MOUTH_Y + 5.0f, .c = 9.0f, .d = mouth_ry});
}

static ShapeList sleepy_face(ShapeList list, int64_t now_ms)
{
    list = shape_list_add(list, (Shape){.kind = SHAPE_ARC_DOWN, .color = COLOR_EYE, .opacity = 255, .a = LEFT_EYE_X, .b = EYE_Y, .c = 16.0f, .d = 6.0f});
    list = shape_list_add(list, (Shape){.kind = SHAPE_ARC_DOWN, .color = COLOR_EYE, .opacity = 255, .a = RIGHT_EYE_X, .b = EYE_Y, .c = 16.0f, .d = 6.0f});
    list = shape_list_add(list, (Shape){.kind = SHAPE_CAPSULE, .color = COLOR_MOUTH, .opacity = 255, .a = MOUTH_X - 5.0f, .b = MOUTH_Y + 6.0f, .c = MOUTH_X + 5.0f, .d = MOUTH_Y + 6.0f, .e = 2.5f});
    // "z Z" drifts up and fades in and out; stepped to ~20 redraws per cycle.
    int step = (int)((now_ms % ZZZ_PERIOD_MS) * 20 / ZZZ_PERIOD_MS);
    float drift = (float)step;
    uint8_t opacity = (uint8_t)(step < 10 ? step * 25 : (19 - step) * 25);
    list = shape_list_add(list, (Shape){.kind = SHAPE_TEXT, .color = COLOR_ZZZ, .opacity = opacity, .a = 240.0f, .b = 72.0f - drift, .text = "z"});
    return shape_list_add(list, (Shape){.kind = SHAPE_TEXT, .color = COLOR_ZZZ, .opacity = opacity, .a = 258.0f, .b = 52.0f - drift, .text = "Z"});
}

ShapeList face_shapes(const BuddyState *state, int64_t now_ms)
{
    ShapeList list = add_blush((ShapeList){.count = 0});
    switch (state->mood) {
    case MOOD_IDLE:
        return idle_face(list, state, now_ms);
    case MOOD_THINKING:
        return thinking_face(list, state, now_ms);
    case MOOD_TALKING:
        return talking_face(list, now_ms);
    case MOOD_SLEEPY:
        return sleepy_face(list, now_ms);
    }
    return list;
}

static bool list_contains(const ShapeList *list, const Shape *shape)
{
    for (int i = 0; i < list->count; i++) {
        if (shape_equal(&list->items[i], shape)) return true;
    }
    return false;
}

Rect face_dirty_rect(const ShapeList *before, const ShapeList *after)
{
    Rect dirty = {0, 0, 0, 0};
    for (int i = 0; i < before->count; i++) {
        if (!list_contains(after, &before->items[i])) dirty = rect_union(dirty, shape_bounds(&before->items[i]));
    }
    for (int i = 0; i < after->count; i++) {
        if (!list_contains(before, &after->items[i])) dirty = rect_union(dirty, shape_bounds(&after->items[i]));
    }
    return rect_intersect(dirty, face_area);
}

void face_draw(Rect area, const ShapeList *shapes, uint16_t *strip_buffer)
{
    Rect clipped = rect_intersect(area, face_area);
    for (int y = clipped.y; y < clipped.y + clipped.h; y += FACE_STRIP_ROWS) {
        int rows = clipped.y + clipped.h - y < FACE_STRIP_ROWS ? clipped.y + clipped.h - y : FACE_STRIP_ROWS;
        Canvas strip = {strip_buffer, {clipped.x, y, clipped.w, rows}};
        canvas_fill(strip, COLOR_FACE_BG);
        for (int i = 0; i < shapes->count; i++) {
            canvas_draw_shape(strip, &shapes->items[i]);
        }
        lcd_push(strip.area.x, strip.area.y, strip.area.w, strip.area.h, strip.pixels);
    }
}
