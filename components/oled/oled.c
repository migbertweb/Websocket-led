/*
 * Archivo: oled.c
 * 
 * Descripción: Driver para pantalla OLED SSD1306 configurada para resolución 72x40 píxeles.
 * Implementa comunicación I2C, primitivas de dibujo (píxel, línea, rectángulo), funciones
 * de texto usando fuentes bitmap 5x7, y pantallas predefinidas para mostrar estado del
 * sistema (IP, LED, sensor DHT11). Incluye inicialización del bus I2C.
 * 
 * Autor: migbertweb
 * Fecha: 2025-11-15
 * Repositorio: https://github.com/migbertweb/Websocket-led
 * Licencia: MIT License
 * 
 * Uso: Este archivo proporciona las funciones para controlar una pantalla OLED conectada
 * por I2C (GPIO5 SDA, GPIO6 SCL, dirección 0x3C). Se utiliza para mostrar información
 * del estado del sistema de forma local sin necesidad de la interfaz web.
 * 
 * Nota: Este proyecto usa Licencia MIT. Se recomienda (no obliga) mantener 
 * derivados como código libre, especialmente para fines educativos.
 */

#include "oled.h"
#include "fonts.h"
#include "led_control.h"

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "OLED";

/* --------------------------------------------------------------------------
 * Comandos SSD1306 (subset usado)
 * -------------------------------------------------------------------------- */
#define SSD1306_DISPLAYOFF          0xAE
#define SSD1306_DISPLAYON           0xAF
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5
#define SSD1306_SETMULTIPLEX        0xA8
#define SSD1306_SETDISPLAYOFFSET    0xD3
#define SSD1306_SETSTARTLINE        0x40
#define SSD1306_CHARGEPUMP          0x8D
#define SSD1306_MEMORYMODE          0x20
#define SSD1306_SEGREMAP            0xA0
#define SSD1306_COMSCANDEC          0xC8
#define SSD1306_SETCOMPINS          0xDA
#define SSD1306_SETCONTRAST         0x81
#define SSD1306_SETPRECHARGE        0xD9
#define SSD1306_SETVCOMDETECT       0xDB
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_NORMALDISPLAY       0xA6
#define SSD1306_COLUMNADDR          0x21
#define SSD1306_PAGEADDR            0x22


/* Buffer para la pantalla (WIDTH x PAGES), páginas = height/8 */
static uint8_t oled_buffer[SCREEN_WIDTH * (SCREEN_HEIGHT / 8)];


/* Funciones privadas I2C -------------------------------------------------- */
static void oled_write_cmd(uint8_t cmd)
{
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (OLED_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, 0x00, true); /* comando */
    i2c_master_write_byte(cmd_handle, cmd, true);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
}

static void oled_write_data(uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (OLED_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, 0x40, true); /* datos */
    i2c_master_write(cmd_handle, data, len, true);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
}


/* Inicialización I2C pública (puede llamarse por separado) */
void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}


/* Inicialización del controlador OLED (SSD1306 módificado para 72x40) */
void oled_init(void)
{
    vTaskDelay(100 / portTICK_PERIOD_MS);

    /* Secuencia de inicialización mínima para SSD1306 */
    oled_write_cmd(SSD1306_DISPLAYOFF);
    oled_write_cmd(SSD1306_SETDISPLAYCLOCKDIV);
    oled_write_cmd(0x80);
    oled_write_cmd(SSD1306_SETMULTIPLEX);
    oled_write_cmd(0x27); /* 39 = 0x27 (40-1) */
    oled_write_cmd(SSD1306_SETDISPLAYOFFSET);
    oled_write_cmd(0x00);
    oled_write_cmd(SSD1306_SETSTARTLINE | 0x00);
    oled_write_cmd(SSD1306_CHARGEPUMP);
    oled_write_cmd(0x14);
    oled_write_cmd(SSD1306_MEMORYMODE);
    oled_write_cmd(0x00);
    oled_write_cmd(SSD1306_SEGREMAP | 0x01);
    oled_write_cmd(SSD1306_COMSCANDEC);
    oled_write_cmd(SSD1306_SETCOMPINS);
    oled_write_cmd(0x12);
    oled_write_cmd(SSD1306_SETCONTRAST);
    oled_write_cmd(0xCF);
    oled_write_cmd(SSD1306_SETPRECHARGE);
    oled_write_cmd(0xF1);
    oled_write_cmd(SSD1306_SETVCOMDETECT);
    oled_write_cmd(0x40);
    oled_write_cmd(SSD1306_DISPLAYALLON_RESUME);
    oled_write_cmd(SSD1306_NORMALDISPLAY);
    oled_write_cmd(SSD1306_DISPLAYON);

    ESP_LOGI(TAG, "OLED 72x40 inicializado");
}


