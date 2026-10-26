#pragma once

#include "buddy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Longest reply the chat app may send, including the terminating NUL.
#define LINK_TEXT_MAX 4096

// One message from the chat app. Lines on the USB serial port look like
//   @CR:{"type":"heartbeat"}
//   @CR:{"type":"thinking"}
//   @CR:{"type":"idle"}
//   @CR:{"type":"say","text":"Hi! I'm Crystal \u007f"}
// Text holds printable ASCII plus FONT_HEART_CODE only.
typedef struct {
    BuddyEventType type;
    char text[LINK_TEXT_MAX];
} LinkEvent;

// Starts reading the chat app's messages from UART0 and returns the queue
// that receives them as LinkEvent items.
QueueHandle_t link_start(void);
