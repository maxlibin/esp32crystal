#include <stdlib.h>
#include <string.h>

#include "buddy.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "face.h"
#include "font.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"
#include "link.h"
#include "ticker.h"

static const char *TAG = "CRYSTAL";

#define FRAME_MS 33

static LinkEvent event;
static char reply[LINK_TEXT_MAX];

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static uint16_t *dma_buffer(size_t pixels)
{
    uint16_t *buffer = heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer: pixels=%u", (unsigned)pixels);
        abort();
    }
    return buffer;
}

void app_main(void)
{
    lcd_init(100);
    QueueHandle_t messages = link_start();
    uint16_t *face_strip = dma_buffer(LCD_WIDTH * FACE_STRIP_ROWS);
    uint16_t *ticker_buffer = dma_buffer(LCD_WIDTH * TICKER_HEIGHT);

    BuddyState state = buddy_initial(now_ms());
    ShapeList drawn_face = face_shapes(&state, now_ms());
    face_draw((Rect){0, 0, LCD_WIDTH, FACE_HEIGHT}, &drawn_face, face_strip);
    TickerPose drawn_ticker = ticker_pose(&state, now_ms());
    ticker_draw(drawn_ticker, reply, ticker_buffer);
    ESP_LOGI(TAG, "Crystal is awake and listening on USB serial");

    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        while (xQueueReceive(messages, &event, 0) == pdTRUE) {
            int32_t width = 0;
            if (event.type == EVENT_SAY) {
                strcpy(reply, event.text);
                width = text_width(&font_nunito, reply);
            }
            state = buddy_apply_event(state, event.type, width, now_ms());
        }

        int64_t now = now_ms();
        state = buddy_tick(state, now, esp_random());

        ShapeList face = face_shapes(&state, now);
        Rect dirty = face_dirty_rect(&drawn_face, &face);
        if (!rect_empty(dirty)) {
            face_draw(dirty, &face, face_strip);
            drawn_face = face;
        }

        TickerPose ticker = ticker_pose(&state, now);
        if (!ticker_pose_equal(ticker, drawn_ticker)) {
            ticker_draw(ticker, reply, ticker_buffer);
            drawn_ticker = ticker;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(FRAME_MS));
    }
}
