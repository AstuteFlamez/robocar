/* incremental_pid.h — VENDOR velocity-form PID, REFERENCE ONLY (not linked by default).
 *
 * Provided so the Phase 2 PID lab can compare the vendor's incremental controller against
 * robocar's positional one (wheel_pid). Returns a DELTA added to the running PWM command;
 * integration lives in that accumulator (implicit anti-windup via the PWM clamp). Its
 * weaknesses — no explicit integral state, and D on a noisy signal — are exactly why the
 * velocity IIR filter in wheel_control is mandatory if you ever adopt it.
 *
 * ORACLE: RRC Lite Misc/pid.c (15-26), used incrementally via encoder_motor.c:44.
 */
#ifndef ROBOCAR_INCREMENTAL_PID_H
#define ROBOCAR_INCREMENTAL_PID_H

typedef struct {
    float kp, ki, kd;
    float setpoint;       /* rps */
    float prev0_err;      /* e[k-1]  (vendor previous_0_err) */
    float prev1_err;      /* e[k-2]  (vendor previous_1_err) */
    float output;         /* last computed DELTA (telemetry) */
} incremental_pid_t;

void  inc_pid_init (incremental_pid_t *p, float kp, float ki, float kd);
void  inc_pid_reset(incremental_pid_t *p);
/* returns the INCREMENTAL change to add to the running PWM command */
float inc_pid_step (incremental_pid_t *p, float measured_rps, float dt);

#endif /* ROBOCAR_INCREMENTAL_PID_H */