/* Control básico ---------------------------------------------------------- */
void oled_clear(void)
{
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

void oled_update(void)
{
    oled_write_cmd(SSD1306_COLUMNADDR);
    oled_write_cmd(X_OFFSET);
    oled_write_cmd(X_OFFSET + SCREEN_WIDTH - 1);
    oled_write_cmd(SSD1306_PAGEADDR);
    oled_write_cmd(0);
    oled_write_cmd((SCREEN_HEIGHT / 8) - 1);
    oled_write_data(oled_buffer, sizeof(oled_buffer));
}

void oled_set_power(int on)
{
    oled_write_cmd(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}


/* Primitivas de dibujo ---------------------------------------------------- */
void oled_draw_pixel(int x, int y)
{
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return;
    }

    int page = y / 8;
    int bit = y % 8;
    oled_buffer[x + page * SCREEN_WIDTH] |= (1 << bit);
}

void oled_draw_line(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        oled_draw_pixel(x0, y0);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void oled_draw_rect(int x, int y, int w, int h)
{
    oled_draw_line(x, y, x + w, y);
    oled_draw_line(x + w, y, x + w, y + h);
    oled_draw_line(x + w, y + h, x, y + h);
    oled_draw_line(x, y + h, x, y);
}

void oled_draw_fill_rect(int x, int y, int w, int h)
{
    for (int i = x; i < x + w; i++) {
        for (int j = y; j < y + h; j++) {
            oled_draw_pixel(i, j);
        }
    }
}


/* Texto ------------------------------------------------------------------- */
void oled_draw_text(int x, int y, const char *text)
{
    const uint8_t (*font)[5] = font_5x7;

    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (c < 32 || c > 126) {
            continue;
        }

        int char_x = x + i * 6; /* 5px ancho + 1px espacio */
        if (char_x >= SCREEN_WIDTH) {
            break;
        }

        const uint8_t *char_data = font[c - 32];

        for (int col = 0; col < 5; col++) {
            uint8_t col_data = char_data[col];
            for (int row = 0; row < 7; row++) {
                if (col_data & (1 << row)) {
                    oled_draw_pixel(char_x + col, y + row);
                }
            }
        }
    }
}

void oled_draw_text_centered(int line, const char *text)
{
    int text_width = strlen(text) * 6;
    int x = (SCREEN_WIDTH - text_width) / 2;
    int y = line * 10;

    if (x < 0) {
        x = 0;
    }
    oled_draw_text(x, y, text);
}


/* Pantallas / utilidades -------------------------------------------------- */
void oled_show_combined_status(bool button_pressed, const char *ip, const char *dht_status)
{
    /* dht_status y ip son mostrados tal cual; se asume cadenas cortas. */
    oled_clear();

    /* Cabecera con IP (si existe) */
    oled_draw_text_centered(0, ip);

    /* Estado LED */
    bool led_state = led_control_get_state();
    oled_draw_text(0, 10, "LED:");
    oled_draw_text(30, 10, led_state ? "ON " : "OFF");
    if (led_state) {
        oled_draw_fill_rect(50, 9, 8, 8);
    } else {
        oled_draw_rect(50, 9, 8, 8);
    }

    /* Estado botón */
    oled_draw_text(0, 20, "BOTON:");
    oled_draw_text(36, 20, button_pressed ? "PRESS" : "FREE");
    if (button_pressed) {
        oled_draw_fill_rect(75, 19, 4, 4);
    } else {
        oled_draw_rect(75, 19, 4, 4);
    }

    oled_draw_text_centered(3, dht_status);
    oled_update();
}

void oled_show_welcome_screen(void)
{
    oled_clear();
    oled_draw_text_centered(0, "SISTEMA");
    oled_draw_text_centered(1, "LED + WS");
    oled_draw_text_centered(2, "ESP32-C3");
    oled_draw_text_centered(3, "Listo!");
    oled_update();
}

void oled_show_splash_screen(void)
{
    oled_clear();
    oled_draw_text_centered(0, "INICIANDO");
    oled_draw_text_centered(2, "SISTEMA");
    oled_update();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}