/* mecanum_kinematics.h — body twist <-> 4 wheel angular speeds (X-config Mecanum).
 *
 * Canonical wheel order EVERYWHERE: FL, RL, FR, RR (robocar golden rule — a mismatch
 * silently swaps two wheels). These enum values are used directly as array indices.
 *
 * Provenance: clean-room re-expression of the standard X-config Mecanum equations,
 * numerically verified sign-for-sign against robocar's existing Phase 3 inverse()/forward()
 * AND the working vendor RRC Lite mecanum_chassis.c. See firmware/README.md.
 */
#ifndef ROBOCAR_MECANUM_KINEMATICS_H
#define ROBOCAR_MECANUM_KINEMATICS_H

enum { FL = 0, RL = 1, FR = 2, RR = 3, N_WHEELS = 4 };

/* Body twist: vx=forward(+x), vy=left(+y), wz=CCW(+z). SI units: m/s, m/s, rad/s.
 *   wheel_radius_m = R
 *   L = lx + ly = half-track + half-wheelbase  (HALF dimensions — NOT the vendor's 2*(lx+ly))
 * out[] is the FL,RL,FR,RR wheel angular speed in rad/s; +out => that wheel rolls the chassis forward.
 * (Per-wheel motor polarity is fixed in the driver/PIO layer, never via signs here.) */
void mecanum_inverse(float vx, float vy, float wz,
                     float wheel_radius_m, float L,
                     float wheel_omega_radps[N_WHEELS]);

/* Forward (least-squares pseudo-inverse): 4 measured wheel speeds -> body twist.
 * vx,vy are the /4 average; wz is /(4L). Round-trips exact against mecanum_inverse(). */
void mecanum_forward(const float wheel_omega_radps[N_WHEELS],
                     float wheel_radius_m, float L,
                     float *vx, float *vy, float *wz);

#endif /* ROBOCAR_MECANUM_KINEMATICS_H */
