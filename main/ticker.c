#include "ticker.h"

#include <math.h>

#include "lcd.h"
#include "render.h"

#define COLOR_TICKER_BG RGB565(0xFF, 0xC6, 0xDA)
#define COLOR_TICKER_TEXT RGB565(0x6B, 0x2A, 0x4A)

#define TEXT_BASELINE (TICKER_Y + 28)
#define DOTS_PERIOD_MS 900
#define DOTS_STEPS 18

static const char heart_text[] = {FONT_HEART_CODE, '\0'};

TickerPose ticker_pose(const BuddyState *state, int64_t now_ms)
{
    switch (state->mood) {
    case MOOD_THINKING:
        return (TickerPose){TICKER_DOTS, 0, (int32_t)((now_ms % DOTS_PERIOD_MS) * DOTS_STEPS / DOTS_PERIOD_MS)};
    case MOOD_TALKING:
        return (TickerPose){TICKER_TEXT, state->scroll_x, 0};
    case MOOD_SLEEPY:
        return (TickerPose){TICKER_ZZZ, 0, 0};
    case MOOD_IDLE:
        break;
    }
    return (TickerPose){TICKER_HEART, 0, 0};
}

bool ticker_pose_equal(TickerPose a, TickerPose b)
{
    return a.mode == b.mode && a.scroll_x == b.scroll_x && a.dots_step == b.dots_step;
}

static void draw_centered(Canvas canvas, const char *text)
{
    int x = (LCD_WIDTH - text_width(&font_nunito, text)) / 2;
    canvas_draw_text(canvas, &font_nunito, x, TEXT_BASELINE, text, COLOR_TICKER_TEXT, 255);
}

static void draw_dots(Canvas canvas, int32_t step)
{
    for (int i = 0; i < 3; i++) {
        // Each dot hops for the first third of its own phase-shifted cycle.
        float phase = (float)((step + DOTS_STEPS - i * 3) % DOTS_STEPS) / DOTS_STEPS;
        float hop = phase < 0.33f ? sinf(phase / 0.33f * (float)M_PI) * 8.0f : 0.0f;
        Shape dot = {.kind = SHAPE_ELLIPSE, .color = COLOR_TICKER_TEXT, .opacity = 255,
                     .a = 140.0f + i * 20.0f, .b = TICKER_Y + 24.0f - hop, .c = 5.0f, .d = 5.0f};
        canvas_draw_shape(canvas, &dot);
    }
}

void ticker_draw(TickerPose pose, const char *reply, uint16_t *buffer)
{
    Canvas canvas = {buffer, {0, TICKER_Y, LCD_WIDTH, TICKER_HEIGHT}};
    canvas_fill(canvas, COLOR_TICKER_BG);
    switch (pose.mode) {
    case TICKER_HEART:
        draw_centered(canvas, heart_text);
        break;
    case TICKER_DOTS:
        draw_dots(canvas, pose.dots_step);
        break;
    case TICKER_TEXT:
        canvas_draw_text(canvas, &font_nunito, pose.scroll_x, TEXT_BASELINE, reply, COLOR_TICKER_TEXT, 255);
        break;
    case TICKER_ZZZ:
        draw_centered(canvas, "zzz...");
        break;
    }
    lcd_push(canvas.area.x, canvas.area.y, canvas.area.w, canvas.area.h, canvas.pixels);
}
