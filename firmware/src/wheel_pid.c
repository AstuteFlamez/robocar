/* wheel_pid.c — C re-expression of robocar's existing phase_2_odometry pid.py.
 * POSITIONAL PID with conditional-integration anti-windup (robocar's chosen design).
 * The vendor RRC Lite uses a DIFFERENT (incremental) form — see incremental_pid.c for
 * that validated reference; this firmware ships the positional form.
 */
#include "wheel_pid.h"

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void wheel_pid_init(wheel_pid_t *p, float kp, float ki, float kd, float out_limit) {
    p->kp = kp; p->ki = ki; p->kd = kd; p->out_limit = out_limit;
    p->integral = 0.0f; p->prev_err = 0.0f;
}

void wheel_pid_reset(wheel_pid_t *p) { p->integral = 0.0f; p->prev_err = 0.0f; }

float wheel_pid_step(wheel_pid_t *p, float setpoint, float measured, float dt) {
    if (dt <= 0.0f) return 0.0f;                 /* guard (the vendor pid.c had none) */
    const float err   = setpoint - measured;
    const float deriv = (err - p->prev_err) / dt;
    p->prev_err = err;

    /* Conditional integration: accumulate only if the result is unsaturated, OR if
     * integrating would pull the output back out of saturation (anti-windup). */
    const float out_unclamped = p->kp*err + p->ki*(p->integral + err*dt) + p->kd*deriv;
    if (out_unclamped > -p->out_limit && out_unclamped < p->out_limit) {
        p->integral += err*dt;
    } else if (out_unclamped * err < 0.0f) {
        p->integral += err*dt;
    }

    const float out = p->kp*err + p->ki*p->integral + p->kd*deriv;
    return clampf(out, -p->out_limit, p->out_limit);
}
