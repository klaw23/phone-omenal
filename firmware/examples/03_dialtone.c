// Milestone 3: call-progress tones. Fill 8kHz mono 16-bit I2S buffers.
// Codec init (ES8388 over I2C + I2S at 8kHz) comes from ESP32-SIP-Voice's
// driver — see the build guide. This file is just the tone math.
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct { float f1, f2; int on_ms, off_ms; } tone_t;

static const tone_t TONE_DIAL     = {350, 440,  0,   0};   // continuous
static const tone_t TONE_BUSY     = {480, 620, 500, 500};
static const tone_t TONE_RINGBACK = {440, 480, 2000, 4000};

static float ph1, ph2;
static int cadence_ms;

// call at 8kHz sample rate; n samples per buffer
void fill_tone(const tone_t *t, int16_t *buf, int n) {
    for (int i = 0; i < n; i++) {
        bool on = (t->on_ms == 0) ||
                  (cadence_ms % (t->on_ms + t->off_ms)) < t->on_ms;
        float s = on ? sinf(ph1) * 0.25f + sinf(ph2) * 0.25f : 0.0f;
        ph1 += 2.0f * (float)M_PI * t->f1 / 8000.0f;
        ph2 += 2.0f * (float)M_PI * t->f2 / 8000.0f;
        buf[i] = (int16_t)(s * 32767.0f);
        if (i % 8 == 7) cadence_ms++;             // 8 samples = 1ms at 8kHz
    }
}
