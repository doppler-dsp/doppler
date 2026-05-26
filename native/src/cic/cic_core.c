/**
 * @file cic_core.c
 * @brief CIC decimation filter — 4-stage, M=1, UQ16 integer pipeline.
 *
 * Input samples (CF32) are converted to UQ16: the Q15 bit pattern of the
 * int16_t representation, zero-extended into the lower 16 bits of a uint64_t.
 * The upper 48 bits provide accumulation headroom for the pipeline gain of
 * CIC_N * log2(R) bits.
 *
 * The unsigned modular-arithmetic CIC property holds unconditionally for
 * uint64_t: every overflow in the integrators cancels exactly in the comb
 * subtractions.  No saturation logic, no range checks, no floating-point
 * in the inner loop.
 *
 * The hot path is fully unrolled (CIC_N=4 stages, M=1 comb).
 */

#include "cic/cic_core.h"

#include <string.h>

/* ── internal helpers ──────────────────────────────────────────────────── */

/* log2 of a known power-of-two value via bit scan. */
static uint32_t
log2_pow2(uint32_t x)
{
    uint32_t k = 0;
    while (x > 1) { k++; x >>= 1; }
    return k;
}

static int
valid_R(uint32_t R)
{
    /* Power of two in [2, 4096]: CIC_N * log2(R) <= 48. */
    return R >= 2 && R <= 4096 && (R & (R - 1)) == 0;
}

/* ── lifecycle ─────────────────────────────────────────────────────────── */

cic_state_t *
cic_create(uint32_t R)
{
    if (!valid_R(R))
        return NULL;

    cic_state_t *s = (cic_state_t *)calloc(1, sizeof(*s));
    if (!s)
        return NULL;

    s->R     = R;
    s->shift = CIC_N * log2_pow2(R);
    return s;
}

void
cic_destroy(cic_state_t *state)
{
    free(state);
}

void
cic_reset(cic_state_t *state)
{
    memset(state->integ_re, 0, sizeof(state->integ_re));
    memset(state->integ_im, 0, sizeof(state->integ_im));
    memset(state->comb_re,  0, sizeof(state->comb_re));
    memset(state->comb_im,  0, sizeof(state->comb_im));
    state->phase = 0;
}

/* ── decimate ──────────────────────────────────────────────────────────── */

size_t
cic_decimate_max_out(cic_state_t *state)
{
    (void)state;
    return 0;
}

/* cic_decimate is a static inline defined in cic_core.h. */

/* ── reconfigure ───────────────────────────────────────────────────────── */

void
cic_reconfigure(cic_state_t *state, uint32_t R)
{
    if (!valid_R(R))
        return;
    state->R     = R;
    state->shift = CIC_N * log2_pow2(R);
    cic_reset(state);
}
