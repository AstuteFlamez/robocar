/* incremental_pid.c — VENDOR-form reference (see header). NOT used by the running firmware.
 *
 * ORACLE: RRC Lite Misc/pid.c pid_controller_update() lines 15-26, applied incrementally
 *   via encoder_motor.c:44  (current_pulse += pid.output).
 * QUIRK (verified in source): pid.c:17 computes 'proportion = err - prev0_err' but NEVER
 *   uses it; the output (pid.c:22) uses kp*err. We reproduce the EFFECTIVE behavior
 *   (kp*err) and do NOT carry the dead variable.
 */
#include "incremental_pid.h"

void inc_pid_init(incremental_pid_t *p, float kp, float ki, float kd) {
    p->kp = kp; p->ki = ki; p->kd = kd;
    p->setpoint = 0.0f; p->prev0_err = 0.0f; p->prev1_err = 0.0f; p->output = 0.0f;
}

void inc_pid_reset(incremental_pid_t *p) {
    p->prev0_err = 0.0f; p->prev1_err = 0.0f; p->output = 0.0f;
}

float inc_pid_step(incremental_pid_t *p, float measured_rps, float dt) {
    if (dt <= 0.0f) return 0.0f;                          /* guard (vendor had none) */
    const float err    = p->setpoint - measured_rps;                     /* pid.c:16 */
    /* vendor 'proportion' (pid.c:17) intentionally omitted: dead code. */
    const float i_term = p->ki * (err * dt);                             /* pid.c:19,22 */
    const float d_term = p->kd * ((err - 2.0f*p->prev1_err + p->prev0_err) / dt); /* pid.c:20,22 */
    const float delta  = (p->kp * err) + i_term + d_term;                /* pid.c:22 */
    p->prev1_err = p->prev0_err;                                         /* pid.c:23 */
    p->prev0_err = err;                                                  /* pid.c:24 */
    p->output = delta;
    return delta;   /* caller: current_pulse += delta  (encoder_motor.c:44) */
}
