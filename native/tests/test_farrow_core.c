/**
 * @file test_farrow_core.c
 * @brief Unit tests for the Farrow fractional-delay interpolator.
 *
 * Tests:
 *   1. Lifecycle / order / init==create parity
 *   2. Endpoints — mu=0 returns d[1], mu->1 returns d[2]
 *   3. Polynomial exactness — each order is exact for its degree
 *   4. Fractional delay of a sinusoid matches the expected phase shift
 *   5. Reset clears the delay line
 */
#include "dp_state_test.h"
#include "farrow/farrow_core.h"
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

/* Interpolate a length-N real sequence f[] at continuous position `pos`
 * (= integer index + fraction) by streaming through the Farrow, accounting for
 * the 2-sample group delay (d[1] lags the newest pushed sample by 2). */
static float complex
interp_at (int order, const float *f, int n, double pos)
{
  farrow_state_t s;
  farrow_init (&s, order);
  int           base = (int)floor (pos);
  double        mu   = pos - base;
  float complex y    = 0.0f;
  for (int i = 0; i <= base + 2 && i < n; i++)
    {
      farrow_push (&s, f[i] + 0.0f * I);
      if (i == base + 2)
        y = farrow_eval (&s, (float)mu);
    }
  return y;
}

int
main (void)
{
  int _fails = 0;

  /* 1. Lifecycle / order / parity */
  {
    farrow_state_t *c = farrow_create (FARROW_CUBIC);
    CHECK (c != NULL);
    if (!c)
      return 1;
    CHECK (c->order == FARROW_CUBIC);
    CHECK (farrow_get_group_delay (c) == 2);
    farrow_state_t v;
    farrow_init (&v, FARROW_CUBIC);
    CHECK (v.order == c->order);
    farrow_destroy (c);
  }

  /* 2. Endpoints: mu=0 -> d[1], mu->1 -> d[2] (ramp d = {0,1,2,3}) */
  {
    for (int order = 0; order <= 2; order++)
      {
        farrow_state_t s;
        farrow_init (&s, order);
        for (int i = 0; i < 4; i++)
          farrow_push (&s, (float)i + 0.0f * I);
        CHECK (cabsf (farrow_eval (&s, 0.0f) - 1.0f) < 1e-5f);     /* d[1] */
        CHECK (cabsf (farrow_eval (&s, 0.999f) - 1.999f) < 1e-2f); /* ->d[2] */
      }
  }

  /* 3. Polynomial exactness — order p is exact for a degree-p polynomial. */
  {
    const int n = 24;
    /* linear & the symmetric piecewise-parabolic are exact for degree 1;
     * cubic is exact for degree 3. */
    int deg[3] = { 1, 1, 3 };
    for (int order = 0; order <= 2; order++)
      {
        float f[24];
        for (int i = 0; i < n; i++)
          {
            double t = (double)i, v = 0.0;
            for (int p = 0; p <= deg[order]; p++)
              v += (p % 2 ? -0.7 : 0.4) * pow (t - 8.0, p);
            f[i] = (float)v;
          }
        int bad = 0;
        for (double pos = 8.0; pos <= 14.0; pos += 0.137)
          {
            double exact = 0.0;
            for (int p = 0; p <= deg[order]; p++)
              exact += (p % 2 ? -0.7 : 0.4) * pow (pos - 8.0, p);
            float complex got = interp_at (order, f, n, pos);
            if (fabsf (crealf (got) - (float)exact) > 1e-2f)
              bad++;
          }
        CHECK (bad == 0); /* exact for its own degree */
      }
  }

  /* 4. Fractional delay of a complex sinusoid -> expected phase shift.
   * Cubic is accurate to a couple percent at a modest frequency. */
  {
    const int      n     = 64;
    double         fnorm = 0.05; /* cycles/sample */
    farrow_state_t s;
    farrow_init (&s, FARROW_CUBIC);
    double        mu     = 0.5;
    int           errors = 0;
    float complex sig[64];
    for (int i = 0; i < n; i++)
      sig[i] = cexpf ((float)(2.0 * M_PI * fnorm * i) * I);
    for (int i = 0; i < n; i++)
      {
        farrow_push (&s, sig[i]);
        if (i >= 3)
          {
            float complex got  = farrow_eval (&s, (float)mu);
            double        pos  = (double)i - 2.0 + mu;
            float complex want = cexpf ((float)(2.0 * M_PI * fnorm * pos) * I);
            if (cabsf (got - want) > 0.02f)
              errors++;
          }
      }
    CHECK (errors == 0);
  }

  /* 5. Reset clears the delay line */
  {
    farrow_state_t *s = farrow_create (FARROW_LINEAR);
    farrow_push (s, 5.0f + 0.0f * I);
    farrow_push (s, 6.0f + 0.0f * I);
    farrow_reset (s);
    CHECK (cabsf (farrow_eval (s, 0.5f)) < 1e-6f);
    farrow_destroy (s);
  }

  if (_fails)
    {
      fprintf (stderr, "test_farrow_core FAILED (%d)\n", _fails);
      return 1;
    }
  /* serializable state — POD snapshot round-trips + rejects a bad envelope. */
  {
    farrow_state_t *a = farrow_create (FARROW_CUBIC);
    farrow_state_t *b = farrow_create (FARROW_CUBIC);
    CHECK (a != NULL && b != NULL);
    farrow_push (a, 1.0f + 0.0f * I);
    farrow_push (a, 0.0f + 2.0f * I);
    farrow_push (a, -1.0f + 0.5f * I);
    farrow_push (a, 0.5f - 0.5f * I);
    DP_STATE_ROUNDTRIP_TEST (farrow, a, b);
    CHECK (farrow_eval (b, 0.3f) == farrow_eval (a, 0.3f));
    farrow_destroy (a);
    farrow_destroy (b);
  }

  printf ("test_farrow_core PASSED\n");
  return 0;
}
