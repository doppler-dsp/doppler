/**
 * @file test_dp_mf.c
 * @brief `dp_mf_test.h`'s self-test: the matched-filter fixtures.
 *
 * The down-converter suites (`test_ddc_core.c`, `test_ddcr_core.c`) measure
 * the same signal through the complex and the real chain, and both get their
 * stimulus and their verdict from this header. Nothing tested it.
 *
 * ## The claim that needed testing most
 *
 * `mf_evm_db()` takes the **minimum over strobe alignment** — a `min` over
 * `(lag, parity)` of the fitted error. That is the shape `dp_ber_test.h` calls
 * "the historic footgun": *"not a measurement of the receiver, it is an
 * optimisation over the answer"*, which "fails in both directions", including
 * "a wide search on a short window finds a lucky low-error alignment on
 * garbage (false PASS)".
 *
 * Here the search is legitimate and the header says why — open loop, no timing
 * loop has steered the strobe, so the phase genuinely is arbitrary. But
 * "legitimate" is an argument, and the failure mode it is exposed to is
 * measurable: **does the search find a good number on a stream carrying the
 * wrong symbols?** So that is measured rather than reasoned about. A stream
 * built from a different sequence must read badly, or every EVM this header
 * has ever reported is a search result rather than a measurement.
 *
 * The other specific claim is that the fitted complex gain normalises out a
 * constant rotation — "the real chain's halfband leaves one". That is what
 * lets a real-input chain be scored against a complex-input reference at all,
 * so it is asserted as an exact invariance rather than a tendency.
 *
 * And the two traps the header documents get one assertion each, since a
 * documented trap with nothing watching it is a comment.
 */
#include "dp_mf_test.h"
#include "dp_rng_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/** @brief A sequence deliberately UNRELATED to `mf_bit`, for the false-pass
 * probe. Differs from it in about half its positions. */
static int
other_bit (int k)
{
  unsigned x = (unsigned)k * 2654435761u + 7u;
  return ((x >> 13) & 1) ? 1 : -1;
}

/**
 * @brief A stream at 2 samples/symbol carrying @p bit, strobes at @p off.
 *
 * `mf_evm_db` reads only `y[lag + par + 2k]`, so this is the minimal input
 * that exercises its search: the right alignment is present and exact, and
 * everything between strobes is zero. A small noise floor keeps the answer
 * finite rather than `-inf`.
 */
static float _Complex *
strobe_stream (int (*bit) (int), size_t off, float _Complex gain, double noise,
               uint32_t *st, size_t *n_out)
{
  size_t          n = 2 * MF_NSYM + 200;
  float _Complex *y = (float _Complex *)calloc (n, sizeof *y);
  int             k;
  size_t          i;
  if (!y)
    return NULL;
  for (i = 0; i < n; i++)
    y[i] = (float _Complex) (noise * dp_cgauss (st));
  for (k = 0; k < MF_NSYM; k++)
    {
      size_t idx = off + 2u * (size_t)k;
      if (idx < n)
        y[idx] += gain * (float)bit (k);
    }
  *n_out = n;
  return y;
}

/** @brief Peak |x| over a record. */
static double
peak_of (const float _Complex *x, size_t n)
{
  double pk = 0.0;
  size_t i;
  for (i = 0; i < n; i++)
    {
      double a = cabs ((double _Complex)x[i]);
      if (a > pk)
        pk = a;
    }
  return pk;
}

