/**
 * @file websocket_server.c
 * @brief Servidor HTTP con endpoint WebSocket para controlar un LED y
 * servir la UI desde SPIFFS.
 *
 * Implementación que maneja:
 *  - Endpoints estáticos: /, /style.css, /websocket.js
 *  - WebSocket en /ws para recibir comandos: "ON", "OFF", "TOGGLE", "STATUS"
 *
 * Autor: migbertweb
 * Fecha: 2025-11-09
 */

#include "websocket_server.h"
#include "led_control.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "esp_netif.h"
#include "esp_event.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Tag usado para logs */
static const char *TAG = "WEB_SOCKET";

/* Handle del servidor HTTPD (global para permitir stop si se quisiera) */
static httpd_handle_t server = NULL;

/*
 * Estructura auxiliar (no utilizada activamente en esta versión) que muestra
 * cómo se podría asociar contexto asíncrono por cliente/FD.
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

// Handler para archivos estáticos desde SPIFFS
/**
 * @brief Sirve un archivo desde SPIFFS al cliente HTTP.
 *
 * @param req Puntero a la estructura de petición HTTP.
 * @param filename Nombre del archivo dentro de /spiffs (sin el prefijo).
 * @param content_type Tipo MIME a establecer en la respuesta.
 * @return esp_err_t ESP_OK en éxito, otro código de error en fallo.
 *
 * Notas:
 *  - Lee el archivo en bloques y usa httpd_resp_send_chunk para evitar
 *    cargar todo el contenido en memoria.
 */
static esp_err_t serve_file(httpd_req_t *req, const char* filename, const char* content_type)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);

    /* Abrir en modo binario por seguridad */
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        ESP_LOGE(TAG, "Archivo no encontrado: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);

    char buffer[512];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            fclose(file);
            ESP_LOGE(TAG, "Error enviando archivo: %s", filename);
            return ESP_FAIL;
        }
    }

    fclose(file);
    /* Terminar chunked response */
    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "Archivo servido: %s", filename);
    return ESP_OK;
}

// Handler para página principal
static esp_err_t index_handler(httpd_req_t *req)
{
    return serve_file(req, "index.html", "text/html");
}

// Handler para CSS
static esp_err_t css_handler(httpd_req_t *req)
{
    return serve_file(req, "style.css", "text/css");
}

// Handler para JavaScript
static esp_err_t js_handler(httpd_req_t *req)
{
    return serve_file(req, "websocket.js", "application/javascript");
}

/// Maneja los mensajes WebSocket
/**
 * @brief Handler para el endpoint WebSocket (/ws).
 *
 * Recibe frames WebSocket de tipo texto con comandos simples:
 *  - "ON"     -> enciende el LED
 *  - "OFF"    -> apaga el LED
 *  - "TOGGLE" -> alterna el estado del LED
 *  - "STATUS" -> solicita el estado actual (sin cambiarlo)
 *
 * Responde con un mensaje de texto en formato "LED:ENCENDIDO" o "LED:APAGADO".
 *
 * @param req Petición HTTP (WebSocket)
 * @return esp_err_t ESP_OK siempre que el handler procese correctamente la petición
 */
static esp_err_t handle_ws_req(httpd_req_t *req)
{
    /* Durante el handshake el método es HTTP_GET; devolver OK para aceptarlo */
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake WebSocket realizado");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Mensaje WebSocket recibido");

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    /* Obtener header del frame (tipo y longitud) */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al recibir info del frame: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Frame type: %d, len: %d", ws_pkt.type, ws_pkt.len);

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && ws_pkt.len > 0) {
        /* Reservar buffer para payload + NUL terminator */
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Error al asignar memoria");
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;

        /* Leer payload completo */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error al recibir payload: %s", esp_err_to_name(ret));
            free(buf);
            return ret;
        }

        /* Asegurar terminador NUL */
        buf[ws_pkt.len] = '\0';
        ESP_LOGI(TAG, "Comando recibido: %s", (char*)buf);

        /* Procesar comando (comparaciones sencillas, case-sensitive) */
        if (strcmp((char*)buf, "ON") == 0) {
            ESP_LOGI(TAG, "Encendiendo LED");
            led_control_set_state(true);
        } else if (strcmp((char*)buf, "OFF") == 0) {
            ESP_LOGI(TAG, "Apagando LED");
            led_control_set_state(false);
        } else if (strcmp((char*)buf, "TOGGLE") == 0) {
            ESP_LOGI(TAG, "Toggle LED");
            led_control_toggle();
        } else if (strcmp((char*)buf, "STATUS") == 0) {
            ESP_LOGI(TAG, "Solicitud de estado");
            /* No cambiar estado, solo responder más abajo */
        } else {
            ESP_LOGW(TAG, "Comando desconocido: %s", (char*)buf);
        }

        free(buf);

        /* Construir respuesta con estado actual */
        bool led_state = led_control_get_state();
        const char* estado = led_state ? "ENCENDIDO" : "APAGADO";
        char response[50];
        snprintf(response, sizeof(response), "LED:%s", estado);

        ESP_LOGI(TAG, "Enviando estado: %s", response);

        httpd_ws_frame_t resp_pkt = {
            .final = true,
            .fragmented = false,
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t*)response,
            .len = strlen(response)
        };

        ret = httpd_ws_send_frame(req, &resp_pkt);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error enviando respuesta: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Respuesta enviada correctamente");
        }
    } else {
        ESP_LOGW(TAG, "Frame no es de texto o está vacío");
    }

    return ESP_OK;
}

// Configuración de los endpoints HTTP/WebSocket
static const httpd_uri_t ws = {
    .uri        = "/ws",
    .method     = HTTP_GET,
    .handler    = handle_ws_req,
    .user_ctx   = NULL,
    .is_websocket = true
};

static const httpd_uri_t index_uri = {
    .uri        = "/",
    .method     = HTTP_GET,
    .handler    = index_handler,
    .user_ctx   = NULL
};

static const httpd_uri_t css_uri = {
    .uri        = "/style.css",
    .method     = HTTP_GET,
    .handler    = css_handler,
    .user_ctx   = NULL
};

static const httpd_uri_t js_uri = {
    .uri        = "/websocket.js",
    .method     = HTTP_GET,
    .handler    = js_handler,
    .user_ctx   = NULL
};

// Inicializar y iniciar el servidor HTTP
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Configuración mejorada para WebSocket
    config.stack_size = 8192;
    config.max_uri_handlers = 20;
    
    ESP_LOGI(TAG, "Iniciando servidor en puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Registrar handlers
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &css_uri);
        httpd_register_uri_handler(server, &js_uri);
        ESP_LOGI(TAG, "Servidor HTTP iniciado correctamente");
        return server;
    }
    
    ESP_LOGE(TAG, "Error al iniciar servidor!");
    return NULL;
}

// WiFi event handler (igual que antes)
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Intentando reconectar al WiFi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Conectado a WiFi! IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}


void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Sukuna-78-2.4g",
            .password = "gMigbert.78", // Agrega tu contraseña aquí si es necesaria
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi inicializado en modo STA");
}

void start_websocket_server(void)
{
    server = start_webserver();
    if (server == NULL) {
        ESP_LOGE(TAG, "Error al iniciar servidor WebSocket");
    }
}

static char s_ip_str[16];

const char* websocket_server_get_ip(void)
{
    esp_netif_ip_info_t ip_info;
    // Obtener handle del netif STA por su key por defecto
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&ip_info.ip));
        return s_ip_str;
    }
    // Si no hay IP, devolver placeholder
    return "0.0.0.0";
}