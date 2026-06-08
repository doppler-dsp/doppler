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
pn_create (uint64_t poly, uint64_t seed, uint32_t length, int lfsr)
{
  /* all-zero register is a fixed point; register holds up to 64 bits */
  if (seed == 0 || length == 0 || length > 64)
    return NULL;
  pn_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->mask = (length >= 64) ? ~(uint64_t)0 : (((uint64_t)1 << length) - 1u);
  obj->poly = poly & obj->mask;
  obj->seed = seed & obj->mask;
  obj->reg  = obj->seed;
  obj->kind = (lfsr == PN_FIBONACCI) ? PN_FIBONACCI : PN_GALOIS;
  obj->topshift = length - 1u;
  if (obj->kind == PN_FIBONACCI)
    {
      /* Fibonacci feedback taps = the canonical primitive polynomial's
       * x^0..x^{n-1} terms, recovered from the Galois rep: x^0, plus an
       * inner x^{j+1} for each Galois tap bit j (the bit at length-1 is the
       * x^n feedback and is masked off). Same polynomial → same period. */
      uint64_t taps = 1u; /* x^0 */
      for (uint32_t j = 0; j + 1 < length; j++)
        if ((obj->poly >> j) & 1u)
          taps |= (uint64_t)1 << (j + 1);
      obj->fib_taps = taps & obj->mask;
    }
  return obj;
}

void
pn_destroy (pn_state_t *state)
{
  free (state);
}

void
pn_reset (pn_state_t *state)
{
  state->reg = state->seed;
}

size_t
pn_generate_max_out (pn_state_t *state)
{
  (void)state;
  return 0; /* output length is the caller-requested n */
}

size_t
pn_generate (pn_state_t *state, size_t n, uint8_t *out)
{
  uint64_t       reg  = state->reg;
  const uint64_t poly = state->poly;
  const uint64_t mask = state->mask;
  if (state->kind == PN_FIBONACCI)
    {
      const uint64_t taps = state->fib_taps;
      const uint32_t top  = state->topshift;
      for (size_t i = 0; i < n; i++)
        {
          out[i]      = (uint8_t)(reg & 1u);
          uint64_t fb = (uint64_t)__builtin_parityll (reg & taps);
          reg         = (reg >> 1) | (fb << top);
        }
    }
  else
    {
      for (size_t i = 0; i < n; i++)
        {
          uint8_t bit = (uint8_t)(reg & 1u);
          reg >>= 1;
          if (bit)
            reg ^= poly;
          reg &= mask;
          out[i] = bit;
        }
    }
  state->reg = reg;
  return n;
}
