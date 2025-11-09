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

// ============================================================================
// VARIABLE GLOBAL DEL DHT11 - DECLARADA AQUÍ
// ============================================================================
static dht11_t g_dht11_sensor = {
    .dht11_pin = GPIO_NUM_4,
    .temperature = 0,
    .humidity = 0
};

// ============================================================================
// TAREA DEL DHT11 - MODIFICADA PARA USAR LA VARIABLE GLOBAL
// ============================================================================
void dht11_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Inicializando DHT11 en GPIO %d", g_dht11_sensor.dht11_pin);
    
    // Inicializar DHT11
    if (dht11_init(&g_dht11_sensor) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DHT11");
        vTaskDelete(NULL);
        return;
    }
    
    // Esperar 2 segundos para que el sensor se estabilice
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "Comenzando lecturas DHT11...");
    
    int success_count = 0;
    int error_count = 0;
    
    while(1) {
        // Leer del sensor DHT11
        esp_err_t result = dht11_read(&g_dht11_sensor, 3);
        
        if(result == ESP_OK) {
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

// ============================================================================
// FUNCIÓN PARA OBTENER DATOS DEL DHT11 DESDE OTROS LUGARES
// ============================================================================
dht11_t* get_dht11_data(void) {
    return &g_dht11_sensor;
}

void app_main(void)
{
    // ============================================================================
    // INICIALIZACIÓN DE HARDWARE - EXISTENTE
    // ============================================================================
    
    // 1. PRIMERO inicializar I2C (para OLED)
    i2c_master_init();
    ESP_LOGI(TAG, "I2C inicializado");
    
    // 2. LUEGO inicializar OLED
    oled_init();
    ESP_LOGI(TAG, "OLED inicializado");
    
    // 3. Mostrar splash screen
    oled_show_splash_screen();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // 4. Mostrar pantalla de bienvenida
    oled_show_welcome_screen();
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // ============================================================================
    // INICIALIZACIÓN DEL SISTEMA - EXISTENTE
    // ============================================================================
    
    // Inicializar NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar SPIFFS (Sistema de archivos)
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

    // ============================================================================
    // INICIALIZACIÓN DE COMPONENTES - EXISTENTE
    // ============================================================================
    
    ESP_LOGI(TAG, "Inicializando control de LED...");
    led_control_init();

    ESP_LOGI(TAG, "Inicializando WiFi...");
    wifi_init_sta();

    ESP_LOGI(TAG, "Inicializando servidor WebSocket...");
    start_websocket_server();

    ESP_LOGI(TAG, "✅ Sistema listo. Conectarse a la IP mostrada para controlar el LED");

    // ============================================================================
    // CREACIÓN DE TAREAS - MODIFICADO
    // ============================================================================
    
    // CREAR TAREA DEL DHT11 - NUEVO (debe ir ANTES del bucle principal)
    xTaskCreate(&dht11_task,           // Función de la tarea
                "dht11_task",          // Nombre de la tarea
                4096,                  // Tamaño de pila (aumentado a 4096 para seguridad)
                NULL,                  // Parámetros
                5,                     // Prioridad (media)
                NULL);                 // Handle de la tarea

    // ============================================================================
    // BUCLE PRINCIPAL - EXISTENTE (con posibles modificaciones para mostrar datos DHT11)
    // ============================================================================
    while(1) {
                // Obtener datos del DHT11 desde la variable global
        float temperature = g_dht11_sensor.temperature;
        float humidity = g_dht11_sensor.humidity;
        // Actualizar pantalla OLED con estado del LED, IP y contador de pulsaciones
        const char* ip_address = websocket_server_get_ip();
        char dht_status[32];
        snprintf(dht_status, sizeof(dht_status), "%.1fC %.1f%%", temperature, humidity);

        // NOTA: Si quieres mostrar los datos del DHT11 en el OLED, necesitarás:
        // 1. Hacer que dht11_sensor sea global o compartida entre tareas
        // 2. Modificar oled_show_combined_status() para aceptar parámetros de temperatura/humedad
        // 3. Usar semáforos/mutex para acceso seguro a los datos del sensor
        
        oled_show_combined_status(led_control_get_state(), ip_address, dht_status);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}