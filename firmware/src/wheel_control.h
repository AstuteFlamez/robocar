/* wheel_control.h — the closed-loop control tick: encoder -> rad/s -> PID -> TB6612.
 *
 * Realizes "C from Phase 2" (per-wheel loop) into Phase 3 (twist + comms-watchdog).
 * All geometry/gains/rates are bench-measured (see firmware/README.md) — nothing hardcoded.
 */
#ifndef ROBOCAR_WHEEL_CONTROL_H
#define ROBOCAR_WHEEL_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

/* counts_per_rev_measured: from Phase 2 Lab 1 (never the calculated 1040).
 * wheel_radius_m, L=lx+ly: measured on the M1 in Phase 3 (HALF dims for L). */
void control_init(float counts_per_rev_measured, float wheel_radius_m, float L);

/* Pi->Pico 'v vx vy wz': inverse-kinematics -> 4 rad/s setpoints; pets the comms-watchdog. */
void control_set_twist(float vx, float vy, float wz);

/* one control tick; pass the REAL measured dt (do NOT assume exactly 0.02 s). */
void control_tick(float dt);

/* telemetry */
float   control_get_omega(int wheel);                 /* filtered wheel speed, rad/s */
int64_t control_get_count(int wheel);                 /* cumulative encoder count */
void    control_get_twist(float *vx, float *vy, float *wz);  /* forward-kinematics estimate */
bool    control_any_fault(void);                      /* true if any wheel has a latched stall fault */

#endif /* ROBOCAR_WHEEL_CONTROL_H */
