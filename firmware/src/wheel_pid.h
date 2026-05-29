/* wheel_pid.h — per-wheel velocity PID, POSITIONAL form (robocar's Phase 2 design).
 *
 * setpoint & measured are in rad/s; dt in seconds; output is signed duty in
 * [-out_limit, +out_limit] for tb6612_set_signed_duty(). Gains are POSITIVE — robocar
 * fixes encoder/PWM polarity in the driver layer, so it never needs the vendor's negative
 * gains. Seed (Phase 2): kp=0.5, ki=2.0, kd=0. See firmware/README.md.
 */
#ifndef ROBOCAR_WHEEL_PID_H
#define ROBOCAR_WHEEL_PID_H

typedef struct {
    float kp, ki, kd;
    float out_limit;     /* signed duty magnitude limit, e.g. 1.0 */
    float integral;      /* explicit integral state (positional form) */
    float prev_err;
} wheel_pid_t;

void  wheel_pid_init(wheel_pid_t *p, float kp, float ki, float kd, float out_limit);
void  wheel_pid_reset(wheel_pid_t *p);   /* call on (re)enable / e-stop release */
float wheel_pid_step(wheel_pid_t *p, float setpoint, float measured, float dt);

#endif /* ROBOCAR_WHEEL_PID_H */
