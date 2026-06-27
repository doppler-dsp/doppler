/**
 * @file test_carrier_nda_core.c
 * @brief Unit tests for the non-data-aided M-th-power carrier loop.
 *
 * Tests:
 *   1. Lifecycle / param validation / init==create parity
 *   2. Arm integrate-and-dump cadence — n dumps per symbol
 *   3. The M-th-power discriminator: phase_error = scaled Im(z^M) (zero with
 *      positive slope at lock; period-2pi/M sawtooth), lock = scaled Re(z^M)
 *   4. Cold-start pull-in on an UNMODULATED carrier (no data)
 *   5. Cold-start pull-in on MODULATED data with NO symbol timing (the
 * headline)
 *   6. Reset reproducibility
 */
#include "carrier_nda/carrier_nda_core.h"
#include "mpsk/mpsk_core.h"
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

#define TWOPI 6.283185307179586

static uint32_t
xs (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return x;
}
static float
gauss (uint32_t *st)
{
  double r1 = (xs (st) + 1.0) / 4294967297.0;
  double r2 = (xs (st) + 1.0) / 4294967297.0;
  return (float)(sqrt (-2.0 * log (r1)) * cos (TWOPI * r2));
}

/* Run the loop over a built signal; report tracked freq + lock. */
static void
run (carrier_nda_state_t *c, const float complex *rx, size_t n, double *f,
     double *lk)
{
  float complex *o = malloc (n * sizeof (*o));
  carrier_nda_steps (c, rx, n, o, n);
  *f  = carrier_nda_get_norm_freq (c);
  *lk = carrier_nda_get_lock (c);
  free (o);
}

