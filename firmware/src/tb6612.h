/* tb6612.h — 2x TB6612FNG motor driver, sign-magnitude (IN1/IN2 direction + PWM speed).
 *
 * robocar-original (the vendor used a different driver + 2-PWM scheme). Canonical pins
 * (device_contracts.md): FL IN1/IN2/PWM = GP2/3/4, RL = GP5/6/7, FR = GP8/9/10, RR = GP11/12/13;
 * shared STBY = GP14 (driven HIGH at init — the vendor source omits this, a common "motor
 * won't move" footgun). PWM carrier ~20 kHz (above audible). Wheel index order FL,RL,FR,RR.
 */
#ifndef ROBOCAR_TB6612_H
#define ROBOCAR_TB6612_H

#include <stdbool.h>
#include "mecanum_kinematics.h"   /* FL,RL,FR,RR,N_WHEELS */

void tb6612_init(void);                              /* configures pins, STBY HIGH, PWM slices */
void tb6612_set_signed_duty(int wheel, float duty);  /* duty in [-1,+1]; +duty => wheel rolls fwd */
void tb6612_coast_all(void);                         /* all outputs to coast (IN1=IN2=0, duty 0) */
void tb6612_enable(bool on);                         /* drive STBY (software kill / arm) */

#endif /* ROBOCAR_TB6612_H */
