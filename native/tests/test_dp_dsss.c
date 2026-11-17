/**
 * @file test_dp_dsss.c
 * @brief `dp_dsss_test.h`'s self-test: the capture's level and structure.
 *
 * The DSSS receiver suites take their capture from this header, so the
 * level it labels is the level every one of their claims is stated at.
 *
 * ## The level is the label (#689)
 *
 * Until #689 the header scaled `dp_cgauss` (`E|z|^2 = 1`) by
 * `sigma / sqrt(2)`, the factor for the other complex-Gaussian convention,
 * so every capture ran 3.01 dB quieter than it claimed, and this file
 * asserted that measured defect so it could not drift unseen. The noise now
 * comes from the shipped `awgn`, sized by dp_awgn_amplitude_for_snr(), and
 * this file asserts the corrected contract: a capture asking for a C/N0
 * delivers it, its noise power counted over both rails. Restoring the
 * `/ sqrt(2)` (or any other per-rail reading) turns it red.
 */
#include "dp_dsss_test.h"
#include "dp_test.h"

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/** @brief The length-7 m-sequence the DSSS suites use. Kept local for the
 * reason the header states: a `static const` array in a shared header draws
 * `-Wunused-const-variable` from every includer that does not touch it. */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/** @brief Mean |x|^2 over `[lo, hi)`. */
static double
mean_power (const float _Complex *x, size_t lo, size_t hi)
{
  double s = 0.0;
  size_t i;
  for (i = lo; i < hi; i++)
    {
      double re = (double)crealf (x[i]), im = (double)cimagf (x[i]);
      s += re * re + im * im;
    }
  return (hi > lo) ? s / (double)(hi - lo) : 0.0;
}

