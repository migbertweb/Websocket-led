#ifndef _DHT_11
#define _DHT_11

#include <driver/gpio.h>
#include <stdio.h>
#include <string.h>
#include <rom/ets_sys.h>
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @file dht11.h
 * @brief Interfaz para el sensor DHT11 (lecturas de temperatura y humedad).
 *
 * Las funciones de lectura son bloqueantes y usan busy-wait; deben ejecutarse
 * en una tarea de FreeRTOS separada si se desean lecturas periódicas.
 *
 * Autor: migbertweb
 * Fecha: 2025-11-09
 */

/**
 * Estructura que guarda la configuración/lecturas del DHT11
 * - dht11_pin: GPIO utilizado
 * - temperature: última lectura de temperatura (°C)
 * - humidity: última lectura de humedad (%%)
 */
typedef struct {
    gpio_num_t dht11_pin;
    float temperature;
    float humidity;
} dht11_t;

/**
 * Inicializa el sensor DHT11 (configura GPIO).
 * @param dht11 Puntero a la estructura dht11_t con el pin configurado
 * @return ESP_OK en éxito, o código de error ESP_ERR_*
 */
esp_err_t dht11_init(dht11_t *dht11);

/**
 * Espera que el pin alcance un estado lógico (0 o 1).
 * @return tiempo contado (microsegundos aproximados) o -1 en caso de timeout
 */
int wait_for_state(dht11_t dht11, int state, int timeout);

/**
 * Mantiene la línea en bajo durante `hold_time_us` microsegundos.
 * Se usa para generar la señal de inicio hacia el sensor.
 */
void hold_low(dht11_t dht11, int hold_time_us);

/**
 * Lee temperatura y humedad desde el sensor.
 * Nota: función bloqueante (busy-wait). Esperar al menos 2s entre lecturas.
 * @param dht11 Puntero a la estructura donde se almacenarán las lecturas
 * @param connection_timeout Número de intentos de handshake antes de fallar
 * @return ESP_OK en éxito, o código de error (ESP_ERR_TIMEOUT, ESP_ERR_INVALID_CRC, ...)
 */
esp_err_t dht11_read(dht11_t *dht11, int connection_timeout);

#endif /* _DHT_11 */