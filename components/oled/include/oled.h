#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>  // <- AGREGAR ESTA LÍNEA
#include "esp_err.h"

// Configuración I2C para ESP32-C3
#define I2C_MASTER_SCL_IO           6
#define I2C_MASTER_SDA_IO           5
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000
#define OLED_ADDRESS                0x3C

// Configuración pantalla OLED 0.42" (72x40)
#define SCREEN_WIDTH                72
#define SCREEN_HEIGHT               40
#define X_OFFSET                    28
#define Y_OFFSET                    12

// Funciones de inicialización
void oled_init(void);
void i2c_master_init(void);

// Funciones de control básico
void oled_clear(void);
void oled_update(void);
void oled_set_power(int on);

// Funciones de texto
void oled_draw_text(int x, int y, const char *text);
void oled_draw_text_centered(int line, const char *text);

// Funciones específicas para nuestro proyecto
void oled_show_wifi_connecting(void);
void oled_show_wifi_connected(const char* ip);
void oled_show_led_status(const char* ip, bool led_state);
void oled_show_error(const char* message);

#endif // OLED_H