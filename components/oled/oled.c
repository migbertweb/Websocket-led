#include "oled.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "OLED";

// Comandos SSD1306
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

// Buffer para la pantalla
static uint8_t oled_buffer[SCREEN_WIDTH * (SCREEN_HEIGHT / 8)];

// Font 5x7 básico (solo caracteres ASCII)
static const uint8_t font_5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    // ... (aquí iría el font completo, pero para simplificar usaremos uno básico)
    {0x7F, 0x41, 0x41, 0x41, 0x7F}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    // ... (agregar más caracteres según necesidad)
};

// Función privada para escribir comandos
static void oled_write_cmd(uint8_t cmd) {
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (OLED_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, 0x00, true);
    i2c_master_write_byte(cmd_handle, cmd, true);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
}

// Función privada para escribir datos
static void oled_write_data(uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (OLED_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, 0x40, true);
    i2c_master_write(cmd_handle, data, len, true);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
}

void i2c_master_init(void) {
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
    ESP_LOGI(TAG, "I2C inicializado: SDA=%d, SCL=%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
}

void oled_init(void) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // Secuencia de inicialización para SSD1306 72x40
    oled_write_cmd(SSD1306_DISPLAYOFF);
    oled_write_cmd(SSD1306_SETDISPLAYCLOCKDIV);
    oled_write_cmd(0x80);
    oled_write_cmd(SSD1306_SETMULTIPLEX);
    oled_write_cmd(0x27); // 39 = 0x27 (40-1)
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
    
    oled_clear();
    oled_update();
    
    ESP_LOGI(TAG, "OLED 72x40 inicializado");
}

void oled_clear(void) {
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

void oled_update(void) {
    oled_write_cmd(SSD1306_COLUMNADDR);
    oled_write_cmd(X_OFFSET);
    oled_write_cmd(X_OFFSET + SCREEN_WIDTH - 1);
    oled_write_cmd(SSD1306_PAGEADDR);
    oled_write_cmd(0);
    oled_write_cmd((SCREEN_HEIGHT / 8) - 1);
    oled_write_data(oled_buffer, sizeof(oled_buffer));
}

void oled_set_power(int on) {
    oled_write_cmd(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}

void oled_draw_pixel(int x, int y) {
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) return;
    
    int page = y / 8;
    int bit = y % 8;
    oled_buffer[x + page * SCREEN_WIDTH] |= (1 << bit);
}

void oled_draw_text(int x, int y, const char *text) {
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (c < 32 || c > 126) continue;
        
        int char_x = x + i * 6; // 5px ancho + 1px espacio
        if (char_x >= SCREEN_WIDTH) break;
        
        // Obtener los datos del carácter
        const uint8_t *char_data = font_5x7[c - 32];
        
        // Dibujar cada columna del carácter
        for (int col = 0; col < 5; col++) {
            uint8_t col_data = char_data[col];
            
            // Dibujar cada fila (bit) del carácter
            for (int row = 0; row < 7; row++) {
                if (col_data & (1 << row)) {
                    oled_draw_pixel(char_x + col, y + row);
                }
            }
        }
    }
}

void oled_draw_text_centered(int line, const char *text) {
    int text_width = strlen(text) * 6; // 5px + 1px espacio
    int x = (SCREEN_WIDTH - text_width) / 2;
    int y = line * 10; // 7px alto + 3px espacio
    
    if (x < 0) x = 0;
    oled_draw_text(x, y, text);
}

// ==================== FUNCIONES ESPECÍFICAS DEL PROYECTO ====================

void oled_show_wifi_connecting(void) {
    oled_clear();
    oled_draw_text_centered(1, "Conectando");
    oled_draw_text_centered(2, "WiFi...");
    oled_update();
    ESP_LOGI(TAG, "OLED: Mostrando conexión WiFi");
}

void oled_show_wifi_connected(const char* ip) {
    oled_clear();
    oled_draw_text_centered(0, "WiFi OK!");
    oled_draw_text_centered(1, "IP:");
    
    // Mostrar la IP (puede necesitar truncarse si es muy larga)
    char ip_display[16];
    snprintf(ip_display, sizeof(ip_display), "%s", ip);
    oled_draw_text_centered(2, ip_display);
    
    oled_draw_text_centered(3, "WebSocket");
    oled_draw_text_centered(4, "Listo!");
    
    oled_update();
    ESP_LOGI(TAG, "OLED: WiFi conectado - IP: %s", ip);
}

void oled_show_led_status(const char* ip, bool led_state) {
    oled_clear();
    
    // Línea 1: IP abreviada
    char ip_short[12];
    // Mostrar solo los últimos segmentos de la IP para ahorrar espacio
    const char *last_octet = strrchr(ip, '.');
    if (last_octet) {
        snprintf(ip_short, sizeof(ip_short), "IP:%s", last_octet + 1);
    } else {
        snprintf(ip_short, sizeof(ip_short), "IP:%s", ip);
    }
    oled_draw_text(0, 0, ip_short);
    
    // Línea 2: Estado del LED
    oled_draw_text(0, 10, "LED:");
    if (led_state) {
        oled_draw_text(30, 10, "ON ");
        // Indicador visual del LED encendido
        for (int i = 0; i < 4; i++) {
            oled_draw_pixel(55 + i, 9);
            oled_draw_pixel(55 + i, 10);
        }
    } else {
        oled_draw_text(30, 10, "OFF");
        // Indicador visual del LED apagado (borde)
        oled_draw_pixel(55, 9);
        oled_draw_pixel(58, 9);
        oled_draw_pixel(55, 10);
        oled_draw_pixel(58, 10);
    }
    
    // Línea 3: Instrucciones
    oled_draw_text(0, 20, "Web:");
    oled_draw_text(0, 30, "Control");
    
    oled_update();
}

void oled_show_error(const char* message) {
    oled_clear();
    oled_draw_text_centered(1, "ERROR");
    oled_draw_text_centered(2, message);
    oled_update();
    ESP_LOGE(TAG, "OLED: Error - %s", message);
}