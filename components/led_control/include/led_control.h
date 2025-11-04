#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdbool.h>

/**
 * @brief Inicializa el control del LED en GPIO2
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

#endif
