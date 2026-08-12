#include "dp_test.h"
#include "pn/pn_core.h"
#include "wfm_synth/wfm_synth_core.h" /* wfm_synth_mls_poly() — the primitive-poly table */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
/* Steps the LFSR until the register returns to its seed; returns the period,
 * or -1 if it exceeds 2^n (non-maximal / degenerate). */
static long
pn_period (uint64_t poly, uint32_t n, int lfsr)
{
  pn_state_t *p = pn_create (poly, 1, n, lfsr);
  if (!p)
    return -1;
  long per = 0;
  long cap = (n >= 31) ? (1L << 31) : (1L << n) + 2;
  do
    {
      pn_step (p);
      per++;
      if (per > cap)
        {
          per = -1;
          break;
        }
    }
  while (p->reg != 1u);
  pn_destroy (p);
  return per;
}

int
main (void)
{
  pn_state_t *obj = pn_create (96, 1, 7, PN_GALOIS);
  DP_CHECK (obj != NULL);
  if (!obj)
    return 1;

  /* ── 64-bit register: length up to 64, mask + no truncation ── */
  DP_CHECK (pn_create (0, 1, 65, PN_GALOIS)
            == NULL); /* length > 64 rejected */
  pn_state_t *p64 = pn_create (wfm_synth_mls_poly (64), 1, 64, PN_GALOIS);
  DP_CHECK (p64 != NULL);
  if (p64)
    {
      DP_CHECK (p64->mask == ~(uint64_t)0); /* full 64-bit mask */
      DP_CHECK (p64->poly > 0xFFFFFFFFu);   /* 64-bit poly survived */
      int hi = 0;
      for (long i = 0; i < 300000; i++)
        {
          pn_step (p64);
          if (p64->reg > 0xFFFFFFFFu)
            hi = 1;                 /* uses the high half */
          DP_CHECK (p64->reg != 0); /* never collapses to 0 */
        }
      DP_CHECK (hi);
      pn_destroy (p64);
    }

  /* ── MLS table: maximal period (Galois), incl. the n > 32 path ── */
  DP_CHECK (pn_period (wfm_synth_mls_poly (7), 7, PN_GALOIS) == (1L << 7) - 1);
  DP_CHECK (pn_period (wfm_synth_mls_poly (17), 17, PN_GALOIS)
            == (1L << 17) - 1);
  DP_CHECK (pn_period (wfm_synth_mls_poly (20), 20, PN_GALOIS)
            == (1L << 20) - 1);

  /* ── Fibonacci: same primitive poly → same maximal period ── */
  DP_CHECK (pn_period (wfm_synth_mls_poly (7), 7, PN_FIBONACCI)
            == (1L << 7) - 1);
  DP_CHECK (pn_period (wfm_synth_mls_poly (17), 17, PN_FIBONACCI)
            == (1L << 17) - 1);
  DP_CHECK (pn_period (wfm_synth_mls_poly (20), 20, PN_FIBONACCI)
            == (1L << 20) - 1);

  /* ── Galois and Fibonacci are distinct realizations (different chips) ── */
  {
    pn_state_t *g    = pn_create (wfm_synth_mls_poly (9), 1, 9, PN_GALOIS);
    pn_state_t *f    = pn_create (wfm_synth_mls_poly (9), 1, 9, PN_FIBONACCI);
    int         diff = 0;
    for (int i = 0; i < 511; i++)
      if (pn_step (g) != pn_step (f))
        diff = 1;
    DP_CHECK (diff); /* same period, different sequence/phase */
    pn_destroy (g);
    pn_destroy (f);
  }

  /* ── table coverage: nonzero for 2..64, zero outside ── */
  DP_CHECK (wfm_synth_mls_poly (1) == 0);
  DP_CHECK (wfm_synth_mls_poly (65) == 0);
  for (uint32_t n = 2; n <= 64; n++)
    DP_CHECK (wfm_synth_mls_poly (n) != 0);

  /* reset */
  pn_reset (obj);

  pn_destroy (obj);

  /* serializable state — advance the LFSR, serialize, restore into a fresh
   * generator, and the chip stream continues identically; clobber rejects. */
  {
    pn_state_t *a = pn_create (96, 1, 7, PN_GALOIS);
    uint8_t     ref[64], got[64];
    pn_generate (a, 9, ref, 9); /* advance */
    size_t sb   = pn_state_bytes (a);
    void  *blob = malloc (sb);
    pn_get_state (a, blob);
    pn_generate (a, 64, ref, 64); /* reference continuation */

    pn_state_t *b = pn_create (96, 1, 7, PN_GALOIS);
    DP_CHECK (pn_set_state (b, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    DP_CHECK (pn_set_state (b, blob) == DP_ERR_INVALID);
    pn_generate (b, 64, got, 64);
    for (int i = 0; i < 64; i++)
      DP_CHECK (got[i] == ref[i]);
    pn_destroy (a);
    pn_destroy (b);
    free (blob);
  }

  /* ── pass_capacity: emission stops at max_out (jm gh-138) ────────── */
  {
    pn_state_t *g = pn_create (0, 1, 7, 0);
    uint8_t     out[16];
    memset (out, 0xAA, sizeof out);
    /* Ask for 16, allow 5: exactly 5 written, the rest untouched. */
    DP_CHECK (pn_generate (g, 16, out, 5) == 5);
    for (int i = 5; i < 16; i++)
      DP_CHECK (out[i] == 0xAA);
    /* Zero capacity emits nothing and does not advance the register. */
    uint64_t reg_before = g->reg;
    DP_CHECK (pn_generate (g, 16, out, 0) == 0);
    DP_CHECK (g->reg == reg_before);
    pn_destroy (g);
  }

  DP_TEST_END ("test_pn_core");
}
