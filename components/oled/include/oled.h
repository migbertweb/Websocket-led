/*
 * oled.h
 *
 * Interfaz para control de una pantalla OLED (72x40) conectada por I2C.
 * Provee inicialización, primitivas de dibujo y utilidades para mostrar
 * textos y estados combinados (por ejemplo: IP, estado del sensor DHT y botón).
 *
 * Notas:
 *  - Las macros de configuración I2C y dimensiones de pantalla están
 *    definidas aquí para facilitar su modificación.
 *  - Este fichero contiene únicamente la interfaz pública; la
 *    implementación está en el componente correspondiente.
 */

#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "fonts.h" /* Tipografías utilizadas por las funciones de texto */

/* -----------------------------
 * Configuración I2C (puede adaptarse según el hardware)
 * ----------------------------- */
#define I2C_MASTER_SCL_IO      6         /* Pin SCL del maestro I2C */
#define I2C_MASTER_SDA_IO      5         /* Pin SDA del maestro I2C */
#define I2C_MASTER_NUM         I2C_NUM_0 /* Núm. del periférico I2C */
#define I2C_MASTER_FREQ_HZ     400000    /* Frecuencia I2C en Hz */
#define OLED_ADDRESS           0x3C      /* Dirección I2C del módulo OLED */


/* -----------------------------
 * Dimensiones de la pantalla OLED
 * Modelos pequeños como 0.42" pueden requerir offsets para centrar
 * el contenido en la memoria framebuffer del controlador.
 * ----------------------------- */
#define SCREEN_WIDTH  72
#define SCREEN_HEIGHT 40
#define X_OFFSET      28
#define Y_OFFSET      12


/* -----------------------------
 * Inicialización y configuración
 * ----------------------------- */
/**
 * Inicializa el driver OLED y la interfaz I2C.
 * Debe llamarse antes de cualquier operación de dibujo.
 */
void oled_init(void);

/**
 * Inicializa el periférico I2C (configuración de pines y velocidad).
 * Se expone por si se quiere inicializar I2C de forma separada.
 */
void i2c_master_init(void);


/* -----------------------------
 * Control básico de pantalla
 * ----------------------------- */
/**
 * Borra el framebuffer interno (no actualiza la pantalla hasta llamar a
 * oled_update()).
 */
void oled_clear(void);

/**
 * Envía el framebuffer al controlador OLED para actualizar la pantalla.
 */
void oled_update(void);

/**
 * Controla la alimentación del panel (0 = apagar, !=0 = encender).
 */
void oled_set_power(int on);


/* -----------------------------
 * Primitivas de dibujo
 * (coordenadas en píxeles, origen en la esquina superior izquierda)
 * ----------------------------- */
void oled_draw_pixel(int x, int y);
void oled_draw_line(int x0, int y0, int x1, int y1);
void oled_draw_rect(int x, int y, int w, int h);
void oled_draw_fill_rect(int x, int y, int w, int h);


/* -----------------------------
 * Texto y utilidades de presentación
 * ----------------------------- */
/**
 * Dibuja una cadena en la posición (x,y). Usa las fuentes definidas en
 * `fonts.h`.
 */
void oled_draw_text(int x, int y, const char *text);

/**
 * Dibuja texto centrado por líneas lógicas (útil para menús simples).
 * `line` es un índice de línea (implementación decidirá altura por línea).
 */
void oled_draw_text_centered(int line, const char *text);


/* -----------------------------
 * Pantallas de ayuda / bienvenida
 * ----------------------------- */
void oled_show_splash_screen(void);
void oled_show_welcome_screen(void);


/* -----------------------------
 * Función principal para mostrar estados combinados
 * ----------------------------- */
/**
 * Muestra un estado combinado en la pantalla: si se presiona un botón,
 * la IP (si está disponible) y el estado del sensor DHT.
 *
 * Parámetros:
 *  - button_pressed: true si el botón está presionado
 *  - ip: cadena con la dirección IP (puede ser NULL o cadena vacía si no disponible)
 *  - dht_status: estado o lectura del sensor DHT (p.ej. "25.3C 40%" o "--")
 */
void oled_show_combined_status(bool button_pressed, const char *ip, const char *dht_status);


#endif /* OLED_H */