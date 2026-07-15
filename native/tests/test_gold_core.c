#include "gold/gold_core.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

#define SF 1023 /* 2^10 - 1: the CCSDS command-link Gold code period */

/* CCSDS 415.0-G-1 5.2.2.4 (Figure 5-1) fixed taps: Register A
 * x^10+x^9+x^8+x^6+x^3+x^2+1 (stages 2,3,6,8,9,10), Register B
 * x^10+x^6+x^5+x^3+x^2+x^1+1 (stages 1,2,3,5,6,10). Register B's initial
 * value is fixed by the standard (1001001000, stage1..stage10 -> bits
 * 0,3,6). */
#define TAPS_A 934u
#define TAPS_B 567u
#define SEED_B 73u
/* CCSDS Figure 5-2 worked example: Register A initial value, PN Code
 * Library Table 1, Code Number 365 (stage1..stage10 = 0111101010). */
#define SEED_A_EXAMPLE 350u
/* An arbitrary different nonzero Register-A seed -- a different member of
 * the same 1024-code Gold family, used for the cross-correlation check. */
#define SEED_A_OTHER 595u

/* Runs the sequence until the (reg_a, reg_b) pair returns to its initial
 * state; returns the period, or -1 if it exceeds one full period. */
static long
gold_period (gold_state_t *g)
{
  uint64_t a0 = g->reg_a, b0 = g->reg_b;
  long     per = 0;
  do
    {
      gold_step (g);
      per++;
      if (per > SF + 1)
        return -1;
    }
  while (g->reg_a != a0 || g->reg_b != b0);
  return per;
}

/* Standalone single-register Fibonacci LFSR period check (same recurrence
 * as gold_step's per-register update), used to verify Register A and
 * Register B are each independently maximal-length -- duplicated here
 * rather than routed through the combined gold_state_t so a degenerate
 * "other" register can't distort the cycle length being measured. */
static long
single_lfsr_period (uint64_t taps, uint64_t seed, uint32_t length)
{
  uint64_t mask
      = (length >= 64) ? ~(uint64_t)0 : (((uint64_t)1 << length) - 1u);
  uint64_t reg  = seed & mask;
  uint64_t reg0 = reg;
  long     per  = 0;
  do
    {
      uint64_t fb = (uint64_t)__builtin_parityll (reg & taps);
      reg         = ((reg << 1) | fb) & mask;
      per++;
      if (per > SF + 1)
        return -1;
    }
  while (reg != reg0);
  return per;
}

/* Circular periodic correlation (direct O(N^2), fine for one-time N=1023
 * test coverage) between two +-1-mapped chip sequences of length SF. */
static void
xcorr_values (const uint8_t *x, const uint8_t *y, int *out /* len SF */)
{
  for (int k = 0; k < SF; k++)
    {
      long s = 0;
      for (int i = 0; i < SF; i++)
        {
          int xi = x[i] ? -1 : 1;
          int yi = y[(i + k) % SF] ? -1 : 1;
          s += xi * yi;
        }
      out[k] = (int)s;
    }
}

static int
is_gold_valued (int v)
{
  return v == -1 || v == -65 || v == 63;
}

int
main (void)
{
  int _fails = 0;

  /* ── construction validation ── */
  CHECK (gold_create (TAPS_A, 0, TAPS_B, SEED_B, 10) == NULL); /* seed_a=0 */
  CHECK (gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, 0, 10)
         == NULL); /* seed_b=0 */
  CHECK (gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 0)
         == NULL); /* length=0 */
  CHECK (gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 65)
         == NULL); /* length>64 */

  gold_state_t *g = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
  CHECK (g != NULL);
  if (!g)
    return 1;

  /* ── register A / register B individually maximal-length (period 1023) ── */
  CHECK (single_lfsr_period (TAPS_A, SEED_A_EXAMPLE, 10) == SF);
  CHECK (single_lfsr_period (TAPS_B, SEED_B, 10) == SF);

  /* ── combined Gold sequence: period exactly SF (not a proper divisor) ── */
  CHECK (gold_period (g) == SF);

  /* ── CCSDS worked example (Figure 5-2, Code #365): first 15 chips + the
   * balance property the standard itself calls out (512 ones, 511 zeros) ── */
  {
    gold_state_t *ex
        = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    uint8_t chips[SF];
    gold_generate (ex, SF, chips);
    static const uint8_t expected[15]
        = { 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1 };
    for (int i = 0; i < 15; i++)
      CHECK (chips[i] == expected[i]);
    int ones = 0;
    for (int i = 0; i < SF; i++)
      ones += chips[i];
    CHECK (ones == 512);
    CHECK (SF - ones == 511);
    gold_destroy (ex);
  }

  /* ── three-valued Gold autocorrelation/cross-correlation set {-1,-65,63}:
   * this is the whole point of using a genuine CCSDS preferred pair ── */
  {
    gold_state_t *g1
        = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    gold_state_t *g2 = gold_create (TAPS_A, SEED_A_OTHER, TAPS_B, SEED_B, 10);
    uint8_t       c1[SF], c2[SF];
    gold_generate (g1, SF, c1);
    gold_generate (g2, SF, c2);

    int acorr[SF];
    xcorr_values (c1, c1, acorr);
    CHECK (acorr[0] == SF); /* peak: full correlation with itself */
    for (int k = 1; k < SF; k++)
      CHECK (is_gold_valued (acorr[k]));

    int xcorr[SF];
    xcorr_values (c1, c2, xcorr);
    for (int k = 0; k < SF; k++)
      CHECK (is_gold_valued (xcorr[k]));

    gold_destroy (g1);
    gold_destroy (g2);
  }

  /* ── reset ── */
  {
    uint8_t before[8], after[8];
    gold_generate (g, 8, before);
    gold_reset (g);
    gold_generate (g, 8, after);
    for (int i = 0; i < 8; i++)
      CHECK (before[i] == after[i]);
  }

  gold_destroy (g);

  /* ── serializable state: advance, serialize, restore into a fresh
   * generator, and the chip stream continues identically; clobber rejects ──
   */
  {
    gold_state_t *a = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    uint8_t       ref[64], got[64];
    gold_generate (a, 17,
                   ref); /* advance mid-stream, off any epoch boundary */
    size_t sb   = gold_state_bytes (a);
    void  *blob = malloc (sb);
    gold_get_state (a, blob);
    gold_generate (a, 64, ref); /* reference continuation */

    gold_state_t *b = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    CHECK (gold_set_state (b, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    CHECK (gold_set_state (b, blob) == DP_ERR_INVALID);
    gold_generate (b, 64, got);
    for (int i = 0; i < 64; i++)
      CHECK (got[i] == ref[i]);
    gold_destroy (a);
    gold_destroy (b);
    free (blob);
  }

  if (_fails)
    {
      fprintf (stderr, "test_gold_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_gold_core PASSED\n");
  return 0;
}
