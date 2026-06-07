#include "pn/pn_core.h"

/*
 * Galois LFSR PN-sequence generator.
 *
 * The register holds `length` bits; `poly` is the Galois tap mask. Each step
 * emits the LSB, shifts right, and XORs `poly` when the emitted bit was 1.
 * With a primitive `poly` and length L this produces a maximal sequence of
 * period 2^L - 1. Output chips are 0/1 (uint8); map to ±1 downstream.
 */

pn_state_t *
pn_create(uint32_t poly, uint32_t seed, uint32_t length)
{
    /* all-zero register is a fixed point; length must fit a uint32 register */
    if (seed == 0 || length == 0 || length > 32)
        return NULL;
    pn_state_t *obj = calloc(1, sizeof(*obj));
    if (!obj)
        return NULL;
    obj->mask = (length >= 32) ? 0xFFFFFFFFu : ((1u << length) - 1u);
    obj->poly = poly & obj->mask;
    obj->seed = seed & obj->mask;
    obj->reg = obj->seed;
    return obj;
}

void
pn_destroy(pn_state_t *state)
{
    free(state);
}

void
pn_reset(pn_state_t *state)
{
    state->reg = state->seed;
}

size_t
pn_generate_max_out(pn_state_t *state)
{
    (void)state;
    return 0; /* output length is the caller-requested n */
}

size_t
pn_generate(pn_state_t *state, size_t n, uint8_t *out)
{
    uint32_t reg = state->reg;
    const uint32_t poly = state->poly;
    const uint32_t mask = state->mask;
    for (size_t i = 0; i < n; i++) {
        uint8_t bit = (uint8_t)(reg & 1u);
        reg >>= 1;
        if (bit)
            reg ^= poly;
        reg &= mask;
        out[i] = bit;
    }
    state->reg = reg;
    return n;
}
