// Milestone 2: ring the bell. US cadence 2s on / 4s off, 20Hz.
// Wiring adds: SLIC RM -> GPIO22, SLIC F/R -> GPIO21.
// Depends on off_hook() from 01_hook.c.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define PIN_RM 22
#define PIN_FR 21

extern bool off_hook(void);

void ring_task(void *arg) {
    gpio_config_t out = { .pin_bit_mask = (1ULL<<PIN_RM) | (1ULL<<PIN_FR),
                          .mode = GPIO_MODE_OUTPUT };
    gpio_config(&out);
    while (true) {
        gpio_set_level(PIN_RM, 1);
        for (int i = 0; i < 80; i++) {            // 80 x 25ms = 2s of 20Hz
            gpio_set_level(PIN_FR, i & 1);
            vTaskDelay(pdMS_TO_TICKS(25));
            if (off_hook()) goto answered;
        }
        gpio_set_level(PIN_RM, 0);
        for (int i = 0; i < 400; i++) {           // 4s silence
            vTaskDelay(pdMS_TO_TICKS(10));
            if (off_hook()) goto answered;
        }
    }
answered:
    gpio_set_level(PIN_RM, 0);
    vTaskDelete(NULL);
}
