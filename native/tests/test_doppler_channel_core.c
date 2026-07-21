#include "doppler_channel/doppler_channel_core.h"
#include "dp_state_test.h"
#include <complex.h>
#include <math.h>
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

static inline int
_almost_eq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}
static inline int
_almost_eq_c (float complex a, float complex b, float tol)
{
  return _almost_eq (crealf (a), crealf (b), tol)
         && _almost_eq (cimagf (a), cimagf (b), tol);
}
#define ALMOST_EQ(a, b, tol) _almost_eq ((float)(a), (float)(b), tol)
#define ALMOST_EQ_C(a, b, tol)                                                \
  _almost_eq_c ((float complex) (a), (float complex) (b), tol)

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
  int _fails = 0;

  float complex *x = malloc (T_N * sizeof *x);
  CHECK (x != NULL);
  if (!x)
    return 1;
  for (size_t i = 0; i < T_N; i++)
    x[i]
        = 1.0f + 0.0f * I; /* DC — any offset in the output is the channel's */

  /* ---- 1. carrier offset is fc * d ------------------------------------ */
  {
    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, T_PPM, 0.0);
    CHECK (ch != NULL);
    size_t         cap = doppler_channel_execute_max_out (ch);
    float complex *y   = malloc (cap * sizeof *y);
    CHECK (y != NULL);
    size_t n = doppler_channel_execute (ch, x, T_N, y, cap);
    CHECK (n > 0);

    /* +/-2 kHz around the expected 50 kHz, 50 Hz resolution. */
    double f = _peak_hz (y, n < 4096 ? n : 4096, T_FS, 48000.0, 52000.0, 50.0);
    CHECK (ALMOST_EQ (f, T_FC * T_PPM * 1e-6, 200.0f));
    CHECK (ALMOST_EQ (doppler_channel_get_offset_hz (ch), 50000.0, 1.0f));

    /* ---- 2. the time base dilates: n_out ~= n_in / (1 + d) --------- */
    double expect = (double)T_N / (1.0 + T_PPM * 1e-6);
    CHECK (fabs ((double)n - expect) <= 2.0);

    free (y);
    doppler_channel_destroy (ch);
  }

  /* ---- 3. d = 0 is a pass-through in rate and carrier alike ----------- */
  {
    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, 0.0, 0.0);
    CHECK (ch != NULL);
    size_t         cap = doppler_channel_execute_max_out (ch);
    float complex *y   = malloc (cap * sizeof *y);
    size_t         n   = doppler_channel_execute (ch, x, T_N, y, cap);
    CHECK (n == T_N);
    CHECK (ALMOST_EQ (doppler_channel_get_offset_hz (ch), 0.0, 1e-9f));
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
    CHECK (ch != NULL);
    size_t         cap = doppler_channel_execute_max_out (ch);
    float complex *y   = malloc (cap * sizeof *y);
    for (int b = 0; b < 16; b++)
      (void)doppler_channel_execute (ch, x, T_N, y, cap);
    double t = doppler_channel_get_elapsed_s (ch);
    CHECK (t > 0.0);
    CHECK (ALMOST_EQ (doppler_channel_get_offset_hz (ch),
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
    CHECK (a != NULL && b != NULL);
    size_t         cap = doppler_channel_execute_max_out (a);
    float complex *ya  = malloc (cap * sizeof *ya);
    float complex *yb  = malloc (cap * sizeof *yb);
    size_t         na  = doppler_channel_execute (a, x, T_N, ya, cap);

    size_t nb = 0;
    for (size_t off = 0; off < T_N; off += 4096)
      nb += doppler_channel_execute (b, x + off, 4096, yb + nb, cap - nb);

    CHECK (na == nb);
    int same = 1;
    for (size_t k = 0; k < (na < nb ? na : nb); k++)
      if (!ALMOST_EQ_C (ya[k], yb[k], 1e-4f))
        {
          same = 0;
          break;
        }
    CHECK (same);
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
    CHECK (a != NULL && b != NULL);
    size_t         cap = doppler_channel_execute_max_out (a);
    float complex *ya  = malloc (cap * sizeof *ya);
    float complex *yb  = malloc (cap * sizeof *yb);

    /* Run `a` through one block, hand its state to `b`, then run both
       over an identical second block: the outputs must agree exactly. */
    (void)doppler_channel_execute (a, x, 8192, ya, cap);
    size_t cb   = doppler_channel_state_bytes (a);
    void  *blob = malloc (cb);
    CHECK (blob != NULL);
    doppler_channel_get_state (a, blob);
    CHECK (doppler_channel_set_state (b, blob) == DP_OK);

    size_t na = doppler_channel_execute (a, x, 8192, ya, cap);
    size_t nb = doppler_channel_execute (b, x, 8192, yb, cap);
    CHECK (na == nb);
    int same = 1;
    for (size_t k = 0; k < (na < nb ? na : nb); k++)
      if (ya[k] != yb[k])
        {
          same = 0;
          break;
        }
    CHECK (same);

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
    CHECK (a != NULL && b != NULL);
    size_t         cap = doppler_channel_execute_max_out (a);
    float complex *y   = malloc (cap * sizeof *y);
    (void)doppler_channel_execute (a, x, 4096, y, cap);
    DP_STATE_ROUNDTRIP_TEST (doppler_channel, a, b);
    free (y);
    doppler_channel_destroy (a);
    doppler_channel_destroy (b);
  }

  /* ---- 8. invalid configuration is rejected, not silently accepted ---- */
  CHECK (doppler_channel_create (0.0, T_FC, 0.0, 0.0) == NULL);
  CHECK (doppler_channel_create (-1.0, T_FC, 0.0, 0.0) == NULL);
  /* d <= -1 (scale <= 0) would stop or reverse time. Use d = -2 (well inside
   * the rejected region) rather than the exact d = -1 boundary: 1 +
   * (-1e6)*1e-6 is not representable as exactly 0, so it lands at +/-1e-17
   * depending on the platform's FP evaluation (rejected on x86, accepted on
   * arm64/macOS) -- testing the unrepresentable boundary is inherently
   * non-portable. */
  CHECK (doppler_channel_create (T_FS, T_FC, -2e6, 0.0) == NULL);

  /* ---- 9. reset returns both clocks to zero --------------------------- */
  {
    doppler_channel_state_t *ch
        = doppler_channel_create (T_FS, T_FC, T_PPM, 0.0);
    CHECK (ch != NULL);
    size_t         cap = doppler_channel_execute_max_out (ch);
    float complex *y   = malloc (cap * sizeof *y);
    (void)doppler_channel_execute (ch, x, T_N, y, cap);
    CHECK (doppler_channel_get_elapsed_s (ch) > 0.0);
    doppler_channel_reset (ch);
    CHECK (ALMOST_EQ (doppler_channel_get_elapsed_s (ch), 0.0, 1e-12f));
    free (y);
    doppler_channel_destroy (ch);
  }

  free (x);
  if (_fails)
    {
      fprintf (stderr, "test_doppler_channel_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_doppler_channel_core PASSED\n");
  return 0;
}
