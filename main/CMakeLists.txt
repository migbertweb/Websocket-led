idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES websocket_server led_control spiffs nvs_flash oled dht11)
