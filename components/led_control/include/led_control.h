#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdbool.h>

/**
 * @file led_control.h
 * @brief API para el control de un LED conectado a GPIO2.
 *
 * Proporciona inicialización y funciones para leer/modificar el estado
 * del LED. La implementación mantiene un estado interno en RAM.
 *
 * Autor: migbertweb
 * Fecha: 2025-11-09
 */

/**
 * @brief Inicializa el control del LED en GPIO2.
 *
 * Configura el pin como salida y pone el LED en estado apagado.
 */
void led_control_init(void);

/**
 * @brief Obtiene el estado actual del LED
 * @return true si está encendido, false si está apagado
 */
bool led_control_get_state(void);

/**
 * @brief Establece el estado del LED
 * @param state true para encender, false para apagar
 */
void led_control_set_state(bool state);

/**
 * @brief Cambia el estado del LED (toggle)
 */
void led_control_toggle(void);

#endif // LED_CONTROL_H
