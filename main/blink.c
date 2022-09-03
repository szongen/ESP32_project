#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
// #include "user_main.h"

#define COM_PIN 2

#define LED_OFF 1
#define LED_ON  0

void blink()
{
    gpio_reset_pin(COM_PIN);

    gpio_set_direction(COM_PIN, GPIO_MODE_OUTPUT);
    while(!0) {
        gpio_set_level(COM_PIN, LED_OFF);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(COM_PIN, LED_ON);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

	gpio_set_level(COM_PIN, LED_ON);

	// handler->ubBlinkQuit = 2;
    vTaskDelete(NULL);
}
