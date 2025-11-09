#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include "esp_http_server.h"

/**
 * @file websocket_server.h
 * @brief Interfaz pública para el servidor WebSocket y gestión de WiFi (STA).
 *
 * Provee funciones para iniciar la interfaz WiFi en modo estación,
 * arrancar el servidor WebSocket/HTTP que sirve la interfaz web desde SPIFFS
 * y obtener la IP asignada al dispositivo.
 *
 * Autor: migbertweb
 * Fecha: 2025-11-09
 */

/**
 * @brief Inicializa la pila de red y conecta al AP configurado (modo STA).
 *
 * Esta función inicializa esp-netif, el loop de eventos por defecto y
 * arranca la interfaz WiFi en modo estación con las credenciales
 * definidas en la implementación (.c).
 *
 * Salidas:
 *  - Logs informativos del proceso de conexión y reconexión.
 */
void wifi_init_sta(void);

/**
 * @brief Inicia el servidor WebSocket/HTTP.
 *
 * Arranca el servidor HTTPD con soporte WebSocket, registra los handlers
 * para los endpoints estáticos (index, css, js) y el endpoint /ws.
 *
 * Errores: registra un log en caso de fallo al iniciar el servidor.
 */
void start_websocket_server(void);

/**
 * @brief Devuelve la IP actual asignada a la interfaz WiFi STA.
 *
 * @return const char* Pointer a una cadena estática con la IP en formato
 * "x.x.x.x". Si no hay IP válida devuelve "0.0.0.0".
 */
const char* websocket_server_get_ip(void);

#endif // WEBSOCKET_SERVER_H