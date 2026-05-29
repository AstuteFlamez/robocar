/* main.c — robocar motor-controller integration skeleton (Pico 2W).
 *
 * ROBOCAR-ORIGINAL GLUE — needs the Phase 1/2/3 bench bring-up before you trust it.
 * Wires together: TB6612 driver, PIO encoders, the 50 Hz control loop, the ASCII Pi<->Pico
 * link, the hardware watchdog (reboot a hung Pico), and a heartbeat LED.
 *
 * ASCII line protocol (robocar's own — deliberately human-readable, NOT the vendor's binary
 * CRC frames). Commands in:  "v <vx> <vy> <wz>" (m/s,m/s,rad/s) | "s" (stop) | "ping".
 * Telemetry out:  "t <ms> <wFL> <wRL> <wFR> <wRR>" (rad/s) | "o <vx> <vy> <wz>" | "ok"/"err"/"pong".
 *
 * MEASURED constants below are PLACEHOLDERS from _engineering/ — confirm on the bench
 * (COUNTS_PER_REV in Phase 2 Lab 1; R, L in Phase 3). _engineering/ stays source of truth.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "tb6612.h"
#include "quadrature_encoder.h"
#include "wheel_control.h"
#include "mecanum_kinematics.h"

/* --- placeholders: MEASURE these; do not trust the numbers (see firmware/README.md). --- */
#define COUNTS_PER_REV_MEASURED  1040.0f   /* Phase 2 Lab 1 — calculated 13*4*20; MEASURE */
#define WHEEL_RADIUS_M           0.0485f   /* Phase 3 — MEASURE the actual M1 wheel */
#define L_HALF_SUM_M             0.150f    /* Phase 3 — (lx+ly), HALF dims; MEASURE/calibrate */

#define CONTROL_HZ        50
#define CONTROL_PERIOD_US (1000000 / CONTROL_HZ)
#define TELEM_PERIOD_US   50000             /* 20 Hz telemetry (matches the protocol contract) */
#define LED_PIN           15u               /* heartbeat (discrete LED; onboard LED needs cyw43_arch) */
#define WATCHDOG_MS       300               /* reboot if the main loop hangs this long */

static volatile bool g_link_active = false;   /* heartbeat: solid-ish when receiving commands */

/* 50 Hz control tick — runs in timer context; pass the REAL measured dt, no blocking/printf here. */
static absolute_time_t s_last_tick;
static volatile uint32_t s_control_ticks = 0;   /* advances every control tick — liveness signal */
static bool control_timer_cb(repeating_timer_t *t) {
    (void)t;
    absolute_time_t now = get_absolute_time();
    float dt = (float)absolute_time_diff_us(s_last_tick, now) / 1e6f;
    s_last_tick = now;
    control_tick(dt);
    s_control_ticks++;
    return true;
}

/* parse one received line; return true if it was a valid command */
static bool handle_line(char *line) {
    if (line[0] == 'v') {                        /* "v vx vy wz" */
        float vx, vy, wz;
        if (sscanf(line + 1, "%f %f %f", &vx, &vy, &wz) == 3) {
            control_set_twist(vx, vy, wz);
            g_link_active = true;
            return true;
        }
        return false;
    }
    if (line[0] == 's' && (line[1] == '\0' || line[1] == ' ')) {  /* stop */
        control_set_twist(0.0f, 0.0f, 0.0f);
        return true;
    }
    if (strncmp(line, "ping", 4) == 0) { printf("pong\n"); return true; }
    return false;
}

int main(void) {
    stdio_init_all();                            /* USB CDC serial — the Pi<->Pico link */

    gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT);

    tb6612_init();                               /* STBY HIGH, PWM 20 kHz, outputs at 0 */
    quad_encoder_init();                         /* PIO decoders + int64 accumulators */
    control_init(COUNTS_PER_REV_MEASURED, WHEEL_RADIUS_M, L_HALF_SUM_M);

    /* 50 Hz closed-loop control on a repeating timer (negative period = fixed rate). */
    s_last_tick = get_absolute_time();
    static repeating_timer_t ctl_timer;
    add_repeating_timer_us(-CONTROL_PERIOD_US, control_timer_cb, NULL, &ctl_timer);

    /* HARDWARE watchdog: distinct from the comms-watchdog in wheel_control. If the main
     * loop hangs (so we stop petting), the Pico reboots and re-enumerates the USB link. */
    watchdog_enable(WATCHDOG_MS, 1);

    char line[64]; int n = 0;
    absolute_time_t next_telem = get_absolute_time();
    absolute_time_t next_blink = get_absolute_time();
    bool led = false;
    uint32_t last_seen_ticks = 0;

    while (true) {
        /* Pet the hardware watchdog ONLY when the control loop is still ticking. A live main
         * loop does NOT imply a live control loop — if the control timer wedges, stop petting
         * so the Pico reboots/re-enumerates instead of holding the last duty. */
        uint32_t now_ticks = s_control_ticks;
        if (now_ticks != last_seen_ticks) { last_seen_ticks = now_ticks; watchdog_update(); }

        /* non-blocking line read from USB */
        int ch = getchar_timeout_us(0);
        while (ch != PICO_ERROR_TIMEOUT) {
            if (ch == '\n' || ch == '\r') {
                if (n > 0) { line[n] = '\0'; printf("%s", handle_line(line) ? "ok\n" : "err\n"); n = 0; }
            } else if (n < (int)sizeof(line) - 1) {
                line[n++] = (char)ch;
            } else {
                n = 0;                            /* overflow -> drop the line */
            }
            ch = getchar_timeout_us(0);
        }

        /* telemetry at 20 Hz */
        if (absolute_time_diff_us(next_telem, get_absolute_time()) >= 0) {
            next_telem = delayed_by_us(next_telem, TELEM_PERIOD_US);
            printf("t %u %.3f %.3f %.3f %.3f\n",
                   to_ms_since_boot(get_absolute_time()),
                   control_get_omega(FL), control_get_omega(RL),
                   control_get_omega(FR), control_get_omega(RR));
            float vx, vy, wz; control_get_twist(&vx, &vy, &wz);
            printf("o %.3f %.3f %.3f\n", vx, vy, wz);
        }

        /* heartbeat: slow=idle/link-OK, fast=receiving commands, very fast=latched stall fault */
        int blink_us = control_any_fault() ? 60000 : (g_link_active ? 100000 : 500000);
        if (absolute_time_diff_us(next_blink, get_absolute_time()) >= 0) {
            next_blink = delayed_by_us(next_blink, blink_us);
            led = !led; gpio_put(LED_PIN, led);
            g_link_active = false;               /* re-armed each time a 'v' arrives */
        }
    }
}
