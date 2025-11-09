/**
 * @file dht11.c
 * @brief Implementación para leer el sensor DHT11 (bit-banging GPIO).
 *
 * Nota: el protocolo DHT11 es temporalmente crítico. Esta implementación
 * preserva la lógica original y añade comentarios y pequeñas aclaraciones.
 *
 * Autor: migbertweb
 * Fecha: 2025-11-09
 */

#include "dht11.h"

static const char *TAG = "DHT11";

/**
 * Inicializa el pin asociado al DHT11.
 * Configura el GPIO en modo open-drain (entrada/salida) y lo deja en alto.
 */
esp_err_t dht11_init(dht11_t *dht11)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dht11->dht11_pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, /* open-drain para compartir línea */
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO");
        return ret;
    }

    /* Dejar línea en alto (inactivo). */
    gpio_set_level(dht11->dht11_pin, 1);

    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d", dht11->dht11_pin);
    return ESP_OK;
}


/**
 * wait_for_state: espera hasta que el pin alcance el 'state' solicitado.
 * Devuelve el número de microsegundos (contados por ciclo) o -1 en timeout.
 * Nota: implementación muy simple basada en busy-wait (ets_delay_us).
 */
int wait_for_state(dht11_t dht11, int state, int timeout)
{
    int count = 0;
    while (gpio_get_level(dht11.dht11_pin) != state) {
        if (count >= timeout) {
            return -1;
        }
        count++;
        ets_delay_us(1);
    }
    return count;
}


/**
 * hold_low: fuerza la línea a bajo durante `hold_time_us` microsegundos.
 * Se usa para generar la señal de start al DHT11 (al menos 18ms según datasheet).
 */
void hold_low(dht11_t dht11, int hold_time_us)
{
    gpio_set_level(dht11.dht11_pin, 0);
    ets_delay_us(hold_time_us);

    /* Liberar la línea (poner en alto). */
    gpio_set_level(dht11.dht11_pin, 1);
    /* Pequeña espera para estabilizar antes de cambiar a entrada. */
    ets_delay_us(40);
}


/**
 * dht11_read: realiza la secuencia de handshake y lectura de 40 bits.
 * Devuelve ESP_OK si la lectura y checksum son válidos, o un error ESP_ERR_*.
 * connection_timeout: número de reintentos de conexión antes de fallar.
 */
esp_err_t dht11_read(dht11_t *dht11, int connection_timeout)
{
    int waited = 0;
    int timeout_counter = 0;
    uint8_t received_data[5] = {0};

    /* Asegurar que la línea está inactiva (alta) antes de empezar. */
    gpio_set_level(dht11->dht11_pin, 1);
    ets_delay_us(200000); /* 200ms para estabilizar */

    /* Intentar handshake con reintentos limitados. */
    while (timeout_counter < connection_timeout) {
        timeout_counter++;
        ESP_LOGD(TAG, "Attempt %d", timeout_counter);

        /* Señal de inicio: mantener bajo >18ms según datasheet. */
        hold_low(*dht11, 18000);

        /* Cambiar a entrada para leer la respuesta del sensor. */
        gpio_set_direction(dht11->dht11_pin, GPIO_MODE_INPUT);

        /* Fases de respuesta del sensor: espera baja, luego alta, luego baja. */
        waited = wait_for_state(*dht11, 0, 100);
        if (waited == -1) {
            ESP_LOGW(TAG, "Phase 1 timeout - sensor not responding");
            gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
            gpio_set_level(dht11->dht11_pin, 1);
            ets_delay_us(200000);
            continue;
        }

        waited = wait_for_state(*dht11, 1, 100);
        if (waited == -1) {
            ESP_LOGW(TAG, "Phase 2 timeout");
            gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
            gpio_set_level(dht11->dht11_pin, 1);
            ets_delay_us(200000);
            continue;
        }

        waited = wait_for_state(*dht11, 0, 100);
        if (waited == -1) {
            ESP_LOGW(TAG, "Phase 3 timeout");
            gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
            gpio_set_level(dht11->dht11_pin, 1);
            ets_delay_us(200000);
            continue;
        }

        /* Handshake correcto, salir del bucle de reintentos. */
        break;
    }

    if (timeout_counter >= connection_timeout) {
        ESP_LOGE(TAG, "Connection failed after %d attempts", connection_timeout);
        gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
        gpio_set_level(dht11->dht11_pin, 1);
        return ESP_ERR_TIMEOUT;
    }

    /* Leer 40 bits (5 bytes): cada bit se transmite como: 50us bajo + 26-28us alto (0) o ~70us alto (1). */
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 8; j++) {
            /* Esperar inicio del pulso alto del bit */
            waited = wait_for_state(*dht11, 1, 70);
            if (waited == -1) {
                ESP_LOGE(TAG, "Bit %d-1 timeout", (i * 8) + j);
                gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
                gpio_set_level(dht11->dht11_pin, 1);
                return ESP_ERR_TIMEOUT;
            }

            /* Medir duración del pulso alto (busy-wait, 1us paso) */
            int high_time = 0;
            while (gpio_get_level(dht11->dht11_pin) == 1 && high_time < 100) {
                high_time++;
                ets_delay_us(1);
            }

            /* Umbral: si >35us consideramos '1' (valor empírico) */
            if (high_time > 35) {
                received_data[i] |= (1 << (7 - j));
            }

            /* Esperar el flanco a bajo que separa bits (salvo quizá el último). */
            waited = wait_for_state(*dht11, 0, 70);
            if (waited == -1 && j < 7) {
                ESP_LOGE(TAG, "Bit %d-0 timeout", (i * 8) + j);
                gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
                gpio_set_level(dht11->dht11_pin, 1);
                return ESP_ERR_TIMEOUT;
            }
        }
    }

    /* Restaurar el pin a salida (open-drain) y ponerlo en alto. */
    gpio_set_direction(dht11->dht11_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(dht11->dht11_pin, 1);

    ESP_LOGD(TAG, "Data: %02X %02X %02X %02X [%02X]", received_data[0], received_data[1], received_data[2], received_data[3], received_data[4]);

    /* Verificar checksum */
    uint8_t crc = received_data[0] + received_data[1] + received_data[2] + received_data[3];
    if (crc == received_data[4]) {
        dht11->humidity = received_data[0] + (received_data[1] / 10.0);
        dht11->temperature = received_data[2] + (received_data[3] / 10.0);

        /* Rango razonable de lectura */
        if (dht11->humidity > 100.0 || dht11->temperature > 50.0) {
            ESP_LOGE(TAG, "Invalid readings: Temp=%.1f, Hum=%.1f", dht11->temperature, dht11->humidity);
            return ESP_ERR_INVALID_RESPONSE;
        }

        ESP_LOGI(TAG, "Read successful: Temp=%.1f°C, Humidity=%.1f%%", dht11->temperature, dht11->humidity);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Checksum error: calc=0x%02X, recv=0x%02X", crc, received_data[4]);
        return ESP_ERR_INVALID_CRC;
    }
}