/**
 * @file test_dp_rng.c
 * @brief Self-test for dp_rng_test.h — the suite's one random source.
 *
 * Every receiver test in this directory now draws its data bits and its AWGN
 * from `dp_rng_test.h`. That makes it the single point where an edit moves
 * every BER, EVM and lock-metric number in the suite at once, silently and
 * plausibly: noise that is slightly wrong still looks like noise, and a test
 * with margin still passes. That is precisely how the generator this header
 * replaced went five copies deep with one of them measurably broken.
 *
 * So the streams are PINNED here, and the distributions are MEASURED here.
 *
 * ## Two kinds of assertion, because two kinds of guarantee
 *
 * **Exact.** `dp_xs32`, `dp_xs64`, `dp_bit`, `dp_uni` and `dp_uni64` are
 * integer shifts plus IEEE-exact arithmetic — an unsigned-to-double
 * conversion, an addition below 2^33, and one correctly-rounded division.
 * Those are bit-reproducible on every platform doppler builds for, so they
 * are compared bit-for-bit against vectors recorded when this header was
 * cut. A changed shift, a changed uniform mapping or a changed draw order
 * fails immediately and says so.
 *
 * **Statistical.** `dp_gauss`, `dp_cgauss` and `dp_gauss64` go through `log`,
 * `cos` and `sin`. Those are libm, and libm is NOT correctly rounded by any
 * standard — glibc, macOS and the arm64 runners each return their own last
 * ulp. Pinning a Gaussian to its bit pattern would therefore pass on the
 * machine it was recorded on and fail everywhere else, which is a flaky test
 * dressed as a strict one. They are checked to a tolerance that no libm
 * disagreement can breach, and then checked where it actually matters: the
 * distribution.
 *
 * The distribution bounds are set to about ten standard errors at this
 * sample count — far too wide to flake, and still narrow enough to catch the
 * real defect that motivated the header. The generator in
 * `test_costas_core.c` drew both of its Box-Muller uniforms from a degenerate
 * two-shift recurrence, and produced mean +0.056, variance 1.115 and a
 * two-sigma tail of 0.063 against the 0.0455 it should have. Each of those
 * three numbers is outside the bounds asserted below, so this test would
 * have failed on it. That is the standard a gate has to meet: it must reject
 * the bug that already happened.
 */
#include "dp_rng_test.h"
#include "dp_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** The seed every vector below was recorded at. */
#define SEED 12345u

/** Draws for the distribution checks. See the header comment for the
 *  ten-standard-error argument that sets the bounds. */
#define NSTAT 200000

/** Bit equality — the claim is about representations, and `==` would call
 *  -0.0 equal to 0.0 while calling two NaNs unequal. */
static int
biteq (const void *a, const void *b, size_t n)
{
  return memcmp (a, b, n) == 0;
}

static int
exact_double (double got, double want)
{
  return biteq (&got, &want, sizeof got);
}

