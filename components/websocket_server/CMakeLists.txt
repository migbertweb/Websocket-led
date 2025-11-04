idf_component_register(SRCS "websocket_server.c"
                    INCLUDE_DIRS "include"
                    REQUIRES led_control esp_http_server esp_wifi spiffs)
