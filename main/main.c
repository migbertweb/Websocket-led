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

static const char *TAG = "MAIN";

void app_main(void)
{
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
}