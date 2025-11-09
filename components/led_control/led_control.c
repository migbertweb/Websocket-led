/**
 * @file led_control.c
 * @brief Control sencillo de un LED conectado a GPIO2.
 *
 * Proporciona inicialización, lectura, escritura y toggle del estado del LED.
 * Mantiene un estado en memoria para evitar leer el pin cada vez.
 *
 * Autor: migbertweb
 * Fecha: 2025-11-09
 */

#include "led_control.h"
#include "driver/gpio.h"
#include "esp_log.h"

/* Tag para logs */
static const char *TAG = "LED_CONTROL";

/* Estado interno del LED: true = encendido, false = apagado. */
static bool led_state = false;

/**
 * @brief Inicializa el GPIO2 para controlar el LED.
 *
 * Configura el pin como salida y deja el LED apagado.
 */
void led_control_init(void)
{
    ESP_LOGI(TAG, "Inicializando LED en GPIO2");

    /* Configurar el GPIO2 como salida */
    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

    /* Apagar el LED inicialmente */
    gpio_set_level(GPIO_NUM_2, 0);
    led_state = false;

    ESP_LOGI(TAG, "LED control inicializado en GPIO2 - Estado: APAGADO");
}

/**
 * @brief Devuelve el estado interno guardado del LED.
 * @return true si está encendido, false si está apagado.
 */
bool led_control_get_state(void)
{
    return led_state;
}

/**
 * @brief Establece el estado del LED y actualiza el GPIO.
 * @param state true para encender, false para apagar.
 */
void led_control_set_state(bool state)
{
    led_state = state;
    gpio_set_level(GPIO_NUM_2, state ? 1 : 0);
    ESP_LOGI(TAG, "LED %s - GPIO2 nivel: %d",
             state ? "ENCENDIDO" : "APAGADO",
             state ? 1 : 0);
}

/**
 * @brief Alterna el estado del LED (toggle) y actualiza el GPIO.
 */
void led_control_toggle(void)
{
    led_state = !led_state;
    gpio_set_level(GPIO_NUM_2, led_state ? 1 : 0);
    ESP_LOGI(TAG, "LED %s (toggle) - GPIO2 nivel: %d",
             led_state ? "ENCENDIDO" : "APAGADO",
             led_state ? 1 : 0);
}