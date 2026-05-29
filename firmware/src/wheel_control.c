/* wheel_control.c — de-STM32'd re-expression of the vendor encoder_motor.c control path.
 * Clean-room, RP2350/Pico SDK. NO HAL, NO FreeRTOS, NO hardware-timer encoder mode.
 * Conventions: wheels indexed FL,RL,FR,RR; speeds in rad/s; +duty => wheel rolls chassis
 * forward (polarity fixed in the PIO/TB6612 layer, NOT via negative gains).
 */
#include <math.h>
#include "pico/stdlib.h"
#include "mecanum_kinematics.h"      /* FL,RL,FR,RR,N_WHEELS, inverse/forward */
#include "wheel_pid.h"
#include "tb6612.h"                  /* tb6612_set_signed_duty() */
#include "quadrature_encoder.h"      /* pio_quad_get_count() — wrap-safe int64 */
#include "wheel_control.h"

/* ---- tunables: STARTING values; bench-tune (provenance in firmware/README.md). ---- */
#define VEL_IIR_ALPHA   0.9f      /* vendor 0.9 new / 0.1 old; loop-rate dependent, re-tune */
#define DEADBAND_DUTY   0.25f     /* vendor 25%; per-motor stiction — RE-MEASURE min-moving duty */
#define RPS_LIMIT_RADPS 47.0f     /* PLACEHOLDER — set from MEASURED max wheel speed (Phase 2); ~450 RPM no-load est. */
#define TWO_PI          6.283185307179586f   /* avoid M_PI (not ISO C / __STRICT_ANSI__ dependent) */
/* robocar Phase 2 seed gains (README.md:203), POSITIVE — polarity fixed upstream: */
#define PID_KP 0.5f
#define PID_KI 2.0f
#define PID_KD 0.0f
/* stall detection (robocar-added; the vendor firmware has none). PLACEHOLDERS — tune on the bench. */
#define STALL_SETPOINT_MIN 1.0f   /* rad/s: only watch wheels actually commanded to move */
#define STALL_OMEGA_MAX    0.3f   /* rad/s: "not moving" threshold */
#define STALL_TICKS        40     /* ~0.8 s at 50 Hz before cutting PWM + latching a fault */

typedef struct {
    int64_t     last_count;       /* int64 cumulative; PIO gives a wide count (no overflow_num) */
    float       tps;              /* filtered ticks/sec */
    float       omega;            /* rad/s (robocar uses rad/s, not the vendor's rps) */
    float       counts_per_rev;   /* MEASURED in Phase 2 — never hardcoded */
    wheel_pid_t pid;
    int         stall_ticks;      /* consecutive ticks commanded-but-not-moving */
    bool        fault;            /* latched stall fault (cleared on reset/coast) */
} wheel_t;

static wheel_t wheels[N_WHEELS];
static float   g_setpoint[N_WHEELS];   /* rad/s, from kinematics */
static float   g_wheel_radius_m;       /* MEASURED */
static float   g_L;                    /* lx+ly, MEASURED (HALF dims) */

/* comms-watchdog: coast if no fresh twist within 0.5 s (Phase 3 README.md:120-122).
 * NOTE: this is the *command-timeout* watchdog. The *hardware* watchdog (reboot a hung
 * Pico) lives in main.c — they are two different protections. */
static absolute_time_t g_last_cmd;
#define WATCHDOG_US 500000

void control_init(float counts_per_rev_measured, float wheel_radius_m, float L) {
    g_wheel_radius_m = wheel_radius_m;
    g_L = L;
    for (int i = 0; i < N_WHEELS; i++) {
        wheels[i].last_count     = pio_quad_get_count(i);   /* seed so the first dt isn't a jump */
        wheels[i].tps            = 0.0f;
        wheels[i].omega          = 0.0f;
        wheels[i].counts_per_rev = counts_per_rev_measured;
        wheel_pid_init(&wheels[i].pid, PID_KP, PID_KI, PID_KD, 1.0f);
        wheels[i].stall_ticks = 0;
        wheels[i].fault = false;
        g_setpoint[i] = 0.0f;
    }
    g_last_cmd = get_absolute_time();
}

