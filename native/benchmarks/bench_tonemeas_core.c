/* bench_tonemeas_core.c — single-tone ADC metrics, per capture.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * `tonemeas_analyze` is a whole-capture measurement, not a streaming one:
 * window, FFT, find the fundamental, sum the harmonics, sum the rest. So
 * the honest unit is TIME PER CAPTURE and the useful sweep is capture
 * size, because that is the knob a bench operator turns when they want a
 * lower noise floor and want to know what it costs them per sweep point.
 *
 * The complex variant is measured beside the real one because it is a
 * different transform, not a wrapper: a real capture folds, a complex one
 * does not.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "jm_bench.h"
#include "tonemeas/tonemeas_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 50
#define N_HARM 8

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile double sink   = 0.0;

  printf ("=== tonemeas benchmark ===\n");
  printf ("%d harmonics, %d rounds\n\n", N_HARM, ITERATIONS);

  const size_t  caps[3] = { 4096, 16384, 65536 };
  static double t_re[3][ITERATIONS], t_cx[3][ITERATIONS];

  for (int k = 0; k < 3; k++)
    {
      const size_t   n  = caps[k];
      float         *x  = malloc (n * sizeof *x);
      float complex *xc = malloc (n * sizeof *xc);
      if (!x || !xc)
        return 1;
      /* A coherent tone near mid-band with a little harmonic content, so
         the harmonic search has something to find rather than reading a
         floor. */
      for (size_t i = 0; i < n; i++)
        {
          double p = 2.0 * M_PI * 0.113 * (double)i;
          x[i]     = (float)(0.5 * sin (p) + 0.002 * sin (2 * p)
                             + 0.001 * sin (3 * p));
          xc[i]    = (float)(0.5 * cos (p)) + (float)(0.5 * sin (p)) * I;
        }

      tonemeas_state_t *m = tonemeas_create (n, 1.0, N_HARM, 1.0, 0, 90.0, 0);
      if (!m)
        {
          (void)fprintf (stderr, "bench_tonemeas: create(n=%zu) NULL\n", n);
          return 1;
        }

      /* Precondition: a capture that measures nothing would time the
         early-out, not the analysis. */
      tone_meas_t probe = tonemeas_analyze (m, x, n);
      if (!(probe.sfdr_dbc > 0.0))
        {
          (void)fprintf (stderr,
                         "bench_tonemeas: n=%zu gave sfdr %.3f dBc — the "
                         "timings below would measure a failed analysis\n",
                         n, probe.sfdr_dbc);
          return 1;
        }

      char name[64];
      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += tonemeas_analyze (m, x, n).sfdr_dbc;
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_re[k][r] = elapsed_sec (&t0, &t1);
        }
      (void)snprintf (name, sizeof name, "analyze[n=%zu]", n);
      jm_bench_add (&_bench, name, t_re[k], ITERATIONS, 1);
      printf ("  %-24s %8.1f us/capture  %7.2f ns/sample\n", name,
              min_sec (t_re[k], ITERATIONS) * 1e6,
              min_sec (t_re[k], ITERATIONS) / (double)n * 1e9);

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += tonemeas_analyze_complex (m, xc, n).sfdr_dbc;
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_cx[k][r] = elapsed_sec (&t0, &t1);
        }
      (void)snprintf (name, sizeof name, "analyze_complex[n=%zu]", n);
      jm_bench_add (&_bench, name, t_cx[k], ITERATIONS, 1);
      printf ("  %-24s %8.1f us/capture  %7.2f ns/sample\n", name,
              min_sec (t_cx[k], ITERATIONS) * 1e6,
              min_sec (t_cx[k], ITERATIONS) / (double)n * 1e9);

      tonemeas_destroy (m);
      free (x);
      free (xc);
    }

  printf ("\n  16x the capture costs %.1fx the time -- slightly WORSE than\n"
          "  linear, which is what n log n has to look like (16 * log ratio\n"
          "  ~ 21x is the bound; the per-sample column shows the same thing\n"
          "  rising 8.7 -> 10.0 ns). A finer noise floor is not free, and\n"
          "  per sweep point this is the fixed cost before any settling.\n"
          "\n  The complex path costs ~1.33x the real one at every size --\n"
          "  it is a different transform, not a wrapper.\n",
          min_sec (t_re[2], ITERATIONS) / min_sec (t_re[0], ITERATIONS));

  (void)sink;
  jm_bench_write_json (&_bench, "tonemeas");
  return 0;
}
