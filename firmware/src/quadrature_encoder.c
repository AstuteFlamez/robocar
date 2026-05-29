/* quadrature_encoder.c — int64 wrap-safe accumulator over the pico-examples PIO decoder.
 * Requires src/quadrature_encoder.pio (from pico-examples, BSD-3-Clause). See firmware/README.md.
 */
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "quadrature_encoder.pio.h"   /* generated from quadrature_encoder.pio by CMake */
#include "quadrature_encoder.h"
#include "mecanum_kinematics.h"       /* FL,RL,FR,RR,N_WHEELS */

/* Per-wheel PIO instance, state machine, and A-channel (base) GPIO. B is base+1. */
static PIO     s_pio[N_WHEELS]  = { pio0, pio0, pio0, pio1 };
static uint    s_sm [N_WHEELS]  = { 0u,   1u,   2u,   0u   };
static const uint s_base[N_WHEELS] = { 16u, 18u, 20u, 26u };   /* FL,RL,FR,RR — A pin */

static int32_t s_last32[N_WHEELS];   /* last raw 32-bit PIO count */
static int64_t s_acc64 [N_WHEELS];   /* cumulative wrap-safe count */

void quad_encoder_init(void) {
    /* Load the program once per PIO instance used (pio0 for FL/RL/FR, pio1 for RR). */
    uint off0 = pio_add_program(pio0, &quadrature_encoder_program);
    uint off1 = pio_add_program(pio1, &quadrature_encoder_program);

    for (int i = 0; i < N_WHEELS; i++) {
        uint off = (s_pio[i] == pio0) ? off0 : off1;
        /* max_step_rate = 0 -> decode as fast as the SM can (fine: ~7.8 kHz per-wheel quadrature
         * transitions at full speed per phase_2_odometry/README.md, well within the SM's reach). */
        quadrature_encoder_program_init(s_pio[i], s_sm[i], off, s_base[i], 0);
        s_last32[i] = (int32_t)quadrature_encoder_get_count(s_pio[i], s_sm[i]);
        s_acc64[i]  = 0;
    }
}

int64_t pio_quad_get_count(int wheel) {
    if (wheel < 0 || wheel >= N_WHEELS) return 0;
    int32_t cur = (int32_t)quadrature_encoder_get_count(s_pio[wheel], s_sm[wheel]);
    /* Wrap-safe delta done in UNSIGNED (signed overflow is UB in ISO C); result is the
     * correct two's-complement difference even across the 32-bit count wrap. */
    int32_t delta = (int32_t)((uint32_t)cur - (uint32_t)s_last32[wheel]);
    s_last32[wheel] = cur;
    s_acc64[wheel] += (int64_t)delta;
    return s_acc64[wheel];
}
