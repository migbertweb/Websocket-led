#include "led_control.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "LED_CONTROL";
static bool led_state = false;

void led_control_init(void)
{
    ESP_LOGI(TAG, "Inicializando LED en GPIO2");
    
    // Configurar el GPIO2 como salida
    gpio_reset_pin(GPIO_NUM_2);
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    
    // Apagar el LED inicialmente
    gpio_set_level(GPIO_NUM_2, 0);
    led_state = false;
    
    ESP_LOGI(TAG, "LED control inicializado en GPIO2 - Estado: APAGADO");
}

bool led_control_get_state(void)
{
    return led_state;
}

void led_control_set_state(bool state)
{
    led_state = state;
    gpio_set_level(GPIO_NUM_2, state ? 1 : 0);
    ESP_LOGI(TAG, "LED %s - GPIO2 nivel: %d", 
             state ? "ENCENDIDO" : "APAGADO", 
             state ? 1 : 0);
}

void led_control_toggle(void)
{
    led_state = !led_state;
    gpio_set_level(GPIO_NUM_2, led_state ? 1 : 0);
    ESP_LOGI(TAG, "LED %s (toggle) - GPIO2 nivel: %d", 
             led_state ? "ENCENDIDO" : "APAGADO", 
             led_state ? 1 : 0);
}