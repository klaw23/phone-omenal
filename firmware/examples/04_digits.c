// Milestone 4: digit collection.
// Rotary: classify SHK breaks (20-90ms = pulse; >300ms quiet = digit done).
// DTMF: MT8870 module; StD -> GPIO18 (interrupt), Q1..Q4 -> GPIO5,13,14,15.
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define PIN_STD 18
#define PIN_Q1  5
#define PIN_Q2  13
#define PIN_Q3  14
#define PIN_Q4  15

static QueueHandle_t digit_q;

/* ---------------- rotary: call from the hook edge detector ---------------- */
static int pulse_count = 0;
static int64_t break_start_us = 0, last_pulse_us = 0;

void rotary_on_hook_edge(bool now_off_hook) {          // from debounced SHK
    int64_t now = esp_timer_get_time();
    if (!now_off_hook) {                               // loop opened: pulse start?
        break_start_us = now;
    } else {                                           // loop closed again
        int64_t br = now - break_start_us;
        if (br > 20000 && br < 90000) {                // 20-90ms = rotary pulse
            pulse_count++;
            last_pulse_us = now;
        }
    }
}

void rotary_poll(void) {                               // call every ~10ms
    if (pulse_count > 0 && esp_timer_get_time() - last_pulse_us > 300000) {
        int digit = pulse_count % 10;                  // 10 pulses = 0
        pulse_count = 0;
        xQueueSend(digit_q, &digit, 0);
    }
}

/* ---------------- DTMF: MT8870 ---------------- */
static void IRAM_ATTR dtmf_isr(void *arg) {
    int code = gpio_get_level(PIN_Q1)
             | gpio_get_level(PIN_Q2) << 1
             | gpio_get_level(PIN_Q3) << 2
             | gpio_get_level(PIN_Q4) << 3;
    int digit = (code == 10) ? 0 : code;               // 1-9 literal, 10='0', 11='*', 12='#'
    BaseType_t hp = pdFALSE;
    xQueueSendFromISR(digit_q, &digit, &hp);
}

void digits_init(void) {
    digit_q = xQueueCreate(16, sizeof(int));
    gpio_config_t in = { .pin_bit_mask = (1ULL<<PIN_Q1)|(1ULL<<PIN_Q2)|(1ULL<<PIN_Q3)|(1ULL<<PIN_Q4),
                         .mode = GPIO_MODE_INPUT };
    gpio_config(&in);
    gpio_config_t std = { .pin_bit_mask = 1ULL<<PIN_STD, .mode = GPIO_MODE_INPUT,
                          .intr_type = GPIO_INTR_POSEDGE };
    gpio_config(&std);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_STD, dtmf_isr, NULL);
}
