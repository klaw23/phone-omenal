// Milestone 1: hook detection with debounce.
// Wiring: SLIC SHK -> 10k -> (tap -> GPIO19) -> 15k -> GND; SLIC PD -> GPIO23.
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define PIN_SHK  19
#define PIN_PD   23
#define DEBOUNCE_US 15000

static bool hook_stable = false;   // true = off hook

bool off_hook(void) { return hook_stable; }

void app_main(void) {
    gpio_config_t in = { .pin_bit_mask = 1ULL << PIN_SHK, .mode = GPIO_MODE_INPUT };
    gpio_config(&in);
    gpio_config_t out = { .pin_bit_mask = 1ULL << PIN_PD, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&out);
    gpio_set_level(PIN_PD, 0);                    // SLIC awake

    bool last_raw = false;
    int64_t change_us = 0;
    while (true) {
        bool raw = gpio_get_level(PIN_SHK);
        if (raw != last_raw) { change_us = esp_timer_get_time(); last_raw = raw; }
        if (raw != hook_stable && esp_timer_get_time() - change_us > DEBOUNCE_US) {
            hook_stable = raw;
            printf(hook_stable ? "OFF HOOK\n" : "ON HOOK\n");
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
