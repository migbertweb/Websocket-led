#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stddef.h>
#include "fonts.h"
#include <stdbool.h>

// Configuración I2C
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

// Funciones de dibujo
void oled_draw_pixel(int x, int y);
void oled_draw_line(int x0, int y0, int x1, int y1);
void oled_draw_rect(int x, int y, int w, int h);
void oled_draw_fill_rect(int x, int y, int w, int h);

// Funciones de texto
void oled_draw_text(int x, int y, const char *text);
void oled_draw_text_centered(int line, const char *text);

// Utilidades
void oled_show_splash_screen(void);
void oled_show_welcome_screen(void);

// Función principal para mostrar estados
void oled_show_combined_status(bool button_pressed, const char* ip, const char* dht_status);
                              
#endif // OLED_H