int
main (void)
{
  int _fails = 0;

  /* ---------------------------------------------------------------- *
   * 1. Lifecycle, param validation, init==create parity              *
   * ---------------------------------------------------------------- */
  {
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.01, 8, 4, 4);
    CHECK (c != NULL);
    if (!c)
      return 1;
    CHECK (c->lf.kp > 0.0 && c->lf.ki > 0.0);
    CHECK (fabs (carrier_nda_get_norm_freq (c) - 0.01) < 1e-12);
    CHECK (carrier_nda_get_m (c) == 4);
    CHECK (carrier_nda_get_n (c) == 4);
    CHECK (carrier_nda_get_sps (c) == 8);
    CHECK (c->arm_len == 2); /* sps/n = 8/4 */

    carrier_nda_state_t v;
    carrier_nda_init (&v, 0.01, 0.707, 0.01, 8, 4, 4);
    CHECK (v.lf.kp == c->lf.kp && v.lf.ki == c->lf.ki);
    CHECK (v.nco.phase_inc == c->nco.phase_inc);
    CHECK (v.arm_len == c->arm_len && v.lock_scale == c->lock_scale);
    carrier_nda_destroy (c);

    /* M in {2,4,8}; sps % n == 0; n > 0; sps > 0 */
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 2) != NULL);
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 8) != NULL);
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 3) == NULL);
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 3, 4) == NULL); /* 8%3 */
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 8, 0, 4) == NULL);
    CHECK (carrier_nda_create (0.01, 0.707, 0.0, 0, 4, 4) == NULL);
  }

  /* ---------------------------------------------------------------- *
   * 2. Arm integrate-and-dump cadence — n dumps per symbol           *
   * ---------------------------------------------------------------- */
  {
    int                  sps = 8, n = 4;
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, sps, n, 4);
    int                  dumps = 0;
    for (int i = 0; i < sps; i++) /* one symbol worth of samples */
      {
        double pe, lk;
        if (carrier_nda_arm_step (c, 1.0f + 0.0f * I, &pe, &lk))
          dumps++;
      }
    CHECK (dumps == n); /* exactly n dumps per symbol */
    carrier_nda_destroy (c);
  }

  /* ---------------------------------------------------------------- *
   * 3. The M-th-power discriminator characteristic                   *
   * ---------------------------------------------------------------- */
  {
    for (int mi = 0; mi < 3; mi++)
      {
        int    m     = (mi == 0) ? 2 : (mi == 1) ? 4 : 8;
        double scale = (m == 2) ? 1.0 : (m == 4) ? 0.619 : 0.412;
        double seg   = TWOPI / m;
        double pe0, lk0;
        carrier_nda_disc (1.0f + 0.0f * I, m, scale, &pe0, &lk0);
        CHECK (fabs (pe0) < 1e-9); /* e(0) = 0          */
        CHECK (lk0 > 0.0);         /* lock peaks at 0   */
        /* constant-gain property: phase_error slope at 0 is ~2 for all M */
        double h = 1e-3 / m, peh, pemh, lk;
        carrier_nda_disc ((float complex)cexp (I * h), m, scale, &peh, &lk);
        carrier_nda_disc ((float complex)cexp (-I * h), m, scale, &pemh, &lk);
        double slope = (peh - pemh) / (2.0 * h);
        CHECK (fabs (slope - 2.0) < 2e-2);
        /* sawtooth period 2pi/M: e(phi) == e(phi + 2pi/M) */
        double pa, pb;
        carrier_nda_disc ((float complex)cexp (I * 0.05), m, scale, &pa, &lk);
        carrier_nda_disc ((float complex)cexp (I * (0.05 + seg)), m, scale,
                          &pb, &lk);
        CHECK (fabs (pa - pb) < 1e-6);
      }
  }

  /* ---------------------------------------------------------------- *
   * 4. Cold-start pull-in on an UNMODULATED carrier (no data)        *
   * ---------------------------------------------------------------- */
  {
    size_t         N    = 40000;
    float complex *rx   = malloc (N * sizeof (*rx));
    double         f0   = 0.001;
    int            ms[] = { 2, 4, 8 };
    for (int mi = 0; mi < 3; mi++)
      {
        uint32_t ns = 5u;
        for (size_t k = 0; k < N; k++)
          rx[k] = (float complex)cexp (I * TWOPI * f0 * (double)k)
                  + 0.05f * gauss (&ns) + 0.05f * gauss (&ns) * I;
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, ms[mi]);
        double f, lk;
        run (c, rx, N, &f, &lk);
        CHECK (fabs (f - f0) < 5e-4);     /* acquired the bare carrier */
        CHECK (lk > 0.3 * c->lock_scale); /* locked (scaled metric)    */
        carrier_nda_destroy (c);
      }
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 5. Cold-start on MODULATED data with NO symbol timing            *
   * ---------------------------------------------------------------- */
  {
    int            sps  = 8;
    size_t         nsym = 6000, N = nsym * (size_t)sps;
    float complex *rx   = malloc (N * sizeof (*rx));
    double         f0   = 0.001;
    int            ms[] = { 2, 4, 8 };
    for (int mi = 0; mi < 3; mi++)
      {
        int      m  = ms[mi];
        uint32_t ds = 99u, ns = 7u;
        for (size_t s = 0; s < nsym; s++)
          {
            float complex a
                = mpsk_constellation ((int)(xs (&ds) % (uint32_t)m), m);
            for (int i = 0; i < sps; i++)
              {
                size_t k = s * (size_t)sps + (size_t)i;
                rx[k]    = a * (float complex)cexp (I * TWOPI * f0 * (double)k)
                           + 0.1f * gauss (&ns) + 0.1f * gauss (&ns) * I;
              }
          }
        carrier_nda_state_t *c
            = carrier_nda_create (0.01, 0.707, 0.0, sps, 4, m);
        double f, lk;
        run (c, rx, N, &f, &lk);
        CHECK (fabs (f - f0) < 5e-4); /* locked despite NO timing  */
        CHECK (lk > 0.3 * c->lock_scale);
        carrier_nda_destroy (c);
      }
    free (rx);
  }

  /* ---------------------------------------------------------------- *
   * 6. Reset reproducibility                                         *
   * ---------------------------------------------------------------- */
  {
    size_t         N  = 8000;
    float complex *rx = malloc (N * sizeof (*rx));
    uint32_t       ns = 3u;
    for (size_t k = 0; k < N; k++)
      rx[k] = (float complex)cexp (I * TWOPI * 0.0012 * (double)k)
              + 0.05f * gauss (&ns) + 0.05f * gauss (&ns) * I;
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    double               f1, lk1, f2, lk2;
    run (c, rx, N, &f1, &lk1);
    carrier_nda_reset (c);
    run (c, rx, N, &f2, &lk2);
    CHECK (f1 == f2 && lk1 == lk2);
    carrier_nda_destroy (c);
    free (rx);
  }

  if (_fails)
    {
      fprintf (stderr, "test_carrier_nda_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_carrier_nda_core PASSED\n");
  return 0;
}
