#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOOD_SLEEPY,
    MOOD_IDLE,
    MOOD_THINKING,
    MOOD_TALKING,
} Mood;

typedef enum {
    EVENT_HEARTBEAT,
    EVENT_THINKING,
    EVENT_IDLE,
    EVENT_SAY,
} BuddyEventType;

// Crystal's behaviour state; the reply text itself lives with the caller.
typedef struct {
    Mood mood;
    bool heard_ever;
    int64_t last_heard_ms;
    int64_t last_chat_ms;
    int64_t blink_start_ms;
    int64_t next_blink_ms;
    int64_t glance_start_ms;
    int64_t next_glance_ms;
    int32_t glance_direction;
    int32_t scroll_x;
    int32_t text_width;
} BuddyState;

BuddyState buddy_initial(int64_t now_ms);

// Applies one message from the chat app. text_width is the pixel width of the
// reply for EVENT_SAY and ignored otherwise.
BuddyState buddy_apply_event(BuddyState state, BuddyEventType event, int32_t text_width, int64_t now_ms);

// Advances timers, scrolling and sleep by one frame.
BuddyState buddy_tick(BuddyState state, int64_t now_ms, uint32_t random);
