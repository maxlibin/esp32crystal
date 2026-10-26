#include "link.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "font.h"
#include "freertos/task.h"

static const char *TAG = "CRYSTAL_LINK";

#define LINE_PREFIX "@CR:"
#define LINK_LINE_MAX (LINK_TEXT_MAX * 2)
#define QUEUE_LENGTH 3

static QueueHandle_t link_queue;
static char line[LINK_LINE_MAX];
static LinkEvent parsed_event;

static bool is_supported_text(const char *text)
{
    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        if (*c < ' ' || *c > FONT_HEART_CODE) return false;
    }
    return true;
}

static bool event_type_from_name(const char *name, BuddyEventType *type)
{
    static const struct {
        const char *name;
        BuddyEventType type;
    } types[] = {
        {"heartbeat", EVENT_HEARTBEAT},
        {"thinking", EVENT_THINKING},
        {"idle", EVENT_IDLE},
        {"say", EVENT_SAY},
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (strcmp(name, types[i].name) == 0) {
            *type = types[i].type;
            return true;
        }
    }
    return false;
}

// Parses one "@CR:" payload into parsed_event; logs and returns false when the
// chat app sent something this firmware does not understand.
static bool parse_payload(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        ESP_LOGE(TAG, "Invalid JSON from chat app: payload=%.80s", json);
        return false;
    }
    bool ok = false;
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type) || !event_type_from_name(type->valuestring, &parsed_event.type)) {
        ESP_LOGE(TAG, "Unknown message type from chat app: payload=%.80s", json);
    } else if (parsed_event.type != EVENT_SAY) {
        parsed_event.text[0] = '\0';
        ok = true;
    } else {
        const cJSON *text = cJSON_GetObjectItemCaseSensitive(root, "text");
        if (!cJSON_IsString(text)) {
            ESP_LOGE(TAG, "say message without text: payload=%.80s", json);
        } else if (strlen(text->valuestring) >= LINK_TEXT_MAX) {
            ESP_LOGE(TAG, "say text too long: length=%u max=%d", (unsigned)strlen(text->valuestring), LINK_TEXT_MAX - 1);
        } else if (!is_supported_text(text->valuestring)) {
            ESP_LOGE(TAG, "say text has characters outside printable ASCII and heart: text=%.80s", text->valuestring);
        } else {
            strcpy(parsed_event.text, text->valuestring);
            ok = true;
        }
    }
    cJSON_Delete(root);
    return ok;
}

static void link_task(void *arg)
{
    size_t length = 0;
    bool overflowed = false;
    uint8_t chunk[256];
    while (true) {
        int count = uart_read_bytes(UART_NUM_0, chunk, sizeof(chunk), pdMS_TO_TICKS(20));
        for (int i = 0; i < count; i++) {
            char byte = (char)chunk[i];
            if (byte == '\n' || byte == '\r') {
                line[length] = '\0';
                if (overflowed) {
                    ESP_LOGE(TAG, "Dropped line longer than buffer: max=%d", LINK_LINE_MAX - 1);
                } else if (length > strlen(LINE_PREFIX) && strncmp(line, LINE_PREFIX, strlen(LINE_PREFIX)) == 0 &&
                           parse_payload(line + strlen(LINE_PREFIX))) {
                    xQueueSend(link_queue, &parsed_event, portMAX_DELAY);
                }
                length = 0;
                overflowed = false;
            } else if (length + 1 < sizeof(line)) {
                line[length++] = byte;
            } else {
                overflowed = true;
            }
        }
    }
}

QueueHandle_t link_start(void)
{
    uart_config_t config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 4096, 0, 0, NULL, 0));
    link_queue = xQueueCreate(QUEUE_LENGTH, sizeof(LinkEvent));
    if (link_queue == NULL) {
        ESP_LOGE(TAG, "Failed to allocate link queue: items=%d item_size=%u", QUEUE_LENGTH, (unsigned)sizeof(LinkEvent));
        abort();
    }
    xTaskCreate(link_task, "crystal_link", 4096, NULL, 5, NULL);
    return link_queue;
}
