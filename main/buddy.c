#include "buddy.h"

#include "lcd.h"

// The chat app sends a heartbeat every 2 s; silence this long means it is gone.
#define OFFLINE_AFTER_MS 10000
// Crystal dozes off after this long without a chat message.
#define QUIET_AFTER_MS (5 * 60 * 1000)
#define SCROLL_PIXELS_PER_FRAME 3

static bool is_offline(const BuddyState *state, int64_t now_ms)
{
    return !state->heard_ever || now_ms - state->last_heard_ms > OFFLINE_AFTER_MS;
}

BuddyState buddy_initial(int64_t now_ms)
{
    return (BuddyState){
        .mood = MOOD_SLEEPY,
        .heard_ever = false,
        .last_heard_ms = now_ms,
        .last_chat_ms = now_ms,
        .blink_start_ms = -1000000,
        .next_blink_ms = now_ms + 2000,
        .glance_start_ms = -1000000,
        .next_glance_ms = now_ms + 6000,
        .glance_direction = 1,
        .scroll_x = 0,
        .text_width = 0,
    };
}

BuddyState buddy_apply_event(BuddyState state, BuddyEventType event, int32_t text_width, int64_t now_ms)
{
    bool was_offline = is_offline(&state, now_ms);
    state.heard_ever = true;
    state.last_heard_ms = now_ms;

    switch (event) {
    case EVENT_HEARTBEAT:
        // The chat app just started (or came back): wake up to greet it.
        if (state.mood == MOOD_SLEEPY && was_offline) {
            state.mood = MOOD_IDLE;
            state.last_chat_ms = now_ms;
        }
        break;
    case EVENT_THINKING:
        state.mood = MOOD_THINKING;
        state.last_chat_ms = now_ms;
        break;
    case EVENT_IDLE:
        state.mood = MOOD_IDLE;
        state.last_chat_ms = now_ms;
        break;
    case EVENT_SAY:
        state.mood = MOOD_TALKING;
        state.last_chat_ms = now_ms;
        state.scroll_x = LCD_WIDTH;
        state.text_width = text_width;
        break;
    }
    return state;
}

BuddyState buddy_tick(BuddyState state, int64_t now_ms, uint32_t random)
{
    if (state.mood == MOOD_TALKING) {
        state.scroll_x -= SCROLL_PIXELS_PER_FRAME;
        if (state.scroll_x + state.text_width < 0) {
            state.mood = MOOD_IDLE;
            state.last_chat_ms = now_ms;
        }
    }

    bool offline = is_offline(&state, now_ms);
    bool quiet = now_ms - state.last_chat_ms > QUIET_AFTER_MS;
    if ((state.mood == MOOD_IDLE && (offline || quiet)) || (state.mood == MOOD_THINKING && offline)) {
        state.mood = MOOD_SLEEPY;
    }

    if (now_ms >= state.next_blink_ms) {
        state.blink_start_ms = now_ms;
        state.next_blink_ms = now_ms + 2000 + random % 4000;
    }
    if (now_ms >= state.next_glance_ms) {
        state.glance_start_ms = now_ms;
        state.glance_direction = (random >> 16) % 2 == 0 ? -1 : 1;
        state.next_glance_ms = now_ms + 5000 + (random >> 8) % 6000;
    }
    return state;
}
