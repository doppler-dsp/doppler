#include "doppler_channel/doppler_channel_core.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* SPEC.md's geometry: 3.069 Mcps at spc=2, a 2.5 GHz carrier, and the +/-50
   kHz uncertainty expressed as what it physically is — 20 ppm of the time
   base. */
#define T_FS 6.138e6
#define T_FC 2.5e9
#define T_PPM 20.0
#define T_RATE 0.2 /* ppm/s == 500 Hz/s at 2.5 GHz */
#define T_N 65536u

/* Dominant frequency of a block, by peak of a naive DFT evaluated only near
   the expected bin — enough to confirm the offset without pulling in an FFT
   dependency for one test. */
static double
_peak_hz (const float complex *y, size_t n, double fs, double lo, double hi,
          double step)
{
  double best = 0.0, best_mag = -1.0;
  for (double f = lo; f <= hi; f += step)
    {
      double sr = 0.0, si = 0.0;
      double w = -2.0 * 3.14159265358979323846 * f / fs;
      for (size_t k = 0; k < n; k++)
        {
          double ph = w * (double)k;
          sr += crealf (y[k]) * cos (ph) - cimagf (y[k]) * sin (ph);
          si += crealf (y[k]) * sin (ph) + cimagf (y[k]) * cos (ph);
        }
      double mag = sr * sr + si * si;
      if (mag > best_mag)
        {
          best_mag = mag;
          best     = f;
        }
    }
  return best;
}

