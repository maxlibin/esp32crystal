#include "lcd.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP32-2432S028 / Cheap Yellow Display pinout
#define LCD_HOST SPI2_HOST
#define PIN_NUM_MISO 12
#define PIN_NUM_MOSI 13
#define PIN_NUM_CLK 14
#define PIN_NUM_LCD_CS 15
#define PIN_NUM_LCD_DC 2
#define PIN_NUM_BK_LIGHT 21

static const char *TAG = "CRYSTAL_LCD";
static spi_device_handle_t lcd_spi;

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_NUM_LCD_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(lcd_spi, &t));
}

static void lcd_data(const void *data, int len)
{
    if (len == 0) {
        return;
    }

    gpio_set_level(PIN_NUM_LCD_DC, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(lcd_spi, &t));
}

static void lcd_data_byte(uint8_t data)
{
    lcd_data(&data, 1);
}

static void lcd_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    lcd_cmd(0x2A);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    lcd_data(data, 4);

    lcd_cmd(0x2B);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    lcd_data(data, 4);

    lcd_cmd(0x2C);
}

void lcd_push(int x, int y, int w, int h, uint16_t *pixels)
{
    if (w * h > LCD_MAX_PUSH_PIXELS) {
        ESP_LOGE(TAG, "lcd_push block too large: w=%d h=%d max_pixels=%d", w, h, LCD_MAX_PUSH_PIXELS);
        abort();
    }
    for (int i = 0; i < w * h; i++) {
        pixels[i] = __builtin_bswap16(pixels[i]);
    }
    lcd_set_addr_window(x, y, x + w - 1, y + h - 1);
    lcd_data(pixels, w * h * sizeof(uint16_t));
}

void lcd_init(int brightness_percent)
{
    ledc_timer_config_t backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));
    ledc_channel_config_t backlight_channel = {
        .gpio_num = PIN_NUM_BK_LIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = brightness_percent * 255 / 100,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));

    gpio_config_t dc_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_NUM_LCD_DC,
    };
    ESP_ERROR_CHECK(gpio_config(&dc_gpio_config));

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_MAX_PUSH_PIXELS * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_NUM_LCD_CS,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &lcd_spi));

    // ILI9341 init sequence proven on this board by the eimesp32 dashboard.
    lcd_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(150));
    lcd_cmd(0x3A);
    lcd_data_byte(0x55);
    lcd_cmd(0x36);
    lcd_data_byte(0x28);
    lcd_cmd(0xB1);
    lcd_data_byte(0x00);
    lcd_data_byte(0x18);
    lcd_cmd(0xB6);
    lcd_data_byte(0x08);
    lcd_data_byte(0x82);
    lcd_data_byte(0x27);
    lcd_cmd(0xC0);
    lcd_data_byte(0x23);
    lcd_cmd(0xC1);
    lcd_data_byte(0x10);
    lcd_cmd(0xC5);
    lcd_data_byte(0x3E);
    lcd_data_byte(0x28);
    lcd_cmd(0xC7);
    lcd_data_byte(0x86);
    lcd_cmd(0xF2);
    lcd_data_byte(0x00);
    lcd_cmd(0x26);
    lcd_data_byte(0x01);
    lcd_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(100));
}
