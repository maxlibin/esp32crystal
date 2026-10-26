idf_component_register(SRCS "crystal_main.c" "buddy.c" "face.c" "ticker.c" "render.c" "lcd.c" "link.c" "font_nunito.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_ledc esp_driver_spi esp_driver_uart esp_timer cjson
                       INCLUDE_DIRS "")
