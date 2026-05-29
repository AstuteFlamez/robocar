/* mecanum_kinematics.c — clean-room X-config Mecanum kinematics.
 *
 * ORACLE (validated against, NOT copied): RRC Lite mecanum_chassis.c
 *   set_xy() lines 57-61 (the +/-1 sign matrix; vp = wz*lever at :57)
 *   ASCII wheel diagram lines 47-49; dispatch set_motors(self,v1,v4,v2,v3) line 66
 * VERIFIED: this matrix == robocar's EXISTING phase_3_teleop inverse() and == textbook
 * X-config Mecanum, sign-for-sign. forward() has no vendor counterpart (the firmware has
 * no odometry); it is robocar's existing Phase 3 forward(), round-trip verified exact.
 *
 * REMAP vs a naive vendor copy:
 *   - vendor lever = (wheelbase+shaft_length) = 2*(lx+ly); here L = lx+ly. Pass HALF dims.
 *   - vendor right-side motors[2],[3] sign-flips are MOTOR WIRING (dropped here; fixed in driver).
 *   - vendor units mm/s + rps  ->  robocar SI m/s + rad/s (omega = v/R).
 */
#include "mecanum_kinematics.h"

void mecanum_inverse(float vx, float vy, float wz,
                     float wheel_radius_m, float L,
                     float w[N_WHEELS]) {
    const float vp    = wz * L;                 /* tangential speed at each wheel (L = lx+ly) */
    const float inv_R = 1.0f / wheel_radius_m;
    w[FL] = (vx - vy - vp) * inv_R;             /* vendor v1, mecanum_chassis.c:58 */
    w[RL] = (vx + vy - vp) * inv_R;             /* vendor v4, mecanum_chassis.c:61 */
    w[FR] = (vx + vy + vp) * inv_R;             /* vendor v2, mecanum_chassis.c:59 */
    w[RR] = (vx - vy + vp) * inv_R;             /* vendor v3, mecanum_chassis.c:60 */
}

void mecanum_forward(const float w[N_WHEELS],
                     float wheel_radius_m, float L,
                     float *vx, float *vy, float *wz) {
    const float R = wheel_radius_m;
    *vx = R * ( w[FL] + w[RL] + w[FR] + w[RR]) * 0.25f;
    *vy = R * (-w[FL] + w[RL] + w[FR] - w[RR]) * 0.25f;
    *wz = R * (-w[FL] - w[RL] + w[FR] + w[RR]) * (0.25f / L);
}
