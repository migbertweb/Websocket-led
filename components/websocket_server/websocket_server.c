/*
 * websocket_server.c
 *
 * Servidor HTTP + WebSocket simple para servir los archivos estáticos desde
 * SPIFFS y recibir comandos para controlar el LED y consultar el sensor DHT11.
 *
 * Nota: se mantiene la lógica original; aquí se mejora la legibilidad y se
 * agregan comentarios explicativos en español.
 */

#include "websocket_server.h"
#include "led_control.h"
#include "dht11.h"            /* Acceso a get_dht11_data() */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "esp_netif.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WEB_SOCKET";
static httpd_handle_t server = NULL;

// Declaración externa si la función está en main.c
extern dht11_t* get_dht11_data(void);
/* --------------------------------------------------------------------------
 * Estructuras auxiliares
 * -------------------------------------------------------------------------- */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};


/* --------------------------------------------------------------------------
 * Funciones utilitarias: servir archivos desde SPIFFS
 * - Lee el archivo en chunks y lo envía como respuesta HTTP.
 * - Devuelve ESP_OK en éxito o ESP_FAIL en caso de error.
 * -------------------------------------------------------------------------- */
static esp_err_t serve_file(httpd_req_t *req, const char *filename, const char *content_type)
{
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);

    FILE *file = fopen(filepath, "r");
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
    httpd_resp_send_chunk(req, NULL, 0); /* terminar la transferencia por chunks */
    ESP_LOGI(TAG, "Archivo servido: %s", filename);
    return ESP_OK;
}


/* Handlers HTTP para recursos estáticos */
static esp_err_t index_handler(httpd_req_t *req)
{
    return serve_file(req, "index.html", "text/html");
}

static esp_err_t css_handler(httpd_req_t *req)
{
    return serve_file(req, "style.css", "text/css");
}

static esp_err_t js_handler(httpd_req_t *req)
{
    return serve_file(req, "websocket.js", "application/javascript");
}


/* --------------------------------------------------------------------------
 * Handler WebSocket: procesa mensajes entrantes y responde con estado
 * Soporta comandos: ON, OFF, TOGGLE, STATUS, GET_DHT
 * -------------------------------------------------------------------------- */
static esp_err_t handle_ws_req(httpd_req_t *req)
{
    /* Si es GET, solo completar handshake */
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake WebSocket realizado");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Mensaje WebSocket recibido");

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));

    /* Primera llamada para obtener metadata del frame (tipo y longitud) */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al recibir info del frame: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Frame type: %d, len: %d", ws_pkt.type, ws_pkt.len);

    /* Procesar únicamente frames de texto con contenido */
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && ws_pkt.len > 0) {
        uint8_t *buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Error al asignar memoria");
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;

        /* Recibir payload completo */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error al recibir payload: %s", esp_err_to_name(ret));
            free(buf);
            return ret;
        }

        /* Asegurar terminador de cadena */
        buf[ws_pkt.len] = '\0';
        ESP_LOGI(TAG, "Comando recibido: %s", (char *)buf);

        /* Procesamiento de comandos reconocidos */
        if (strcmp((char *)buf, "ON") == 0) {
            ESP_LOGI(TAG, "Encendiendo LED");
            led_control_set_state(true);
        } else if (strcmp((char *)buf, "OFF") == 0) {
            ESP_LOGI(TAG, "Apagando LED");
            led_control_set_state(false);
        } else if (strcmp((char *)buf, "TOGGLE") == 0) {
            ESP_LOGI(TAG, "Toggle LED");
            led_control_toggle();
        } else if (strcmp((char *)buf, "STATUS") == 0) {
            ESP_LOGI(TAG, "Solicitud de estado");
            /* No cambiar estado, solo se responderá más abajo */
        } else if (strcmp((char *)buf, "GET_DHT") == 0) {
            ESP_LOGI(TAG, "Solicitud de datos DHT11");

            /* Obtener datos actuales del sensor (estructura global) */
            dht11_t *dht_data = get_dht11_data();

            /* Preparar respuesta con formato: "DHT:temperatura:humedad" */
            char dht_response[50];
            snprintf(dht_response, sizeof(dht_response), "DHT:%.1f:%.1f",
                     dht_data->temperature, dht_data->humidity);

            ESP_LOGI(TAG, "Enviando datos DHT: %s", dht_response);

            httpd_ws_frame_t dht_pkt = {
                .final = true,
                .fragmented = false,
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)dht_response,
                .len = (uint32_t)strlen(dht_response),
            };

            ret = httpd_ws_send_frame(req, &dht_pkt);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Error enviando datos DHT: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "Comando desconocido: %s", (char *)buf);
        }

        free(buf);

        /* Responder siempre con el estado actual del LED */
        bool led_state = led_control_get_state();
        const char *estado = led_state ? "ENCENDIDO" : "APAGADO";
        char response[50];
        snprintf(response, sizeof(response), "LED:%s", estado);

        ESP_LOGI(TAG, "Enviando estado: %s", response);

        httpd_ws_frame_t resp_pkt = {
            .final = true,
            .fragmented = false,
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t *)response,
            .len = (uint32_t)strlen(response),
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


/* --------------------------------------------------------------------------
 * Registro de URIs para el servidor HTTP
 * -------------------------------------------------------------------------- */
static const httpd_uri_t ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = handle_ws_req,
    .user_ctx = NULL,
    .is_websocket = true,
};

static const httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t css_uri = {
    .uri = "/style.css",
    .method = HTTP_GET,
    .handler = css_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t js_uri = {
    .uri = "/websocket.js",
    .method = HTTP_GET,
    .handler = js_handler,
    .user_ctx = NULL,
};


/* --------------------------------------------------------------------------
 * Inicialización y arranque del servidor HTTP
 * -------------------------------------------------------------------------- */
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Ajustes para mejorar soporte de WebSocket */
    config.stack_size = 8192;
    config.max_uri_handlers = 20;

    ESP_LOGI(TAG, "Iniciando servidor en puerto: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
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


/* --------------------------------------------------------------------------
 * Manejador de eventos WiFi/IP (registro de logs y reconexión)
 * -------------------------------------------------------------------------- */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Intentando reconectar al WiFi...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
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
            .password = "gMigbert.78",
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

const char *websocket_server_get_ip(void)
{
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&ip_info.ip));
        return s_ip_str;
    }

    return "0.0.0.0";
}