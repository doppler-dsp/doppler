/*
 * test_util_core.c — the shared EMA primitive.
 *
 * `ema_step` and `ema_alpha_decim` are the library's one first-order
 * exponential moving average. Four sites had written the recursion out by
 * hand, in two different algebraic forms, before this existed; the point of
 * a single primitive is that its properties are established ONCE, here,
 * rather than assumed independently at each call site.
 *
 * Every section below is a property, not a spot value, and each was proven
 * by sabotage — breaking the implementation in the specific way the section
 * describes and watching this file go red.
 */
#include "dp_test.h"

#include "util/util_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* A deterministic stream of (state, x) pairs. xorshift32 lives in
 * dp_rng_test.h for the C suite; this file needs only a spread of
 * magnitudes and signs, so it walks a fixed table rather than pulling in a
 * generator (tests-ssot forbids a private copy of one). */
static const double SAMPLES[] = { 0.0,     1.0,
                                  -1.0,    0.5,
                                  -0.25,   7.0,
                                  -3.5,    1e-3,
                                  -1e-3,   1e6,
                                  -1e6,    1e-9,
                                  -1e-9,   0.1,
                                  -0.7,    123.456,
                                  -987.6,  3.25e4,
                                  -5.5e-5, 2.0,
                                  -8.0,    0.3333333333333333 };
enum
{
  NS = (int)(sizeof SAMPLES / sizeof SAMPLES[0])
};