int
main (void)
{
  uint32_t st = 4242u;

  printf ("dp_mf_test.h self-test — the matched-filter fixtures\n");

  /* ── 1. The symbol source is deterministic, +-1, and not constant ─────── */

  {
    int k, pos = 0, neg = 0, stable = 1;
    for (k = 0; k < 4000; k++)
      {
        int b = mf_bit (k);
        if (b != 1 && b != -1)
          stable = 0;
        if (b > 0)
          pos++;
        else
          neg++;
        if (mf_bit (k) != b)
          stable = 0; /* a second call must agree */
      }
    DP_CHECK_MSG (stable, "mf_bit is +-1 and depends only on k");
    /* Both signs, in quantity. A constant sequence would make every EVM below
       meaningless while still looking like a measurement, and `mf_evm_db`
       fits a gain against exactly this. */
    DP_CHECK_MSG (pos > 1500 && neg > 1500,
                  "mf_bit is roughly balanced, not a near-constant sequence");
  }

  /* ── 2. mf_tx's geometry and the CIC bound (trap 2) ───────────────────── */

  {
    size_t          n = 0;
    float _Complex *x = mf_tx (16.0, 0.0, 0.0, &n);
    DP_REQUIRE (x != NULL);
    DP_CHECK (n == (size_t)(MF_NSYM * 16.0) + 64);

    /* Trap 2, as an assertion: "keep the amplitude inside the CIC's +-1.0
       input bound", because a CIC clips silently past it and costs ~25 dB of
       EVM for reasons that have nothing to do with the matched filter. The
       amplitude is 0.25 and an RRC peaks ~1.58x above its symbol amplitude,
       so the headroom is real but not enormous -- which is why it is checked
       rather than assumed. */
    DP_CHECK_MSG (
        peak_of (x, n) < 1.0,
        "the shaped stream stays inside the CIC's +-1.0 input bound");
    DP_CHECK_MSG (peak_of (x, n) > 0.25,
                  "...and does peak above the 0.25 symbol amplitude, so the "
                  "bound above is not satisfied by a silent zero");
    free (x);
  }

  /* fc = 0 is strictly real, and a carrier rotates without changing |x| — the
     property that lets the complex and real chains be compared at all. */
  {
    size_t          nb = 0, nf = 0, i;
    float _Complex *xb       = mf_tx (8.0, 0.0, 0.0, &nb);
    float _Complex *xf       = mf_tx (8.0, 0.0, 0.125, &nf);
    double          worst_im = 0.0, worst_env = 0.0;
    DP_REQUIRE (xb != NULL && xf != NULL);
    DP_CHECK (nb == nf);
    for (i = 0; i < nb; i++)
      {
        double im = fabs (cimag ((double _Complex)xb[i]));
        double e  = fabs (cabs ((double _Complex)xf[i])
                          - fabs (creal ((double _Complex)xb[i])));
        if (im > worst_im)
          worst_im = im;
        if (e > worst_env)
          worst_env = e;
      }
    DP_CHECK_MSG (worst_im == 0.0, "fc = 0 produces a strictly real stream");
    DP_CHECK_MSG (worst_env < 1e-6,
                  "a carrier rotates the stream without changing |x|");
    free (xb);
    free (xf);
  }

  /* `phi` is a timing phase in SYMBOLS — trap 1 is about sweeping it finely
     enough, which only means anything if it moves the waveform at all. */
  {
    size_t          n0 = 0, n1 = 0, i;
    float _Complex *x0   = mf_tx (8.0, 0.0, 0.0, &n0);
    float _Complex *x1   = mf_tx (8.0, 0.25, 0.0, &n1);
    double          diff = 0.0;
    DP_REQUIRE (x0 != NULL && x1 != NULL);
    for (i = 0; i < n0 && i < n1; i++)
      diff += cabs ((double _Complex)x0[i] - (double _Complex)x1[i]);
    DP_CHECK_MSG (diff > 1.0,
                  "phi moves the timing phase, so a sweep over it sweeps "
                  "something");
    free (x0);
    free (x1);
  }

  /* ── 3. mf_evm_db finds the right alignment ───────────────────────────── */

  {
    size_t          n = 0;
    float _Complex *y = strobe_stream (mf_bit, 10, 1.0f, 1e-4, &st, &n);
    double          evm;
    DP_REQUIRE (y != NULL);
    evm = mf_evm_db (y, n);
    DP_CHECK_MSG (evm < -50.0,
                  "a perfectly aligned stream reads a very low EVM — the "
                  "search finds the alignment that is there");
    free (y);
  }

  /* ── 4. The fitted gain normalises out rotation AND scale ─────────────── */

  /* The header's stated reason: "the real chain's halfband leaves [a constant
     rotation]", and normalising it out is what lets one EVM number score both
     chains. An invariance, so it is asserted as one. */
  {
    size_t          n1 = 0, n2 = 0;
    uint32_t        s1 = 999u, s2 = 999u; /* same noise draw in both */
    float _Complex *y1 = strobe_stream (mf_bit, 10, 1.0f, 1e-4, &s1, &n1);
    float _Complex g = (float)(2.5 * cos (0.7)) + (float)(2.5 * sin (0.7)) * I;
    float _Complex *y2 = strobe_stream (mf_bit, 10, g, 1e-4 * 2.5, &s2, &n2);
    DP_REQUIRE (y1 != NULL && y2 != NULL);
    DP_CHECK_NEAR (mf_evm_db (y2, n2), mf_evm_db (y1, n1), 0.5);
    free (y1);
    free (y2);
  }

  /* ── 5. The false-PASS probe — the one that justifies the min ─────────── */

  /* A min over alignment can find a lucky low-error fit on garbage. If it did
     so here, every EVM this header has reported would be a search result
     rather than a measurement. A stream carrying an UNRELATED sequence must
     therefore read badly at every alignment the search tries. */
  {
    size_t          n = 0;
    float _Complex *y = strobe_stream (other_bit, 10, 1.0f, 1e-4, &st, &n);
    double          evm;
    DP_REQUIRE (y != NULL);
    evm = mf_evm_db (y, n);
    DP_CHECK_MSG (evm > -6.0,
                  "a stream carrying the WRONG sequence reads badly at every "
                  "alignment — the min-over-lag search does not manufacture a "
                  "passing number from garbage");
    free (y);
  }

  /* ── 6. And it degrades with noise, monotonically ─────────────────────── */

  /* Without this the assertions above are satisfied by a function returning
     two constants. */
  {
    double prev      = -1e9;
    double noises[4] = { 1e-4, 1e-3, 1e-2, 1e-1 };
    int    i, monotone = 1;
    for (i = 0; i < 4; i++)
      {
        size_t          n = 0;
        uint32_t        s = 31337u;
        float _Complex *y
            = strobe_stream (mf_bit, 10, 1.0f, noises[i], &s, &n);
        double ev;
        DP_REQUIRE (y != NULL);
        ev = mf_evm_db (y, n);
        if (i > 0 && !(ev > prev))
          monotone = 0;
        prev = ev;
        free (y);
      }
    DP_CHECK_MSG (monotone,
                  "EVM worsens monotonically with noise — the estimator "
                  "responds to the thing it claims to measure");
  }

  DP_TEST_END ("test_dp_mf");
}
