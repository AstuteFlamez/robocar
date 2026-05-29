/* quadrature_encoder.h — 4x quadrature decode in PIO, exposed as a wrap-safe int64 count.
 *
 * robocar-original integration. The PIO program itself (quadrature_encoder.pio) is from
 * Raspberry Pi's pico-examples (BSD-3-Clause) — copy it into src/ (see firmware/README.md).
 * The value-add here is the int64 cumulative accumulator: PIO offloads edge counting from
 * the CPU (robocar ADR), and we difference its 32-bit count wrap-safely into an int64 so
 * odometry never has to reconstruct a 16-bit overflow the way the STM32 vendor firmware did.
 *
 * Wiring (device_contracts.md, A/B on adjacent GPIOs, A = base):
 *   FL = GP16/17 (pio0 sm0), RL = GP18/19 (pio0 sm1), FR = GP20/21 (pio0 sm2), RR = GP26/27 (pio1 sm0).
 */
#ifndef ROBOCAR_QUADRATURE_ENCODER_H
#define ROBOCAR_QUADRATURE_ENCODER_H

#include <stdint.h>

void    quad_encoder_init(void);             /* loads PIO program, starts 4 state machines */
int64_t pio_quad_get_count(int wheel);       /* wrap-safe cumulative count, FL,RL,FR,RR */

#endif /* ROBOCAR_QUADRATURE_ENCODER_H */
