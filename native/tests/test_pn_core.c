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

  /* ── the MLS table, verified rather than assumed ───────────────────────
   * The header calls these "verified primitive polynomials (period 2^n-1)".
   * Across BOTH suites that was pinned at six entries -- n=7/17/20 here and
   * n=5/9/40 in test_pn.py -- and the other 57 were asserted only to be
   * NONZERO. A wrong entry is not a crash: it is a spreading code with a
   * short period, which quietly costs the processing gain every DSSS caller
   * is relying on.
   *
   * Stepping is the definitive check and it is affordable up to n=24
   * (~33M steps total, tens of ms). The remaining lengths cannot be walked
   * -- n=64 is 1.8e19 states -- and are certified algebraically in the
   * validator (see §2.2 there), which is cross-checked against THIS loop on
   * the overlap so the algebra is not trusted on its own word. */
  for (uint32_t n = 2; n <= 24; n++)
    {
      const uint64_t poly   = pn_mls_poly (n);
      const long     period = (1L << n) - 1;

      for (int kind = PN_GALOIS; kind <= PN_FIBONACCI; kind++)
        {
          pn_state_t *p = pn_create (poly, 1, n, kind);
          DP_CHECK (p != NULL);
          if (!p)
            continue;

          /* Walk exactly one claimed period, counting output ones. The
             register must return to the seed at 2^n-1 and NOT before. */
          long ones = 0, back_at = 0;
          for (long i = 0; i < period; i++)
            {
              ones += pn_step (p);
              if (p->reg == 1u && back_at == 0)
                back_at = i + 1;
            }
          DP_CHECK (back_at == period); /* maximal: no earlier return */
          /* Balance: an m-sequence has exactly 2^(n-1) ones per period,
             because the register visits every nonzero state once and half
             of those have LSB set. A non-primitive poly fails this too. */
          DP_CHECK (ones == (1L << (n - 1)));
          pn_destroy (p);
        }
    }

  /* ── "they differ only in chip ordering/phase" is a stronger claim than
   * "they differ" ──────────────────────────────────────────────────────
   * Both suites asserted only that Galois != Fibonacci, which is the
   * COMPLEMENT of what the header says. The claim is that the two are one
   * sequence, and measuring it says exactly how: the Fibonacci output is
   * the Galois output read BACKWARDS about index 0,
   *
   *     fib[i] == gal[(P - i) % P],   P = 2^n - 1
   *
   * which is the "ordering" in the header's "chip ordering/phase" doing
   * real work. A plain rotation does NOT exist -- searched all P of them at
   * n=5..11 and found none -- because a Galois LFSR realizes the RECIPROCAL
   * of the polynomial its Fibonacci twin does. Pinning the closed form
   * rather than "some shift aligns them" is what makes this catch a wrong
   * fib_taps derivation: an unrelated m-sequence of the same period and
   * balance passes "they differ", and fails this. */
  for (uint32_t n = 5; n <= 11; n += 2)
    {
      const size_t   period = (size_t)((1L << n) - 1);
      const uint64_t poly   = pn_mls_poly (n);
      uint8_t       *g = malloc (period), *f = malloc (period);
      DP_CHECK (g && f);
      if (!g || !f)
        {
          free (g);
          free (f);
          continue;
        }
      pn_state_t *pg = pn_create (poly, 1, n, PN_GALOIS);
      pn_state_t *pf = pn_create (poly, 1, n, PN_FIBONACCI);
      DP_CHECK (pg && pf);
      pn_generate (pg, period, g, period);
      pn_generate (pf, period, f, period);

      int reversed_ok = 1, identical = 1;
      for (size_t i = 0; i < period; i++)
        {
          if (f[i] != g[(period - i) % period])
            reversed_ok = 0;
          if (f[i] != g[i])
            identical = 0;
        }
      DP_CHECK (reversed_ok); /* one sequence, read the other way */
      DP_CHECK (!identical);  /* and genuinely not the same order */

      pn_destroy (pg);
      pn_destroy (pf);
      free (g);
      free (f);
    }

  /* ── reset actually restarts the sequence ──────────────────────────────
   * `pn_reset (obj)` was CALLED above with nothing asserted after it -- a
   * reset that reloaded nothing at all passed. Python covers this; C did
   * not, and C is the only face the sanitisers run. Advance first, so the
   * register is provably not already at the seed when reset is called. */
  {
    pn_state_t *r = pn_create (pn_mls_poly (7), 1, 7, PN_GALOIS);
    uint8_t     first[32], again[32];
    DP_CHECK (pn_generate (r, 32, first, 32) == 32);
    DP_CHECK (r->reg != r->seed); /* precondition: state really moved */
    pn_reset (r);
    DP_CHECK (r->reg == r->seed);
    DP_CHECK (pn_generate (r, 32, again, 32) == 32);
    for (int i = 0; i < 32; i++)
      DP_CHECK (again[i] == first[i]);
    pn_destroy (r);
  }

  /* ── the constructor's documented rejects, and the NULL no-op ──────────
   * "seed must be non-zero (the all-zero state is a fixed point)" and
   * "length 1..64" are contract, and neither was exercised here. */
  DP_CHECK (pn_create (pn_mls_poly (7), 0, 7, PN_GALOIS) == NULL);
  DP_CHECK (pn_create (pn_mls_poly (7), 1, 0, PN_GALOIS) == NULL);
  pn_destroy (NULL); /* documented no-op; a crash here is the test */

  /* ── "A zero poly is not a polynomial" ─────────────────────────────────
   * The header's own warning: pn_create takes the mask verbatim, so poly=0
   * is a register with no feedback -- it shifts the seed out and then emits
   * zeros forever, giving "a constant field that still looks like a field".
   * Pinned because it is what every caller's `poly ? poly : pn_mls_poly(n)`
   * resolution exists to avoid; if this ever started auto-selecting, that
   * resolution would be silently dead code. */
  {
    const uint32_t n = 7;
    pn_state_t    *z = pn_create (0, 0x7F, n, PN_GALOIS);
    DP_CHECK (z != NULL);
    if (z)
      {
        uint8_t chips[128];
        DP_CHECK (pn_generate (z, 128, chips, 128) == 128);
        /* The seed shifts out over the first `n` chips ... */
        int any_one = 0;
        for (uint32_t i = 0; i < n; i++)
          any_one |= chips[i];
        DP_CHECK (any_one); /* precondition: the seed really was emitted */
        /* ... and everything after it is zero, forever. */
        for (size_t i = n; i < 128; i++)
          DP_CHECK (chips[i] == 0);
        DP_CHECK (z->reg == 0);
        pn_destroy (z);
      }
  }

  DP_TEST_END ("test_pn_core");
}
