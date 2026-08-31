#include "dp_test.h"
#include "gold/gold_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 * the same Gold family (1023 reachable codes), used for the cross-correlation
 * check. */
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

/* memcmp order over two SF-chip codes, for qsort. */
static int
cmp_code (const void *a, const void *b)
{
  return memcmp (*(const uint8_t *const *)a, *(const uint8_t *const *)b, SF);
}

static int
is_gold_valued (int v)
{
  return v == -1 || v == -65 || v == 63;
}

int
main (void)
{

  /* ── construction validation ── */
  DP_CHECK (gold_create (TAPS_A, 0, TAPS_B, SEED_B, 10)
            == NULL); /* seed_a=0 */
  DP_CHECK (gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, 0, 10)
            == NULL); /* seed_b=0 */
  DP_CHECK (gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 0)
            == NULL); /* length=0 */
  DP_CHECK (gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 65)
            == NULL); /* length>64 */

  gold_state_t *g = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
  DP_CHECK (g != NULL);
  if (!g)
    return 1;

  /* ── register A / register B individually maximal-length (period 1023) ── */
  DP_CHECK (single_lfsr_period (TAPS_A, SEED_A_EXAMPLE, 10) == SF);
  DP_CHECK (single_lfsr_period (TAPS_B, SEED_B, 10) == SF);

  /* ── combined Gold sequence: period exactly SF (not a proper divisor) ── */
  DP_CHECK (gold_period (g) == SF);

  /* ── CCSDS worked example (Figure 5-2, Code #365): first 15 chips + the
   * balance property the standard itself calls out (512 ones, 511 zeros) ── */
  {
    gold_state_t *ex
        = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    uint8_t chips[SF];
    gold_generate (ex, SF, chips, SF);
    static const uint8_t expected[15]
        = { 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1 };
    for (int i = 0; i < 15; i++)
      DP_CHECK (chips[i] == expected[i]);
    int ones = 0;
    for (int i = 0; i < SF; i++)
      ones += chips[i];
    DP_CHECK (ones == 512);
    DP_CHECK (SF - ones == 511);
    gold_destroy (ex);
  }

  /* ── three-valued Gold autocorrelation/cross-correlation set {-1,-65,63}:
   * this is the whole point of using a genuine CCSDS preferred pair ── */
  {
    gold_state_t *g1
        = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    gold_state_t *g2 = gold_create (TAPS_A, SEED_A_OTHER, TAPS_B, SEED_B, 10);
    uint8_t       c1[SF], c2[SF];
    gold_generate (g1, SF, c1, SF);
    gold_generate (g2, SF, c2, SF);

    int acorr[SF];
    xcorr_values (c1, c1, acorr);
    DP_CHECK (acorr[0] == SF); /* peak: full correlation with itself */
    for (int k = 1; k < SF; k++)
      DP_CHECK (is_gold_valued (acorr[k]));

    int xcorr[SF];
    xcorr_values (c1, c2, xcorr);
    for (int k = 0; k < SF; k++)
      DP_CHECK (is_gold_valued (xcorr[k]));

    gold_destroy (g1);
    gold_destroy (g2);
  }

  /* ── reset ── */
  {
    uint8_t before[8], after[8];
    gold_generate (g, 8, before, 8);
    gold_reset (g);
    gold_generate (g, 8, after, 8);
    for (int i = 0; i < 8; i++)
      DP_CHECK (before[i] == after[i]);
  }

  gold_destroy (g);

  /* ── serializable state: advance, serialize, restore into a fresh
   * generator, and the chip stream continues identically; clobber rejects ──
   */
  {
    gold_state_t *a = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    uint8_t       ref[64], got[64];
    gold_generate (a, 17, ref,
                   17); /* advance mid-stream, off any epoch boundary */
    size_t sb   = gold_state_bytes (a);
    void  *blob = malloc (sb);
    gold_get_state (a, blob);
    gold_generate (a, 64, ref, 64); /* reference continuation */

    gold_state_t *b = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    DP_CHECK (gold_set_state (b, blob) == DP_OK);
    ((char *)blob)[0] ^= (char)0xFF;
    DP_CHECK (gold_set_state (b, blob) == DP_ERR_INVALID);
    gold_generate (b, 64, got, 64);
    for (int i = 0; i < 64; i++)
      DP_CHECK (got[i] == ref[i]);
    gold_destroy (a);
    gold_destroy (b);
    free (blob);
  }

  /* ── how many codes does seed_a actually reach? ────────────────────────
   * The header said varying seed_a "walks the whole Gold-code family
   * (2^length members)" -- 1024 at length=10. Measured, it is 1023, and
   * the arithmetic was wrong in both directions at once:
   *
   *   - only 2^length - 1 seeds exist, because gold_create rejects zero;
   *   - the classical Gold set for a preferred pair has 2^n + 1 = 1025
   *     members -- the 2^n - 1 XOR combinations PLUS the two constituent
   *     m-sequences, and this generator cannot emit those two at all
   *     because it always XORs both registers.
   *
   * So 1024 was neither the reachable count nor the family size. The
   * header now says 1023 reachable of 1025, and this pins it: a caller
   * sizing a code-assignment scheme reads that number and allocates
   * against it. Distinctness is compared by 64-bit FNV-1a over each full
   * period -- a collision would fail the test, never pass it. */
  {
    /* Every code held at once (1023 x 1023 = ~1 MB), then sorted by
       memcmp and checked adjacent-pairwise. Exact: no hash, so no
       collision to reason about, and nothing here resembles a private
       RNG for check_tests_ssot to object to. */
    uint8_t        *all  = malloc ((size_t)SF * SF);
    const uint8_t **ptrs = malloc ((size_t)SF * sizeof *ptrs);
    DP_CHECK (all && ptrs);
    if (all && ptrs)
      {
        size_t n_seeds = 0;
        for (uint64_t seed_a = 1; seed_a <= (uint64_t)SF; seed_a++)
          {
            gold_state_t *gs
                = gold_create (TAPS_A, seed_a, TAPS_B, SEED_B, 10);
            DP_CHECK (gs != NULL);
            if (!gs)
              continue;
            uint8_t *slot = all + n_seeds * (size_t)SF;
            DP_CHECK (gold_generate (gs, SF, slot, SF) == SF);
            ptrs[n_seeds++] = slot;
            gold_destroy (gs);
          }
        DP_CHECK (n_seeds == (size_t)SF); /* 1023 nonzero seeds */

        qsort (ptrs, n_seeds, sizeof *ptrs, cmp_code);
        int all_distinct = 1;
        for (size_t i = 1; i < n_seeds; i++)
          if (memcmp (ptrs[i], ptrs[i - 1], SF) == 0)
            all_distinct = 0;
        DP_CHECK (all_distinct);
      }
    free (all);
    free (ptrs);
  }

  /* ── the preferred pair is a claim about the FAMILY ────────────────────
   * The three-valued set was checked for ONE pair of members. "Preferred
   * pair" means the property holds between ANY two codes in the family,
   * which is what makes the whole set usable for multiple access -- one
   * good pair says nothing about the other 1022. Sampled across six
   * spread-out members here (15 pairs); the full 1023x1022/2 is the
   * validator's job, not ctest's. */
  {
    static const uint64_t seeds[6] = { 1u, 350u, 595u, 700u, 900u, 1023u };
    uint8_t              *seqs[6];
    int                   ok = 1;
    for (int s = 0; s < 6; s++)
      {
        seqs[s]          = malloc (SF);
        gold_state_t *gs = gold_create (TAPS_A, seeds[s], TAPS_B, SEED_B, 10);
        DP_CHECK (seqs[s] && gs);
        if (seqs[s] && gs)
          gold_generate (gs, SF, seqs[s], SF);
        if (gs)
          gold_destroy (gs);
      }
    int *vals = malloc ((size_t)SF * sizeof *vals);
    DP_CHECK (vals);
    if (vals)
      for (int a = 0; a < 6 && ok; a++)
        for (int b = a + 1; b < 6 && ok; b++)
          {
            xcorr_values (seqs[a], seqs[b], vals);
            for (int k = 0; k < SF; k++)
              if (!is_gold_valued (vals[k]))
                ok = 0;
          }
    DP_CHECK (ok); /* every sampled pair is three-valued */
    free (vals);
    for (int s = 0; s < 6; s++)
      free (seqs[s]);
  }

  /* ── the output contract: capacity, wrapping, and the NULL no-op ───────
   * gold_generate is documented to return min(n, max_out) and to stop
   * emitting at the capacity, and every existing call passed
   * max_out == n, so "emission stops there" was never exercised. */
  {
    gold_state_t *g2
        = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    DP_CHECK (g2 != NULL);
    if (g2)
      {
        uint8_t out[16];
        memset (out, 0xAA, sizeof out);
        /* Ask for 16, allow 5: exactly 5 written, the rest untouched. */
        DP_CHECK (gold_generate (g2, 16, out, 5) == 5);
        for (int i = 5; i < 16; i++)
          DP_CHECK (out[i] == 0xAA);
        /* Zero capacity emits nothing and does not advance either LFSR. */
        uint64_t a_before = g2->reg_a, b_before = g2->reg_b;
        DP_CHECK (gold_generate (g2, 16, out, 0) == 0);
        DP_CHECK (g2->reg_a == a_before && g2->reg_b == b_before);
        gold_destroy (g2);
      }
  }
  {
    /* "Requesting more than one period is valid -- the sequence simply
       wraps around." Two periods back to back must repeat exactly. */
    gold_state_t *g3
        = gold_create (TAPS_A, SEED_A_EXAMPLE, TAPS_B, SEED_B, 10);
    DP_CHECK (g3 != NULL);
    if (g3)
      {
        uint8_t *two = malloc ((size_t)SF * 2);
        DP_CHECK (two != NULL);
        if (two)
          {
            DP_CHECK (gold_generate (g3, (size_t)SF * 2, two, (size_t)SF * 2)
                      == (size_t)SF * 2);
            for (int i = 0; i < SF; i++)
              DP_CHECK (two[i] == two[i + SF]);
          }
        free (two);
        gold_destroy (g3);
      }
  }
  gold_destroy (NULL); /* documented no-op; a crash here is the test */

  DP_TEST_END ("test_gold_core");
}