int
main (void)
{

  float complex *x = malloc (T_N * sizeof *x);
  DP_CHECK (x != NULL);
  if (!x)
    return 1;
  for (size_t i = 0; i < T_N; i++)
    x[i]
        = 1.0f + 0.0f * I; /* DC — any offset in the output is the channel's */

  /* ---- 1. carrier offset is fc * d ------------------------------------ */
  {
    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, T_PPM, 0.0);
    DP_CHECK (ch != NULL);
    size_t         cap = doppler_channel_execute_max_out (ch);
    float complex *y   = malloc (cap * sizeof *y);
    DP_CHECK (y != NULL);
    size_t n = doppler_channel_execute (ch, x, T_N, y, cap);
    DP_CHECK (n > 0);

    /* +/-2 kHz around the expected 50 kHz, 50 Hz resolution. */
    double f = _peak_hz (y, n < 4096 ? n : 4096, T_FS, 48000.0, 52000.0, 50.0);
    DP_CHECK (dp_nearf (f, T_FC * T_PPM * 1e-6, 200.0f));
    DP_CHECK (dp_nearf (doppler_channel_get_offset_hz (ch), 50000.0, 1.0f));

    /* ---- 2. the time base dilates: n_out ~= n_in / (1 + d) --------- */
    double expect = (double)T_N / (1.0 + T_PPM * 1e-6);
    DP_CHECK (fabs ((double)n - expect) <= 2.0);

    free (y);
    doppler_channel_destroy (ch);
  }

  /* ---- 3. d = 0 is a pass-through in rate and carrier alike ----------- */
  {
    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
    DP_CHECK (ch != NULL);
    size_t         cap = doppler_channel_execute_max_out (ch);
    float complex *y   = malloc (cap * sizeof *y);
    size_t         n   = doppler_channel_execute (ch, x, T_N, y, cap);
    DP_CHECK (n == T_N);
    DP_CHECK (dp_nearf (doppler_channel_get_offset_hz (ch), 0.0, 1e-9f));
    free (y);
    doppler_channel_destroy (ch);
  }

  /* ---- 4. the ramp is the INTEGRAL, not t*d(t) ------------------------ */
  /* The distinguishing test: offset(t) must be fc*d_dot*t, NOT twice that.
     A t*d(t) implementation passes every static-Doppler check above and
     fails only here, which is exactly why this case exists. */
  {
    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, 0.0, T_RATE);
    DP_CHECK (ch != NULL);
    size_t         cap = doppler_channel_execute_max_out (ch);
    float complex *y   = malloc (cap * sizeof *y);
    for (int b = 0; b < 16; b++)
      (void)doppler_channel_execute (ch, x, T_N, y, cap);
    double t = doppler_channel_get_elapsed_s (ch);
    DP_CHECK (t > 0.0);
    DP_CHECK (dp_nearf (doppler_channel_get_offset_hz (ch),
                        T_FC * T_RATE * 1e-6 * t, 0.01f));
    free (y);
    doppler_channel_destroy (ch);
  }

  /* ---- 5. blockwise == one big call (chunk invariance) ---------------- */
  {
    doppler_channel_state_t *a
        = doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    doppler_channel_state_t *b
        = doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    DP_CHECK (a != NULL && b != NULL);
    size_t         cap = doppler_channel_execute_max_out (a);
    float complex *ya  = malloc (cap * sizeof *ya);
    float complex *yb  = malloc (cap * sizeof *yb);
    size_t         na  = doppler_channel_execute (a, x, T_N, ya, cap);

    size_t nb = 0;
    for (size_t off = 0; off < T_N; off += 4096)
      nb += doppler_channel_execute (b, x + off, 4096, yb + nb, cap - nb);

    DP_CHECK (na == nb);
    int same = 1;
    for (size_t k = 0; k < (na < nb ? na : nb); k++)
      if (!dp_cnearf (ya[k], yb[k], 1e-4f))
        {
          same = 0;
          break;
        }
    DP_CHECK (same);
    free (ya);
    free (yb);
    doppler_channel_destroy (a);
    doppler_channel_destroy (b);
  }

  /* ---- 6. mid-stream resume is bit-exact ------------------------------ */
  {
    doppler_channel_state_t *a
        = doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    doppler_channel_state_t *b
        = doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    DP_CHECK (a != NULL && b != NULL);
    size_t         cap = doppler_channel_execute_max_out (a);
    float complex *ya  = malloc (cap * sizeof *ya);
    float complex *yb  = malloc (cap * sizeof *yb);

    /* Run `a` through one block, hand its state to `b`, then run both
       over an identical second block: the outputs must agree exactly. */
    (void)doppler_channel_execute (a, x, 8192, ya, cap);
    size_t cb   = doppler_channel_state_bytes (a);
    void  *blob = malloc (cb);
    DP_CHECK (blob != NULL);
    doppler_channel_get_state (a, blob);
    DP_CHECK (doppler_channel_set_state (b, blob) == DP_OK);

    size_t na = doppler_channel_execute (a, x, 8192, ya, cap);
    size_t nb = doppler_channel_execute (b, x, 8192, yb, cap);
    DP_CHECK (na == nb);
    int same = 1;
    for (size_t k = 0; k < (na < nb ? na : nb); k++)
      if (ya[k] != yb[k])
        {
          same = 0;
          break;
        }
    DP_CHECK (same);

    free (blob);
    free (ya);
    free (yb);
    doppler_channel_destroy (a);
    doppler_channel_destroy (b);
  }

  /* ---- 7. the standard round-trip + envelope reject ------------------- */
  {
    doppler_channel_state_t *a
        = doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    doppler_channel_state_t *b
        = doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    DP_CHECK (a != NULL && b != NULL);
    size_t         cap = doppler_channel_execute_max_out (a);
    float complex *y   = malloc (cap * sizeof *y);
    (void)doppler_channel_execute (a, x, 4096, y, cap);
    DP_STATE_ROUNDTRIP_TEST (doppler_channel, a, b);
    free (y);
    doppler_channel_destroy (a);
    doppler_channel_destroy (b);
  }

  /* ---- 8. invalid configuration is rejected, not silently accepted ---- */
  DP_CHECK (doppler_channel_create (0.0, T_FC, 0.0, 0.0) == NULL);
  DP_CHECK (doppler_channel_create (-1.0, T_FC, 0.0, 0.0) == NULL);
  /* d <= -1 (scale <= 0) would stop or reverse time. Use d = -2 (well inside
   * the rejected region) rather than the exact d = -1 boundary: 1 +
   * (-1e6)*1e-6 is not representable as exactly 0, so it lands at +/-1e-17
   * depending on the platform's FP evaluation (rejected on x86, accepted on
   * arm64/macOS) -- testing the unrepresentable boundary is inherently
   * non-portable. */
  DP_CHECK (doppler_channel_create (T_FS, T_FC, -2e6, 0.0) == NULL);

  /* ---- 9. reset returns both clocks to zero --------------------------- */
  {
    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, T_PPM, 0.0);
    DP_CHECK (ch != NULL);
    size_t         cap = doppler_channel_execute_max_out (ch);
    float complex *y   = malloc (cap * sizeof *y);
    (void)doppler_channel_execute (ch, x, T_N, y, cap);
    DP_CHECK (doppler_channel_get_elapsed_s (ch) > 0.0);
    doppler_channel_reset (ch);
    DP_CHECK (dp_nearf (doppler_channel_get_elapsed_s (ch), 0.0, 1e-12f));
    free (y);
    doppler_channel_destroy (ch);
  }

  /* ---- 10. a CONSTANT profile is the scalar, by another route -------- */
  {
    /* The profile is absolute, so a flat profile at T_PPM must reproduce
       create(..., T_PPM, 0) -- same output count, same samples. Not asserted
       bit-exact: the two routes reach the resampler's rate as `base + 0` and
       as `1 + (base - 1)`, which is the same number and not the same
       floating-point sum. */
    double *prof = malloc (T_N * sizeof *prof);
    for (size_t i = 0; i < T_N; i++)
      prof[i] = T_PPM;

    doppler_channel_state_t *a = doppler_channel_create (T_FS, T_FC, T_PPM, 0);
    doppler_channel_state_t *b = doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
    DP_CHECK (a != NULL && b != NULL);

    size_t         ca = doppler_channel_execute_max_out (a);
    size_t         cb = doppler_channel_profile_max_out (prof, T_N);
    float complex *ya = malloc (ca * sizeof *ya);
    float complex *yb = malloc (cb * sizeof *yb);
    size_t         na = doppler_channel_execute (a, x, T_N, ya, ca);
    size_t nb = doppler_channel_execute_profile (b, x, T_N, prof, T_N, yb, cb);

    DP_CHECK (na > 0 && na == nb);
    double worst = 0.0;
    for (size_t i = 0; i < na; i++)
      {
        double e = cabs ((double complex)ya[i] - (double complex)yb[i]);
        if (e > worst)
          worst = e;
      }
    DP_CHECK_MSG (worst < 2e-3,
                  "a flat profile does not reproduce the scalar route");

    /* And the diagnostic follows the profile rather than the closed form. */
    DP_CHECK_NEAR (doppler_channel_get_offset_hz (b), T_FC * T_PPM * 1e-6,
                   1.0);

    free (ya);
    free (yb);
    free (prof);
    doppler_channel_destroy (a);
    doppler_channel_destroy (b);
  }

  /* ---- 11. a LINEAR profile is the ramp, by another route ------------- */
  {
    double *prof = malloc (T_N * sizeof *prof);
    for (size_t i = 0; i < T_N; i++)
      prof[i] = T_PPM + T_RATE * ((double)i / T_FS);

    doppler_channel_state_t *a
        = doppler_channel_create (T_FS, T_FC, T_PPM, T_RATE);
    doppler_channel_state_t *b = doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
    DP_CHECK (a != NULL && b != NULL);

    size_t         ca = doppler_channel_execute_max_out (a);
    size_t         cb = doppler_channel_profile_max_out (prof, T_N);
    float complex *ya = malloc (ca * sizeof *ya);
    float complex *yb = malloc (cb * sizeof *yb);
    size_t         na = doppler_channel_execute (a, x, T_N, ya, ca);
    size_t nb = doppler_channel_execute_profile (b, x, T_N, prof, T_N, yb, cb);

    DP_CHECK (na > 0 && na == nb);
    double worst = 0.0;
    for (size_t i = 0; i < na; i++)
      {
        double e = cabs ((double complex)ya[i] - (double complex)yb[i]);
        if (e > worst)
          worst = e;
      }
    DP_CHECK_MSG (worst < 5e-3,
                  "a linear profile does not reproduce the ramp");

    free (ya);
    free (yb);
    free (prof);
    doppler_channel_destroy (a);
    doppler_channel_destroy (b);
  }

  /* ---- 12. the carrier follows a profile the closed form cannot --- */
  {
    /* A SIGN CHANGE mid-record: no (d0, d_dot) reproduces it, which is the
       whole reason the array form exists. Measure the offset in each half
       and check it tracks. */
    size_t  n    = T_N;
    double *prof = malloc (n * sizeof *prof);
    for (size_t i = 0; i < n; i++)
      prof[i] = (i < n / 2) ? +20.0 : -20.0;

    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
    DP_CHECK (ch != NULL);
    size_t         cap = doppler_channel_profile_max_out (prof, n);
    float complex *y   = malloc (cap * sizeof *y);
    size_t m = doppler_channel_execute_profile (ch, x, n, prof, n, y, cap);
    DP_CHECK (m > 1024);

    double f1 = _peak_hz (y, m / 2, T_FS, 40e3, 60e3, 200.0);
    double f2 = _peak_hz (y + m / 2, m / 2, T_FS, -60e3, -40e3, 200.0);
    DP_CHECK_NEAR (f1, +50e3, 2e3);
    DP_CHECK_NEAR (f2, -50e3, 2e3);
    /* The final diagnostic reports where the profile ended, not where a
       ramp through those points would have gone. */
    DP_CHECK_NEAR (doppler_channel_get_offset_hz (ch), -50e3, 1e3);

    free (y);
    free (prof);
    doppler_channel_destroy (ch);
  }

  /* ---- 13. an invalid profile writes NOTHING, not a valid prefix ----- */
  {
    double *prof = malloc (T_N * sizeof *prof);
    for (size_t i = 0; i < T_N; i++)
      prof[i] = 10.0;
    prof[T_N - 1] = -2e6; /* time reversed, in the LAST sample */

    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
    DP_CHECK (ch != NULL);
    float complex *y = malloc (T_N * sizeof *y);

    /* The bad sample is last, so a check folded into the chunk loop would
       have emitted every earlier sample before noticing. */
    DP_CHECK (doppler_channel_profile_max_out (prof, T_N) == 0);
    DP_CHECK (doppler_channel_execute_profile (ch, x, T_N, prof, T_N, y, T_N)
              == 0);
    DP_CHECK (doppler_channel_get_elapsed_s (ch) == 0.0);

    /* A profile of the wrong length is refused, not read short. */
    for (size_t i = 0; i < T_N; i++)
      prof[i] = 10.0;
    DP_CHECK (
        doppler_channel_execute_profile (ch, x, T_N, prof, T_N - 1, y, T_N)
        == 0);
    prof[T_N - 1] = -2e6;

    /* NULL guards, including the profile itself. */
    DP_CHECK (doppler_channel_execute_profile (ch, x, T_N, NULL, T_N, y, T_N)
              == 0);
    DP_CHECK (
        doppler_channel_execute_profile (ch, NULL, T_N, prof, T_N, y, T_N)
        == 0);

    free (y);
    free (prof);
    doppler_channel_destroy (ch);
  }

  /* ---- 14. a profile stream resumes bit-exact across a split --------- */
  {
    /* The accumulator IS the carrier phase here, so a blob that dropped it
       would resume at zero excess and step the phase at the seam. */
    size_t  n    = 8192;
    double *prof = malloc (n * sizeof *prof);
    for (size_t i = 0; i < n; i++)
      prof[i] = 25.0 * cos (3.14159265358979 * (double)i / (double)n);

    doppler_channel_state_t *a = doppler_channel_create (T_FS, T_FC, 0, 0);
    doppler_channel_state_t *b = doppler_channel_create (T_FS, T_FC, 0, 0);
    DP_CHECK (a != NULL && b != NULL);

    size_t         cap = doppler_channel_profile_max_out (prof, n);
    float complex *ya  = malloc (cap * sizeof *ya);
    float complex *yb  = malloc (cap * sizeof *yb);

    /* a: straight through. b: first half, hand the state over, second half. */
    size_t na = doppler_channel_execute_profile (a, x, n, prof, n, ya, cap);
    size_t h
        = doppler_channel_execute_profile (b, x, n / 2, prof, n / 2, yb, cap);

    size_t sz   = doppler_channel_state_bytes (b);
    void  *blob = malloc (sz);
    doppler_channel_get_state (b, blob);
    doppler_channel_state_t *c = doppler_channel_create (T_FS, T_FC, 0, 0);
    DP_CHECK (c != NULL);
    DP_CHECK (doppler_channel_set_state (c, blob) == DP_OK);
    size_t h2 = doppler_channel_execute_profile (
        c, x + n / 2, n / 2, prof + n / 2, n / 2, yb + h, cap - h);

    DP_CHECK_MSG (h + h2 == na,
                  "a split profile stream produced a different count");
    size_t bad = na;
    for (size_t i = 0; i < na && bad == na; i++)
      if (ya[i] != yb[i])
        bad = i;
    DP_CHECK_MSG (bad == na, "resume is not bit-exact across the split");

    /* A clobbered envelope is refused rather than reinterpreted. */
    ((uint8_t *)blob)[0] ^= 0xFFu;
    DP_CHECK (doppler_channel_set_state (c, blob) == DP_ERR_INVALID);

    free (blob);
    free (ya);
    free (yb);
    free (prof);
    doppler_channel_destroy (a);
    doppler_channel_destroy (b);
    doppler_channel_destroy (c);
  }

  free (x);
  DP_TEST_END ("test_doppler_channel_core");
}