int
main (void)
{
  /* ── Exact: the integer generator ─────────────────────────────────── */
  {
    static const uint32_t want[]
        = { 0xc6e5747au, 0x652a09afu, 0xa7e08fa0u, 0x748e41eau };
    uint32_t st = SEED;
    for (size_t i = 0; i < sizeof want / sizeof *want; i++)
      DP_CHECK (dp_xs32 (&st) == want[i]);
  }

  /* A million draws through an FNV-1a hash: the head of the stream being
   * right does not mean the period or the state update are. */
  {
    uint32_t st = SEED, h = 2166136261u;
    for (long i = 0; i < 1000000; i++)
      h = (h ^ dp_xs32 (&st)) * 16777619u;
    DP_CHECK (h == 0xfde642f9u);
  }

  /* xorshift is a bijection on the nonzero words, so the state must never
   * reach the absorbing zero from a nonzero start. */
  {
    uint32_t st       = SEED;
    int      hit_zero = 0;
    for (long i = 0; i < 1000000; i++)
      hit_zero |= (dp_xs32 (&st) == 0u);
    DP_CHECK (!hit_zero);
  }

  /* A zero seed is substituted, not propagated — the whole point of the
   * guard, and the one case where a silent failure reads as a passing test
   * (an all-zeros "random" stream asserts fine against almost anything). */
  {
    uint32_t zero = 0u, one = 1u;
    for (int i = 0; i < 16; i++)
      DP_CHECK (dp_xs32 (&zero) == dp_xs32 (&one));
  }

  /* ── Exact: the uniform and the bit ───────────────────────────────── */
  {
    static const double want[] = { 0.77693870529138043, 0.39517269646861297,
                                   0.65577027954725309, 0.4552956769579799 };
    uint32_t            st     = SEED;
    for (size_t i = 0; i < sizeof want / sizeof *want; i++)
      DP_CHECK (exact_double (dp_uni (&st), want[i]));
  }

  /* (0, 1] — open at zero, because log() is the next thing to touch it. */
  {
    uint32_t st  = SEED;
    int      bad = 0;
    for (long i = 0; i < 1000000; i++)
      {
        double u = dp_uni (&st);
        bad += !(u > 0.0 && u <= 1.0);
      }
    DP_CHECK (bad == 0);
  }

  {
    static const int want[] = { 1, -1, 1, 1, -1, 1, 1, 1 };
    uint32_t         st     = SEED;
    for (size_t i = 0; i < sizeof want / sizeof *want; i++)
      DP_CHECK (dp_bit (&st) == want[i]);
  }

  /* ── Exact: the 64-bit generator ──────────────────────────────────── */
  {
    static const uint64_t want[]
        = { 0x00000c163a391e19ull, 0x9c0eb9542f03ca65ull,
            0xa228090ad781f4b1ull };
    uint64_t st = SEED;
    for (size_t i = 0; i < sizeof want / sizeof *want; i++)
      DP_CHECK (dp_xs64 (&st) == want[i]);
  }

  {
    static const double want[]
        = { 7.2043096510654436e-07, 0.60959966950669231, 0.63342339052861252 };
    uint64_t st = SEED;
    for (size_t i = 0; i < sizeof want / sizeof *want; i++)
      DP_CHECK (exact_double (dp_uni64 (&st), want[i]));
  }

  /* ── Tolerant: the Gaussians, to a bound no libm can breach ───────── */
  {
    static const double want[]
        = { -0.56186474243911055, -0.88263358813564619, 0.17235003287434994 };
    uint32_t st = SEED;
    for (size_t i = 0; i < sizeof want / sizeof *want; i++)
      DP_CHECK_NEAR (dp_gauss (&st), want[i], 1e-12);
  }

  {
    /* 1e-6, not 1e-7. `dp_cgauss` returns FLOAT, and a float's ulp at
       |x| >= 1 is 1.192e-7 — larger than a 1e-7 bound, so `im[2]`
       (-1.3314296) could not have absorbed the single-ulp libm difference
       this whole block exists to tolerate. Exactly one of the six values is
       affected, which is why it read as fine: the other five are < 1.0,
       where the ulp is <= 5.96e-8 and there was over a ulp of headroom. A
       tolerance has to be checked against the magnitudes it will see, not
       just against the ones in front of you. (The 1e-12 bounds on the
       `double` generators above have thousands of ulp of room.) */
    static const double re[] = { -0.397298366, -0.624116182, 0.121869877 };
    static const double im[] = { 0.307486296, 0.18006584, -1.3314296 };
    uint32_t            st   = SEED;
    for (size_t i = 0; i < sizeof re / sizeof *re; i++)
      {
        float complex z = dp_cgauss (&st);
        DP_CHECK_NEAR (crealf (z), re[i], 1e-6);
        DP_CHECK_NEAR (cimagf (z), im[i], 1e-6);
      }
  }

  {
    static const double want[]
        = { -4.1065174243983478, 0.65964523271250786, 1.1890021205837233 };
    uint64_t st = SEED;
    for (size_t i = 0; i < sizeof want / sizeof *want; i++)
      DP_CHECK_NEAR (dp_gauss64 (&st), want[i], 1e-12);
  }

  /* ── The distribution: mean, variance, and the two-sigma tail ─────── */
  {
    uint32_t st = SEED;
    double   s = 0.0, s2 = 0.0;
    long     tail = 0;
    double  *v    = malloc (NSTAT * sizeof *v);
    DP_REQUIRE (v != NULL);
    for (long i = 0; i < NSTAT; i++)
      {
        v[i] = dp_gauss (&st);
        s += v[i];
      }
    double mean = s / NSTAT;
    for (long i = 0; i < NSTAT; i++)
      s2 += (v[i] - mean) * (v[i] - mean);
    double var = s2 / NSTAT;
    for (long i = 0; i < NSTAT; i++)
      if (fabs (v[i] - mean) > 2.0 * sqrt (var))
        tail++;
    free (v);

    /* The costas generator scored +0.056 / 1.115 / 0.0630 here. */
    DP_CHECK_NEAR (mean, 0.0, 0.02);
    DP_CHECK_NEAR (var, 1.0, 0.03);
    DP_CHECK_NEAR ((double)tail / NSTAT, 0.0455, 0.005);
  }

  /* dp_cgauss carries E|z|^2 = 1, i.e. 0.5 per component — the convention
   * the receiver tests scale `sigma` against. The sum form
   * (dp_gauss + I*dp_gauss) would land at 2.0 here, so this assertion is
   * what stops the two conventions being quietly swapped. */
  {
    uint32_t st = SEED;
    double   pr = 0.0, pi = 0.0, cross = 0.0;
    for (long i = 0; i < NSTAT; i++)
      {
        float complex z = dp_cgauss (&st);
        pr += (double)crealf (z) * (double)crealf (z);
        pi += (double)cimagf (z) * (double)cimagf (z);
        cross += (double)crealf (z) * (double)cimagf (z);
      }
    pr /= NSTAT;
    pi /= NSTAT;
    cross /= NSTAT;
    DP_CHECK_NEAR (pr, 0.5, 0.02); /* variance per component */
    DP_CHECK_NEAR (pi, 0.5, 0.02);
    DP_CHECK_NEAR (pr + pi, 1.0, 0.03); /* E|z|^2 — the stated contract */
    DP_CHECK_NEAR (cross, 0.0, 0.02);   /* circularly symmetric */
  }

  {
    uint64_t st = SEED;
    double   s = 0.0, s2 = 0.0;
    double  *v = malloc (NSTAT * sizeof *v);
    DP_REQUIRE (v != NULL);
    for (long i = 0; i < NSTAT; i++)
      {
        v[i] = dp_gauss64 (&st);
        s += v[i];
      }
    double mean = s / NSTAT;
    for (long i = 0; i < NSTAT; i++)
      s2 += (v[i] - mean) * (v[i] - mean);
    free (v);
    DP_CHECK_NEAR (mean, 0.0, 0.02);
    DP_CHECK_NEAR (s2 / NSTAT, 1.0, 0.03);
  }

  /* dp_bit is balanced: a data stream with a DC component would bias every
   * BPSK carrier-recovery test that draws from it. */
  {
    uint32_t st  = SEED;
    long     sum = 0;
    for (long i = 0; i < NSTAT; i++)
      sum += dp_bit (&st);
    DP_CHECK_NEAR ((double)sum / NSTAT, 0.0, 0.02);
  }

  DP_TEST_END ("test_dp_rng");
}