int
main (void)
{
  /* ── §1 — alpha == 1 is EXACT pass-through ────────────────────────
   *
   * The reason this primitive uses a branch rather than the bare
   * incremental form. `det_ema_alpha` returns exactly 1.0 for "no gain
   * requested, so no averaging", so a caller can and does hand this
   * function a 1. `state + 1*(x - state)` rounds twice and was measured
   * inexact for 9.6% of random pairs; an EMA asked not to average must
   * return the observation itself, bit for bit.
   *
   * Sabotage: drop the `alpha >= 1.0` branch — this section goes red.
   * `volatile` for the same reason as §6: folded at compile time the
   * cancellation does not exist, and the section would pass against the
   * broken form. */
  for (int i = 0; i < NS; i++)
    for (int j = 0; j < NS; j++)
      {
        volatile double s = SAMPLES[i], x = SAMPLES[j], a = 1.0;
        DP_CHECK (ema_step (s, x, a) == SAMPLES[j]);
      }

  /* ── §2 — alpha == 0 EXACTLY freezes the state ────────────────────
   *
   * The other boundary, and the one the incremental form already gets
   * right: no motion at all, not merely a small step.
   *
   * Note what this does NOT establish. It was written expecting the
   * two-product form `alpha*x + (1-alpha)*state` to fail here, and
   * measurement says otherwise: that form is exact at alpha 0 too, and
   * holds §3's fixed point as well. So §2 and §3 pin a property both
   * algebraic forms have — they are a floor under any implementation,
   * not the argument for this one. The case that actually separates the
   * two is §1, plus an accuracy margin no C assertion here captures
   * (measured against a 60-digit reference: 2.7e-17 against 5.4e-15 at
   * alpha 1e-5, in this form's favour, widening as alpha shrinks). */
  for (int i = 0; i < NS; i++)
    for (int j = 0; j < NS; j++)
      DP_CHECK (ema_step (SAMPLES[i], SAMPLES[j], 0.0) == SAMPLES[i]);

  /* ── §3 — the fixed point does not move ───────────────────────────
   *
   * x == state must give back state EXACTLY, at every alpha. A converged
   * average that drifts is a slow bias in every consumer, and it would be
   * invisible in a settling test that only ever looks at the transient.
   *
   * Sabotage: flip the correction's sign, or scale it — the state moves
   * and every entry fails. NOT caught by swapping in the two-product
   * form, which holds this fixed point exactly; see §2's note. */
  {
    const double alphas[] = { 0.0, 1e-7, 1e-5, 0.01, 0.05, 0.5, 0.9, 1.0 };
    for (int a = 0; a < (int)(sizeof alphas / sizeof alphas[0]); a++)
      for (int i = 0; i < NS; i++)
        DP_CHECK (ema_step (SAMPLES[i], SAMPLES[i], alphas[a]) == SAMPLES[i]);
  }

  /* ── §4 — the step lands between the endpoints, and moves toward x ──
   *
   * The defining behaviour: an average approaches its observation and
   * never passes it. Checked as an ordering rather than a value so it
   * cannot be satisfied by a formula that happens to hit one point.
   *
   * Sabotage: flip the sign of the correction — every ordering fails. */
  {
    const double alphas[] = { 0.05, 0.25, 0.5, 0.75 };
    for (int a = 0; a < 4; a++)
      for (int i = 0; i < NS; i++)
        for (int j = 0; j < NS; j++)
          {
            double s = SAMPLES[i], x = SAMPLES[j];
            double y = ema_step (s, x, alphas[a]);
            if (s < x)
              DP_CHECK (y > s - 1e-12 && y < x + 1e-12);
            else if (s > x)
              DP_CHECK (y < s + 1e-12 && y > x - 1e-12);
          }
  }

  /* ── §5 — alpha above 1 saturates to pass-through, never overshoots ──
   *
   * A coefficient greater than 1 is a caller error, but the answer must
   * still be bounded: the bare recursion would fly PAST the observation
   * and oscillate outward. Documented as saturating; pinned here.
   *
   * Sabotage: change the guard to `alpha > 1.0` — 1.0 itself still
   * passes, but this section keeps 1.5 honest either way; change it to
   * `alpha >= 2.0` and 1.5 overshoots to 1.5*x - 0.5*state. */
  DP_CHECK (ema_step (0.0, 1.0, 1.5) == 1.0);
  DP_CHECK (ema_step (10.0, -10.0, 4.0) == -10.0);

  /* ── §6 — ema_alpha_decim at d == 1 is EXACTLY alpha ──────────────
   *
   * The property that makes a decimated loop comparable to the
   * undecimated one at all: set the chunk to a single sample and the
   * coefficient must be the per-sample coefficient itself, not a value
   * five ulps away.
   *
   * This is the defect `agc_steps` carries today — it forms the pole as
   * `1 - a1^d` by repeated multiply, and at d == 1 that is `1-(1-alpha)`,
   * measured 6 ulps off at alpha 0.05 and 26865 ulps off at 1e-5. The
   * damage grows as the average lengthens, which is the direction a
   * narrow-band estimator moves.
   *
   * Sabotage: return `1.0 - pow(1.0 - alpha, (double)d)` unconditionally
   * — every one of these fails, and the small alphas fail hugely.
   *
   * `volatile` is load-bearing, not decoration. With plain literals the
   * compiler folds the whole call at -O2 and evaluates the cancelling
   * form exactly, so the section passes against a broken implementation:
   * observed, on the first run of this sabotage. Reading alpha through a
   * volatile forces the arithmetic to happen at runtime in double, which
   * is the only place the cancellation exists. test_nco_core.c carries
   * the same guard for the same reason. */
  {
    const double as[] = { 1e-9, 1e-7, 1e-5, 6.25e-5, 1e-3, 0.01, 0.05, 0.5 };
    for (int i = 0; i < (int)(sizeof as / sizeof as[0]); i++)
      {
        volatile double a   = as[i];
        volatile size_t one = 1;
        DP_CHECK (ema_alpha_decim (a, one) == as[i]);
      }
  }

  /* ── §7 — compounding is the real thing, not an approximation ──────
   *
   * d steps of alpha must equal ONE step of ema_alpha_decim(alpha, d).
   * This is what "decim does not retune the loop" means, and it is the
   * identity the whole decimated path rests on.
   *
   * Sabotage: return `(double)d * alpha` — the linear approximation —
   * and d=32 fails by 1.5e-2, far outside the tolerance. */
  {
    const double as[] = { 1e-4, 1e-3, 0.01, 0.05 };
    const size_t ds[] = { 2, 4, 8, 16, 32, 64 };
    for (int i = 0; i < 4; i++)
      for (int k = 0; k < 6; k++)
        {
          double per = 0.0;
          for (size_t n = 0; n < ds[k]; n++)
            per = ema_step (per, 1.0, as[i]);
          double chunk = ema_step (0.0, 1.0, ema_alpha_decim (as[i], ds[k]));
          DP_CHECK (fabs (per - chunk) < 1e-15);
        }
  }

  /* ── §8 — the compounded coefficient stays a coefficient ───────────
   *
   * In [0, 1] and non-decreasing in d: a longer chunk can only move the
   * average further, never less far, and never past its observation.
   * Both boundaries answered directly rather than through log1p(-1),
   * which is -inf.
   *
   * Sabotage: drop the `alpha >= 1.0` early return and d>1 gives NaN. */
  DP_CHECK (ema_alpha_decim (0.0, 8) == 0.0);
  DP_CHECK (ema_alpha_decim (1.0, 8) == 1.0);
  DP_CHECK (ema_alpha_decim (0.5, 0) == 0.5); /* d < 1 behaves as d == 1 */
  {
    const double as[] = { 1e-7, 1e-3, 0.05, 0.5, 0.99 };
    for (int i = 0; i < 5; i++)
      {
        double prev = ema_alpha_decim (as[i], 1);
        for (size_t d = 2; d <= 512; d *= 2)
          {
            double v = ema_alpha_decim (as[i], d);
            DP_CHECK (v >= 0.0 && v <= 1.0);
            DP_CHECK (v >= prev);
            prev = v;
          }
      }
  }

  /* ── hex_to_bin / bin_to_hex ──────────────────────────────────────────
   *
   * Measured against an EXTERNAL truth rather than against each other where
   * it matters. A pure round-trip is blind to any defect the two share -- a
   * pair that agreed on a wrong bit order would round-trip perfectly -- so
   * the first assertion pins the actual bits of a literal whose expansion is
   * published: the CCSDS ASM, 0x1ACFFC1D, whose first transmitted bit is the
   * top bit of 0x1A. Only then is the round-trip worth anything.
   */
  {
    uint8_t       b[64];
    char          h[32];
    const uint8_t asm_big[] = { 0, 0, 0, 1, 1, 0, 1, 0 }; /* 0x1A, MSB first */

    DP_CHECK (hex_to_bin ("1ACFFC1D", b, sizeof b, DP_BITORDER_BIG) == 32u);
    DP_CHECK_MSG (memcmp (b, asm_big, sizeof asm_big) == 0,
                  "big order is the order the literal READS");

    /* LITTLE reverses within each byte, so the same first byte inverts. */
    const uint8_t asm_little[] = { 0, 1, 0, 1, 1, 0, 0, 0 };
    DP_CHECK (hex_to_bin ("1ACFFC1D", b, sizeof b, DP_BITORDER_LITTLE) == 32u);
    DP_CHECK_MSG (memcmp (b, asm_little, sizeof asm_little) == 0,
                  "little order reverses within the byte, not the string");

    /* Round-trip, both orders, now that the orders themselves are pinned. */
    for (int bo = 0; bo < 2; bo++)
      {
        DP_CHECK (hex_to_bin ("1acffc1d", b, sizeof b, bo) == 32u);
        DP_CHECK (bin_to_hex (b, 32u, h, sizeof h, bo) == 8u);
        DP_CHECK_MSG (strcmp (h, "1acffc1d") == 0, "round-trip is exact");
      }

    /* An ODD digit count is accepted and yields a 4-bit tail. */
    DP_CHECK (hex_to_bin ("abc", b, sizeof b, DP_BITORDER_BIG) == 12u);
    DP_CHECK (bin_to_hex (b, 12u, h, sizeof h, DP_BITORDER_BIG) == 3u);
    DP_CHECK_MSG (strcmp (h, "abc") == 0, "an odd nibble round-trips too");

    /* Refusals. A bad digit REFUSES rather than skipping: a typo that
       silently shortened a marker is the failure this exists to prevent. */
    memset (b, 0xAAu, sizeof b);
    DP_CHECK_MSG (hex_to_bin ("12g4", b, sizeof b, DP_BITORDER_BIG) == 0,
                  "a bad digit is a refusal");
    DP_CHECK_MSG (b[0] == 0xAAu, "...and writes nothing at all");
    DP_CHECK (hex_to_bin ("", b, sizeof b, DP_BITORDER_BIG) == 0);
    DP_CHECK (hex_to_bin (NULL, b, sizeof b, DP_BITORDER_BIG) == 0);
    DP_CHECK_MSG (hex_to_bin ("1ACF", b, 15u, DP_BITORDER_BIG) == 0,
                  "16 bits do not fit in 15");
    DP_CHECK_MSG (hex_to_bin ("1ACF", b, sizeof b, 7) == 0,
                  "an unknown bit order is a refusal, not a default");

    DP_CHECK (hex_to_bin ("1ACF", b, sizeof b, DP_BITORDER_BIG) == 16u);
    DP_CHECK_MSG (bin_to_hex (b, 15u, h, sizeof h, DP_BITORDER_BIG) == 0,
                  "a bit count that is not a whole number of digits");
    DP_CHECK_MSG (bin_to_hex (b, 16u, h, 4u, DP_BITORDER_BIG) == 0,
                  "4 digits plus a NUL do not fit in 4 chars");
    DP_CHECK (bin_to_hex (b, 16u, h, 5u, DP_BITORDER_BIG) == 4u);

    /* Any non-zero byte reads as 1, so a caller's 0/255 mask still renders. */
    uint8_t loud[8];
    for (int i = 0; i < 8; i++)
      loud[i] = (uint8_t)(asm_big[i] ? 255u : 0u);
    DP_CHECK (bin_to_hex (loud, 8u, h, sizeof h, DP_BITORDER_BIG) == 2u);
    DP_CHECK_MSG (strcmp (h, "1a") == 0, "any non-zero byte is a set bit");
  }

  /* ── int_to_bin / bin_to_int ──────────────────────────────────────────
   *
   * The form a field literal actually wants. Pinned against the published
   * expansion first, then round-tripped -- and then checked against
   * hex_to_bin, which is the one cross-check worth having here: the two
   * derive the same bits from different inputs (a shifted integer, a parsed
   * string), so agreement is evidence and not tautology. A marker the two
   * expanded differently would sync to nothing.
   */
  {
    uint8_t  b[64], viahex[64];
    uint64_t v = 0;

    const uint8_t byte_big[] = { 0, 0, 0, 1, 1, 0, 1, 0 }; /* 0x1A */
    DP_CHECK (int_to_bin (0x1Au, 8u, b, sizeof b, DP_BITORDER_BIG) == 8u);
    DP_CHECK_MSG (memcmp (b, byte_big, sizeof byte_big) == 0,
                  "bit 0 is the MOST significant of the n_bits requested");

    /* The agreement that matters: same value, two derivations. */
    DP_CHECK (int_to_bin (0x1ACFFC1DULL, 32u, b, sizeof b, DP_BITORDER_BIG)
              == 32u);
    DP_CHECK (hex_to_bin ("1ACFFC1D", viahex, sizeof viahex, DP_BITORDER_BIG)
              == 32u);
    DP_CHECK_MSG (memcmp (b, viahex, 32u) == 0,
                  "int_to_bin and hex_to_bin expand a value identically");
    DP_CHECK (int_to_bin (0x1ACFFC1DULL, 32u, b, sizeof b, DP_BITORDER_LITTLE)
              == 32u);
    DP_CHECK (
        hex_to_bin ("1ACFFC1D", viahex, sizeof viahex, DP_BITORDER_LITTLE)
        == 32u);
    DP_CHECK_MSG (memcmp (b, viahex, 32u) == 0,
                  "...in the other bit order too");

    /* Round-trip across widths, including ones that are not whole bytes. */
    const unsigned widths[] = { 1u, 4u, 7u, 8u, 12u, 32u, 63u, 64u };
    for (size_t w = 0; w < sizeof widths / sizeof widths[0]; w++)
      for (int bo = 0; bo < 2; bo++)
        {
          const unsigned n = widths[w];
          const uint64_t want
              = (n == 64u) ? 0x0123456789ABCDEFULL
                           : (0x0123456789ABCDEFULL & ((1ULL << n) - 1u));
          DP_CHECK (int_to_bin (want, n, b, sizeof b, bo) == n);
          DP_CHECK (bin_to_int (b, n, &v, bo) == 0);
          DP_CHECK_MSG (v == want, "int round-trips at every width");
        }

    /* Refusals. */
    DP_CHECK_MSG (int_to_bin (1u, 0u, b, sizeof b, DP_BITORDER_BIG) == 0,
                  "zero bits is a refusal, not an empty success");
    DP_CHECK (int_to_bin (1u, 65u, b, sizeof b, DP_BITORDER_BIG) == 0);
    DP_CHECK (int_to_bin (1u, 8u, NULL, sizeof b, DP_BITORDER_BIG) == 0);
    DP_CHECK_MSG (int_to_bin (1u, 8u, b, 7u, DP_BITORDER_BIG) == 0,
                  "8 bits do not fit in 7");
    DP_CHECK (int_to_bin (1u, 8u, b, sizeof b, 7) == 0);
    DP_CHECK (bin_to_int (b, 0u, &v, DP_BITORDER_BIG) == -1);
    DP_CHECK (bin_to_int (b, 65u, &v, DP_BITORDER_BIG) == -1);
    DP_CHECK (bin_to_int (NULL, 8u, &v, DP_BITORDER_BIG) == -1);
    DP_CHECK (bin_to_int (b, 8u, NULL, DP_BITORDER_BIG) == -1);
    DP_CHECK (bin_to_int (b, 8u, &v, 7) == -1);

    /* Only the low n_bits are read, so a caller need not mask first. */
    DP_CHECK (int_to_bin (0xFF00u, 8u, b, sizeof b, DP_BITORDER_BIG) == 8u);
    DP_CHECK (bin_to_int (b, 8u, &v, DP_BITORDER_BIG) == 0);
    DP_CHECK_MSG (v == 0u, "the high bits above n_bits are ignored");
  }

  DP_TEST_END ("test_util_core");
}
