/* tb6612.c — TB6612FNG sign-magnitude driver. robocar-original. See tb6612.h. */
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "tb6612.h"

#define STBY_PIN     14u
#define PWM_FREQ_HZ  20000u

/* indexed FL,RL,FR,RR (device_contracts.md canonical pin map) */
static const uint IN1[N_WHEELS] = { 2u, 5u, 8u, 11u };
static const uint IN2[N_WHEELS] = { 3u, 6u, 9u, 12u };
static const uint PWMP[N_WHEELS] = { 4u, 7u, 10u, 13u };

static uint16_t g_top = 0;   /* PWM counts per period (computed from sysclk) */

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void tb6612_init(void) {
    /* STBY: drive HIGH to enable both drivers (LOW = all outputs disabled). */
    gpio_init(STBY_PIN); gpio_set_dir(STBY_PIN, GPIO_OUT); gpio_put(STBY_PIN, 1);

    /* ~20 kHz: counts/period = f_sys / freq, with clkdiv = 1. At 150 MHz -> 7500 (top 7499). */
    const uint32_t f_sys = clock_get_hz(clk_sys);
    uint32_t counts = f_sys / PWM_FREQ_HZ;
    if (counts < 2u)     counts = 2u;
    if (counts > 65536u) counts = 65536u;   /* clamp; at 20 kHz this never trips */
    g_top = (uint16_t)(counts - 1u);

    for (int i = 0; i < N_WHEELS; i++) {
        gpio_init(IN1[i]); gpio_set_dir(IN1[i], GPIO_OUT); gpio_put(IN1[i], 0);
        gpio_init(IN2[i]); gpio_set_dir(IN2[i], GPIO_OUT); gpio_put(IN2[i], 0);

        gpio_set_function(PWMP[i], GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(PWMP[i]);
        pwm_set_clkdiv(slice, 1.0f);
        pwm_set_wrap(slice, g_top);                 /* idempotent if two pins share a slice */
        pwm_set_gpio_level(PWMP[i], 0);
        pwm_set_enabled(slice, true);
    }
}

void tb6612_enable(bool on) { gpio_put(STBY_PIN, on ? 1 : 0); }

void tb6612_set_signed_duty(int wheel, float duty) {
    if (wheel < 0 || wheel >= N_WHEELS) return;
    duty = clampf(duty, -1.0f, 1.0f);

    if (duty > 0.0f) {                  /* forward */
        gpio_put(IN1[wheel], 1); gpio_put(IN2[wheel], 0);
    } else if (duty < 0.0f) {           /* reverse */
        gpio_put(IN1[wheel], 0); gpio_put(IN2[wheel], 1);
    } else {                            /* coast */
        gpio_put(IN1[wheel], 0); gpio_put(IN2[wheel], 0);
    }
    uint16_t level = (uint16_t)(fabsf(duty) * (float)g_top);
    pwm_set_gpio_level(PWMP[wheel], level);
}

void tb6612_coast_all(void) {
    for (int i = 0; i < N_WHEELS; i++) tb6612_set_signed_duty(i, 0.0f);
}