void control_set_twist(float vx, float vy, float wz) {
    float w[N_WHEELS];
    mecanum_inverse(vx, vy, wz, g_wheel_radius_m, g_L, w);
    /* If any wheel exceeds the limit, scale ALL FOUR by a common factor so the commanded
     * body-twist DIRECTION is preserved. Clamping each wheel independently would distort a
     * combined twist (e.g. vx+wz) into a different motion. */
    float peak = 0.0f;
    for (int i = 0; i < N_WHEELS; i++) { float a = fabsf(w[i]); if (a > peak) peak = a; }
    const float scale = (peak > RPS_LIMIT_RADPS) ? (RPS_LIMIT_RADPS / peak) : 1.0f;
    for (int i = 0; i < N_WHEELS; i++) g_setpoint[i] = w[i] * scale;
    g_last_cmd = get_absolute_time();
}

void control_tick(float dt) {
    if (dt <= 0.0f) return;
    /* Comms-watchdog: on link loss, truly COAST (high-Z) — do NOT run the PID toward 0,
     * which would actively brake and contradict the documented fail-safe (safety.md). */
    if (absolute_time_diff_us(g_last_cmd, get_absolute_time()) > WATCHDOG_US) {
        for (int i = 0; i < N_WHEELS; i++) {
            g_setpoint[i] = 0.0f;
            wheel_pid_reset(&wheels[i].pid);     /* don't wind up while disarmed */
            wheels[i].stall_ticks = 0;
            wheels[i].fault = false;
            tb6612_set_signed_duty(i, 0.0f);     /* IN1=IN2=0 -> high-Z coast */
        }
        return;                                  /* skip the PID body this tick */
    }
    const float rads_per_rev = TWO_PI;
    for (int i = 0; i < N_WHEELS; i++) {
        wheel_t *w = &wheels[i];
        int64_t c       = pio_quad_get_count(i);                 /* wrap-safe int64 */
        float   raw_tps = (float)(c - w->last_count) / dt;
        w->last_count   = c;
        w->tps   = VEL_IIR_ALPHA*raw_tps + (1.0f - VEL_IIR_ALPHA)*w->tps;
        w->omega = (w->tps / w->counts_per_rev) * rads_per_rev;  /* ticks/s -> rad/s */

        float duty = wheel_pid_step(&w->pid, g_setpoint[i], w->omega, dt);
        /* deadband: below this the 310 just buzzes. RE-MEASURE on the bench. */
        if (duty > -DEADBAND_DUTY && duty < DEADBAND_DUTY) duty = 0.0f;

        /* Stall detection (robocar-added; the vendor has none): commanded to move but measured
         * ~0 for STALL_TICKS in a row -> cut PWM + latch a fault. Thresholds are PLACEHOLDERS,
         * tune on the bench. Gradable test: hand-hold one wheel; only the held wheel cuts. */
        if (fabsf(g_setpoint[i]) > STALL_SETPOINT_MIN && fabsf(w->omega) < STALL_OMEGA_MAX) {
            if (++w->stall_ticks >= STALL_TICKS) { duty = 0.0f; w->fault = true; }
        } else {
            w->stall_ticks = 0;
            w->fault = false;
        }
        tb6612_set_signed_duty(i, duty);
    }
}

/* Telemetry getters: read state written by control_tick() (timer IRQ). A torn float/int64
 * read is harmless for ~20 Hz telemetry; snapshot under save_and_disable_interrupts() if it
 * ever matters. */
float   control_get_omega(int wheel) { return (wheel>=0 && wheel<N_WHEELS) ? wheels[wheel].omega : 0.0f; }
int64_t control_get_count(int wheel) { return (wheel>=0 && wheel<N_WHEELS) ? wheels[wheel].last_count : 0; }

bool control_any_fault(void) {
    for (int i = 0; i < N_WHEELS; i++) if (wheels[i].fault) return true;
    return false;
}

void control_get_twist(float *vx, float *vy, float *wz) {
    float w[N_WHEELS];
    for (int i = 0; i < N_WHEELS; i++) w[i] = wheels[i].omega;
    mecanum_forward(w, g_wheel_radius_m, g_L, vx, vy, wz);
}
