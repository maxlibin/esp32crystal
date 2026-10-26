#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buddy.h"

#define TICKER_Y 200
#define TICKER_HEIGHT 40

typedef enum {
    TICKER_HEART,
    TICKER_DOTS,
    TICKER_TEXT,
    TICKER_ZZZ,
} TickerMode;

// What the ticker strip shows on one frame; equal poses render identical pixels.
typedef struct {
    TickerMode mode;
    int32_t scroll_x;
    int32_t dots_step;
} TickerPose;

TickerPose ticker_pose(const BuddyState *state, int64_t now_ms);
bool ticker_pose_equal(TickerPose a, TickerPose b);

// Renders the full ticker strip into a DMA buffer of LCD_WIDTH * TICKER_HEIGHT
// pixels and pushes it to the panel.
void ticker_draw(TickerPose pose, const char *reply, uint16_t *buffer);
