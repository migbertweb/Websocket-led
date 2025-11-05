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

static const char *TAG = "MAIN";

void app_main(void)
{
        // 1. PRIMERO inicializar I2C
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

    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar SPIFFS
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

    // Inicializar componentes
    ESP_LOGI(TAG, "Inicializando control de LED...");
    led_control_init();

    ESP_LOGI(TAG, "Inicializando WiFi...");
    wifi_init_sta();

    ESP_LOGI(TAG, "Inicializando servidor WebSocket...");
    start_websocket_server();

    ESP_LOGI(TAG, "✅ Sistema listo. Conectarse a la IP mostrada para controlar el LED");

      while(1) {
        // Actualizar pantalla OLED con estado del LED, IP y contador de pulsaciones
        const char* ip_address = websocket_server_get_ip();

       oled_show_combined_status(led_control_get_state(), ip_address);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}