int
main (void)
{
  const size_t SF = 7, SPC = 4, NSYM = 40, PRE = 200000;
  const double FS   = 1.0e6;
  const double TSYM = (double)(SF * SPC);

  printf ("dp_dsss_test.h self-test — the DSSS capture, and its level\n");

  /* ── 1. The capture delivers the C/N0 it is asked for (#689) ─────────── */

  /* cn0 = 60 dB-Hz at fs = 1e6 is a per-sample SNR of 0 dB against the
     unit-power signal, so the noise power must be 1.0, both rails counted:
     any discrepancy is a convention rather than arithmetic. */
  {
    float _Complex *x    = NULL;
    double         *data = NULL;
    size_t          n    = 0;
    double          p;

    dp_dsss_capture (CODE7, SF, SPC, FS, TSYM, 0.0, 60.0, NSYM, PRE, 7u, &x,
                     &n, &data);
    DP_REQUIRE (x != NULL && data != NULL);

    /* The pre-silence is noise ONLY, which is what makes this a clean read of
       the noise level rather than of the capture. 2e5 draws: the mean's
       relative spread is about 1/sqrt(2e5), 0.2%. */
    p = mean_power (x, 0, PRE);
    printf ("  noise power at 60 dB-Hz over 1 MSa/s: %.4f (label 1.0)\n", p);
    DP_CHECK_MSG (fabs (p - 1.0) < 0.02,
                  "the noise power is the label's, sigma^2 over both rails, "
                  "not sigma^2/2 (doppler#689)");
    DP_CHECK_NEAR (10.0 * log10 (p), 0.0, 0.1); /* the label, in dB */
    /* Both rails counted: each carries half, so neither reading of
       "per rail" can pass for the other. */
    {
      double re = 0.0;
      for (size_t i = 0; i < PRE; i++)
        re += (double)crealf (x[i]) * (double)crealf (x[i]);
      DP_CHECK (fabs (re / (double)PRE - 0.5) < 0.01);
    }

    free (x);
    free (data);
  }

  /* ── 2. The capture's structure ───────────────────────────────────────── */

  {
    float _Complex *x    = NULL;
    double         *data = NULL;
    size_t          n = 0, idx, bad = 0;
    /* A very high cn0 makes sigma tiny, so the signal region can be compared
       against its closed form directly. */
    const double CN0 = 120.0;

    dp_dsss_capture (CODE7, SF, SPC, FS, TSYM, 0.0, CN0, NSYM, 64, 7u, &x, &n,
                     &data);
    DP_REQUIRE (x != NULL && data != NULL);

    /* The documented length: pre_silence + n_sym*tsym + 4*sf*spc. */
    DP_CHECK (n == 64 + (size_t)((double)NSYM * TSYM) + 4 * SF * SPC);

    /* The pre-silence really is signal-free — everything before `pre_silence`
       must be noise alone, or every acquisition test built on this file is
       starting mid-signal. */
    DP_CHECK_MSG (mean_power (x, 0, 64) < 1e-4,
                  "the pre-silence carries noise only, at this cn0 "
                  "effectively nothing");

    /* And the signal region is exactly data * code * carrier. At doppler = 0
       the carrier is 1, so each sample is a real +-1 -- the spreading, the
       chip phase and the symbol clock all pinned in one comparison. */
    for (idx = 0; idx < 200; idx++)
      {
        size_t si   = (size_t)((double)idx / TSYM);
        size_t cph  = (idx / SPC) % SF;
        double want = data[si] * (CODE7[cph] & 1 ? -1.0 : 1.0);
        if (fabs ((double)crealf (x[64 + idx]) - want) > 1e-2)
          bad++;
        if (fabs ((double)cimagf (x[64 + idx])) > 1e-2)
          bad++;
      }
    DP_CHECK_MSG (bad == 0,
                  "the signal is data * code * carrier, and at doppler = 0 it "
                  "is strictly real");

    /* The data symbols are +-1, or a BER scored against them is meaningless.
     */
    bad = 0;
    for (idx = 0; idx < NSYM; idx++)
      if (data[idx] != 1.0 && data[idx] != -1.0)
        bad++;
    DP_CHECK_MSG (bad == 0, "the data symbols are exactly +-1");

    free (x);
    free (data);
  }

  /* ── 3. Reproducible from the seed, and only from the seed ────────────── */

  {
    float _Complex *x1 = NULL, *x2 = NULL, *x3 = NULL;
    double         *d1 = NULL, *d2 = NULL, *d3 = NULL;
    size_t          n1 = 0, n2 = 0, n3 = 0, i, same = 0, diff = 0;

    dp_dsss_capture (CODE7, SF, SPC, FS, TSYM, 0.0, 60.0, NSYM, 64, 7u, &x1,
                     &n1, &d1);
    dp_dsss_capture (CODE7, SF, SPC, FS, TSYM, 0.0, 60.0, NSYM, 64, 7u, &x2,
                     &n2, &d2);
    dp_dsss_capture (CODE7, SF, SPC, FS, TSYM, 0.0, 60.0, NSYM, 64, 11u, &x3,
                     &n3, &d3);
    DP_REQUIRE (x1 && x2 && x3 && d1 && d2 && d3);
    DP_CHECK (n1 == n2 && n1 == n3);
    for (i = 0; i < n1; i++)
      {
        if (x1[i] == x2[i])
          same++;
        if (x1[i] != x3[i])
          diff++;
      }
    DP_CHECK_MSG (same == n1,
                  "the same seed gives a bit-identical capture — goal 7, a "
                  "run reproducible from its description");
    DP_CHECK_MSG (diff > n1 / 2,
                  "...and a different seed gives a different one, so "
                  "reproducibility is not a constant");

    /* The header says the data bits and the noise SHARE one stream, which is
       why the seed changes both. */
    same = 0;
    for (i = 0; i < NSYM; i++)
      if (d1[i] != d3[i])
        same++;
    DP_CHECK_MSG (same > 5,
                  "the seed drives the data bits too — one stream, as the "
                  "header states");

    free (x1);
    free (x2);
    free (x3);
    free (d1);
    free (d2);
    free (d3);
  }

  /* ── 4. Doppler: the fixed residual, and the ramp ─────────────────────── */

  /* A fixed residual rotates at exactly `doppler_hz`. Read the phase the
     capture carries against the closed form the docstring states, at a sample
     chosen inside the first symbol so `data` and `code` are both +1-signed and
     known. */
  {
    float _Complex *x    = NULL;
    double         *data = NULL;
    size_t          n = 0, idx = 3;
    double          want, got, sgn;
    const double    DOP = 1000.0;

    dp_dsss_capture (CODE7, SF, SPC, FS, TSYM, DOP, 120.0, NSYM, 0, 7u, &x, &n,
                     &data);
    DP_REQUIRE (x != NULL && data != NULL);
    sgn  = data[0] * (CODE7[(idx / SPC) % SF] & 1 ? -1.0 : 1.0);
    want = 2.0 * DP_DSSS_PI * DOP / FS * (double)idx;
    got  = carg ((double _Complex)x[idx] * sgn);
    DP_CHECK_NEAR (got, want, 1e-3);
    free (x);
    free (data);
  }

  /* The ramp's phase is the INTEGRAL of a linear frequency, `pi*rate*t^2` —
     a factor of two away from the obvious wrong answer, which is exactly the
     kind of thing that reads as a receiver failing to track. */
  {
    float _Complex *x    = NULL;
    double         *data = NULL;
    size_t          n = 0, idx = 997;
    double          t, want, got, sgn;
    const double    RATE = 5.0e5; /* Hz/s */

    dp_dsss_ramp_capture (CODE7, SF, SPC, FS, TSYM, RATE, 120.0, NSYM, 0, 7u,
                          &x, &n, &data);
    DP_REQUIRE (x != NULL && data != NULL);
    t    = (double)idx / FS;
    sgn  = data[(size_t)((double)idx / TSYM)]
           * (CODE7[(idx / SPC) % SF] & 1 ? -1.0 : 1.0);
    want = 2.0 * DP_DSSS_PI * (0.5 * RATE * t * t);
    got  = carg ((double _Complex)x[idx] * sgn);
    DP_CHECK_NEAR (got, want, 1e-3);
    DP_CHECK_MSG (fabs (want) > 1e-3,
                  "the ramp has actually accumulated phase by this sample, so "
                  "the comparison above is not against zero");
    free (x);
    free (data);
  }

  /* At rate = 0 the ramp builder must reduce to a carrier-free capture — the
     degenerate case, which is where a `t^2` sign or factor error would still
     look fine and then diverge everywhere else. */
  {
    float _Complex *x    = NULL;
    double         *data = NULL;
    size_t          n = 0, idx, bad = 0;

    dp_dsss_ramp_capture (CODE7, SF, SPC, FS, TSYM, 0.0, 120.0, NSYM, 0, 7u,
                          &x, &n, &data);
    DP_REQUIRE (x != NULL && data != NULL);
    for (idx = 0; idx < 200; idx++)
      if (fabs ((double)cimagf (x[idx])) > 1e-2)
        bad++;
    DP_CHECK_MSG (bad == 0, "a zero Doppler rate leaves a strictly real "
                            "capture");
    free (x);
    free (data);
  }

  DP_TEST_END ("test_dp_dsss");
}
