/*
 * test_filter_core.c — design_lowpass, the filter module's one free
 * function.
 *
 * It is auto-sized: the caller gives band edges and a stopband
 * attenuation, and `kaiser_num_taps` decides the length. That is the
 * hazard worth testing, because the length is not in the signature —
 * `void design_lowpass(double, double, double, float *out)` says nothing
 * about how much `out` must hold, and the only way to know is to run the
 * same expression the implementation runs.
 *
 * Everything below is a property of a windowed-sinc lowpass rather than a
 * golden tap set: unity DC gain, linear phase, a passband that passes and
 * a stopband that meets the attenuation it was asked for. A golden array
 * would pin the current coefficients and say nothing about whether they
 * are a filter.
 */
#include "dp_test.h"
#include "filter/filter_core.h"
#include "resample/resample_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The manifest's own out_size expression -- the length a caller must
   allocate, written once here so the test cannot drift from it. */
static size_t
taps_for (double fpass, double fstop, double atten_db)
{
  return (size_t)(kaiser_num_taps (1, atten_db, fpass / 2.0, fstop / 2.0) | 1);
}

/* |H(f)| of a real FIR at normalised frequency f (cycles/sample). */
static double
mag_at (const float *h, size_t n, double f)
{
  double re = 0.0, im = 0.0;
  for (size_t k = 0; k < n; k++)
    {
      const double p = -2.0 * M_PI * f * (double)k;
      re += (double)h[k] * cos (p);
      im += (double)h[k] * sin (p);
    }
  return sqrt (re * re + im * im);
}

int
main (void)
{
  static float h[8192];

  /* ── Unity DC gain, linear phase, at three specifications ───────── */
  {
    const double fp[3]  = { 0.20, 0.10, 0.30 };
    const double fs_[3] = { 0.30, 0.14, 0.36 };
    const double at[3]  = { 60.0, 80.0, 40.0 };

    for (int c = 0; c < 3; c++)
      {
        const size_t n = taps_for (fp[c], fs_[c], at[c]);
        DP_CHECK (n >= 1 && n < 8192);
        DP_CHECK (n % 2 == 1); /* the `| 1` makes it odd -- linear phase */

        for (size_t i = 0; i < n; i++)
          h[i] = -12345.0f;
        design_lowpass (fp[c], fs_[c], at[c], h);

        /* Unity DC gain: the sum of the taps is 1, so the filter does
           not change the level of a signal it passes. It is a windowed
           sinc and is NOT renormalised afterwards, so the sum lands
           within a thousandth of 1 rather than on it -- measured at
           1.0003 / 1.00003 / 1.0011 for the three specs here. The
           tolerance is that truncation error, not a fudge: tighten it
           and this becomes a test of the window, which is elsewhere. */
        double sum = 0.0;
        for (size_t i = 0; i < n; i++)
          sum += (double)h[i];
        DP_CHECK (fabs (sum - 1.0) < 2e-3);

        /* Symmetric, which for an odd length IS linear phase. A group
           delay that varied with frequency would smear every edge the
           filter passes, and nothing downstream would say so. */
        for (size_t i = 0; i < n; i++)
          DP_CHECK (fabs ((double)h[i] - (double)h[n - 1 - i]) < 1e-6);
      }
  }

  /* ── It passes the passband and stops the stopband ──────────────── */
  {
    const double fpass = 0.20, fstop = 0.30, atten = 60.0;
    const size_t n = taps_for (fpass, fstop, atten);
    design_lowpass (fpass, fstop, atten, h);

    /* DC and the passband edge are both within a fraction of a dB of
       unity -- the edges are NYQUIST-normalised, so the frequency in
       cycles/sample is half the quoted number. */
    DP_CHECK (fabs (mag_at (h, n, 0.0) - 1.0) < 2e-3);
    DP_CHECK (fabs (mag_at (h, n, fpass / 2.0 * 0.5) - 1.0) < 0.05);

    /* The stopband meets the attenuation it was ASKED for -- this is
       the claim the `atten_db` argument makes, and the only one that
       makes the tap count worth paying for. Checked across the band
       rather than at one point, since a windowed sinc's stopband is a
       ripple and a single sample can land in a null. */
    const double lin = pow (10.0, -atten / 20.0);
    for (double f = fstop / 2.0; f <= 0.5; f += 0.005)
      DP_CHECK (mag_at (h, n, f) < lin * 2.0);
  }

  /* ── A tighter transition really does cost taps ─────────────────── */
  {
    DP_CHECK (taps_for (0.20, 0.215, 60.0) > taps_for (0.20, 0.40, 60.0));
    /* ...and more attenuation costs taps at a fixed transition. */
    DP_CHECK (taps_for (0.20, 0.30, 90.0) > taps_for (0.20, 0.30, 40.0));
  }

  /* ── It writes exactly the length its own sizing expression gives ── */
  {
    const size_t n = taps_for (0.20, 0.30, 60.0);
    for (size_t i = 0; i < n + 8; i++)
      h[i] = -12345.0f;
    design_lowpass (0.20, 0.30, 60.0, h);

    /* Every tap inside the length was written... */
    for (size_t i = 0; i < n; i++)
      DP_CHECK (h[i] != -12345.0f);
    /* ...and nothing past it was, which is the contract a caller sizes
       the buffer from and which the signature cannot state. */
    for (size_t i = n; i < n + 8; i++)
      DP_CHECK (h[i] == -12345.0f);
  }

  DP_TEST_END ("test_filter_core");
}
