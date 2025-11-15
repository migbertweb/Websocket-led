/*
 * Archivo: main.c
 * 
 * Descripción: Punto de entrada principal del sistema ESP32-C3. Inicializa todos los
 * componentes hardware (I2C, OLED, LED, DHT11), configura WiFi, inicia el servidor
 * WebSocket y crea las tareas para lectura periódica del sensor DHT11 y actualización
 * de la pantalla OLED con el estado del sistema.
 * 
 * Autor: migbertweb
 * Fecha: 2025-11-15
 * Repositorio: https://github.com/migbertweb/Websocket-led
 * Licencia: MIT License
 * 
 * Uso: Este archivo contiene la función app_main() que es el punto de entrada del
 * programa después del bootloader. Coordina la inicialización de todos los componentes
 * y crea las tareas de FreeRTOS necesarias para el funcionamiento del sistema.
 * 
 * Nota: Este proyecto usa Licencia MIT. Se recomienda (no obliga) mantener 
 * derivados como código libre, especialmente para fines educativos.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

#include "led_control.h"
#include "websocket_server.h"
#include "oled.h"
#include "dht11.h"

static const char *TAG = "MAIN";

/* ------------------------------------------------------------------
 * Sensor DHT11 (variable global)
 * - Se declara aquí para que la tarea `dht11_task` y otras partes del
 *   sistema puedan acceder a las lecturas mediante get_dht11_data().
 * - Nota: el acceso concurrido no está protegido en este ejemplo; si
 *   múltiples tareas acceden a los datos, considerar un mutex.
 * ------------------------------------------------------------------ */
static dht11_t g_dht11_sensor = {
    .dht11_pin = GPIO_NUM_4,
    .temperature = 0,
    .humidity = 0
};


/**
 * Tarea que maneja la lectura periódica del DHT11.
 * La tarea inicializa el sensor y luego realiza lecturas cada ~3s.
 */
void dht11_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Inicializando DHT11 en GPIO %d", g_dht11_sensor.dht11_pin);

    if (dht11_init(&g_dht11_sensor) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DHT11");
        vTaskDelete(NULL);
        return;
    }

    /* Esperar 2s para estabilizar el sensor. */
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Comenzando lecturas DHT11...");

    int success_count = 0;
    int error_count = 0;

    for (;;) {
        /* Leer sensor (bloqueante) */
        esp_err_t result = dht11_read(&g_dht11_sensor, 3);

        if (result == ESP_OK) {
            success_count++;
            ESP_LOGI(TAG, "DHT11 ✅ #%d - Temp: %.1f°C, Hum: %.1f%%",
                     success_count, g_dht11_sensor.temperature, g_dht11_sensor.humidity);
        } else {
            error_count++;
            ESP_LOGW(TAG, "DHT11 ❌ #%d - Error: %d", error_count, result);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}


/**
 * Devuelve un puntero a la estructura global con las últimas lecturas.
 * Útil para que otras tareas lean los valores (sin sincronización en este ejemplo).
 */
dht11_t *get_dht11_data(void)
{
    return &g_dht11_sensor;
}


void app_main(void)
{
    /* ------------------------------------------------------------------
     * Inicialización hardware básico
     * ------------------------------------------------------------------ */

    /* 1) Inicializar I2C (para OLED) */
    i2c_master_init();
    ESP_LOGI(TAG, "I2C inicializado");

    /* 2) Inicializar OLED */
    oled_init();
    ESP_LOGI(TAG, "OLED inicializado");

    /* Mostrar pantallas de inicio */
    oled_show_splash_screen();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    oled_show_welcome_screen();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    /* ------------------------------------------------------------------
     * Inicialización del sistema (NVS, SPIFFS, WiFi, WebSocket, etc.)
     * ------------------------------------------------------------------ */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Inicializando SPIFFS...");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };

    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS partición size: total: %d, used: %d", total, used);
    }

    /* Inicializar componentes */
    ESP_LOGI(TAG, "Inicializando control de LED...");
    led_control_init();

    ESP_LOGI(TAG, "Inicializando WiFi...");
    wifi_init_sta();

    ESP_LOGI(TAG, "Inicializando servidor WebSocket...");
    start_websocket_server();

    ESP_LOGI(TAG, "✅ Sistema listo. Conectarse a la IP mostrada para controlar el LED");

    /* ------------------------------------------------------------------
     * Crear tareas
     * ------------------------------------------------------------------ */
    xTaskCreate(&dht11_task, "dht11_task", 4096, NULL, 5, NULL);

    /* Bucle principal: actualiza OLED con estado y lecturas del DHT11 */
    for (;;) {
        float temperature = g_dht11_sensor.temperature;
        float humidity = g_dht11_sensor.humidity;

        const char *ip_address = websocket_server_get_ip();
        char dht_status[32];
        snprintf(dht_status, sizeof(dht_status), "%.1fC %.1f%%", temperature, humidity);

        /* Mostrar estado combinado: led, ip y dht */
        oled_show_combined_status(led_control_get_state(), ip_address, dht_status);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}