#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include "esp_http_server.h"

/**
 * @brief Inicializa y conecta el WiFi
 */
void wifi_init_sta(void);

/**
 * @brief Inicia el servidor WebSocket
 */
void start_websocket_server(void);

#endif