#include "dht11.h"

static const char *TAG = "DHT11";

esp_err_t dht11_init(dht11_t *dht11)
{
    // Configurar como input primero con pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dht11->dht11_pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,  // Cambiado a OUTPUT_OD
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO");
        return ret;
    }
    
    // Poner en estado alto inicial
    gpio_set_level(dht11->dht11_pin, 1);
    
    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d", dht11->dht11_pin);
    return ESP_OK;
}

int wait_for_state(dht11_t dht11, int state, int timeout)
{
    int count = 0;
    
    while(gpio_get_level(dht11.dht11_pin) != state)
    {
        if(count >= timeout) return -1;
        count++;
        ets_delay_us(1);
    }
    return count;
}

void hold_low(dht11_t dht11, int hold_time_us)
{
    // Ya está configurado como OUTPUT_OD, solo poner a bajo
    gpio_set_level(dht11.dht11_pin, 0);
    ets_delay_us(hold_time_us);
    
    // Volver a alto
    gpio_set_level(dht11.dht11_pin, 1);
    // Pequeño delay para asegurar transición
    ets_delay_us(40);
}

esp_err_t dht11_read(dht11_t *dht11, int connection_timeout)
{
    int waited = 0;
    int timeout_counter = 0;

    uint8_t received_data[5] = {0, 0, 0, 0, 0};

    // Asegurar que el pin está en estado alto antes de empezar
    gpio_set_level(dht11->dht11_pin, 1);
    ets_delay_us(200000); // Esperar 200ms para estabilizar

    while(timeout_counter < connection_timeout)
    {
        timeout_counter++;
        
        ESP_LOGD(TAG, "Attempt %d", timeout_counter);
        
        // Enviar señal de inicio - mantener bajo por 18ms
        hold_low(*dht11, 18000);
        
        // Cambiar a input para leer la respuesta
        gpio_set_direction(dht11->dht11_pin, GPIO_MODE_INPUT);
        
        // Esperar respuesta del sensor (debe ponerse en bajo)
        waited = wait_for_state(*dht11, 0, 100);
        if(waited == -1)
        {
            ESP_LOGW(TAG, "Phase 1 timeout - sensor not responding");
            // Restaurar como output
            gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
            gpio_set_level(dht11->dht11_pin, 1);
            ets_delay_us(200000); // 200ms delay
            continue;
        }

        // Esperar que el sensor ponga alto (80us)
        waited = wait_for_state(*dht11, 1, 100);
        if(waited == -1)
        {
            ESP_LOGW(TAG, "Phase 2 timeout");
            gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
            gpio_set_level(dht11->dht11_pin, 1);
            ets_delay_us(200000);
            continue;
        }
        
        // Esperar inicio de datos (bajo otra vez)
        waited = wait_for_state(*dht11, 0, 100);
        if(waited == -1) 
        {
            ESP_LOGW(TAG, "Phase 3 timeout");
            gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
            gpio_set_level(dht11->dht11_pin, 1);
            ets_delay_us(200000);
            continue;
        }
        
        break;
    }
    
    if(timeout_counter >= connection_timeout) {
        ESP_LOGE(TAG, "Connection failed after %d attempts", connection_timeout);
        gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
        gpio_set_level(dht11->dht11_pin, 1);
        return ESP_ERR_TIMEOUT;
    }

    // Leer los 40 bits de datos
    for(int i = 0; i < 5; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            // Esperar pulso alto (duración indica 0 o 1)
            waited = wait_for_state(*dht11, 1, 70);
            if(waited == -1) {
                ESP_LOGE(TAG, "Bit %d-1 timeout", (i * 8) + j);
                gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
                gpio_set_level(dht11->dht11_pin, 1);
                return ESP_ERR_TIMEOUT;
            }
            
            // Medir duración del pulso alto
            int high_time = 0;
            while(gpio_get_level(dht11->dht11_pin) == 1 && high_time < 100) {
                high_time++;
                ets_delay_us(1);
            }
            
            // Si el pulso alto fue mayor a 35us, es un 1
            if(high_time > 35) {
                received_data[i] |= (1 << (7 - j));
            }
            
            // Esperar que termine el bit (estado bajo entre bits)
            waited = wait_for_state(*dht11, 0, 70);
            if(waited == -1 && j < 7) { // No es necesario para el último bit
                ESP_LOGE(TAG, "Bit %d-0 timeout", (i * 8) + j);
                gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
                gpio_set_level(dht11->dht11_pin, 1);
                return ESP_ERR_TIMEOUT;
            }
        }
    }

    // Restaurar el pin a output y estado alto
    gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(dht11->dht11_pin, 1);

    // Debug: mostrar datos recibidos
    ESP_LOGD(TAG, "Data: %02X %02X %02X %02X [%02X]", 
             received_data[0], received_data[1], received_data[2], received_data[3], received_data[4]);

    // Verificar checksum
    uint8_t crc = received_data[0] + received_data[1] + received_data[2] + received_data[3];
    if(crc == received_data[4]) {
        dht11->humidity = received_data[0] + (received_data[1] / 10.0);
        dht11->temperature = received_data[2] + (received_data[3] / 10.0);
        
        // Validar rangos razonables
        if (dht11->humidity > 100.0 || dht11->temperature > 50.0) {
            ESP_LOGE(TAG, "Invalid readings: Temp=%.1f, Hum=%.1f", 
                     dht11->temperature, dht11->humidity);
            return ESP_ERR_INVALID_RESPONSE;
        }
        
        ESP_LOGI(TAG, "Read successful: Temp=%.1f°C, Humidity=%.1f%%", 
                 dht11->temperature, dht11->humidity);
        return ESP_OK;
    }
    else {
        ESP_LOGE(TAG, "Checksum error: calc=0x%02X, recv=0x%02X", crc, received_data[4]);
        return ESP_ERR_INVALID_CRC;
    }
}