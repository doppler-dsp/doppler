/* bench_imdmeas_core.c — two-tone intermodulation, per capture.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * IMD is a whole-capture measurement over a two-tone stimulus: window,
 * FFT, locate both fundamentals, then search the 3rd- and 5th-order
 * product frequencies. The product search is bounded and small next to the
 * transform, so capture size should dominate -- which is what the sweep is
 * for. Reported per CAPTURE, because that is the unit a bench sweep pays.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "imdmeas/imdmeas_core.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 50

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

  printf ("=== imdmeas benchmark ===\n");
  printf ("two-tone stimulus, %d rounds\n\n", ITERATIONS);

  const size_t  caps[3] = { 4096, 16384, 65536 };
  static double t_an[3][ITERATIONS];

  for (int k = 0; k < 3; k++)
    {
      const size_t n = caps[k];
      float       *x = malloc (n * sizeof *x);
      if (!x)
        return 1;

      /* Two coherent tones plus deliberate 3rd-order products, so the
         product search finds real spurs instead of the noise floor --
         which would time the same FFT but a degenerate search. */
      for (size_t i = 0; i < n; i++)
        {
          double p1 = 2.0 * M_PI * 0.101 * (double)i;
          double p2 = 2.0 * M_PI * 0.123 * (double)i;
          double p3 = 2.0 * M_PI * 0.079 * (double)i; /* 2*f1 - f2 */
          double p4 = 2.0 * M_PI * 0.145 * (double)i; /* 2*f2 - f1 */
          x[i] = (float)(0.4 * sin (p1) + 0.4 * sin (p2) + 0.002 * sin (p3)
                         + 0.002 * sin (p4));
        }

      imdmeas_state_t *m = imdmeas_create (n, 1.0, 1.0, 0, 90.0);
      if (!m)
        {
          (void)fprintf (stderr, "bench_imdmeas: create(n=%zu) NULL\n", n);
          return 1;
        }

      imd_meas_t probe = imdmeas_analyze (m, x, n);
      if (!isfinite (probe.imd3_dbc))
        {
          (void)fprintf (stderr,
                         "bench_imdmeas: n=%zu gave imd3 %.3f dBc — the "
                         "timings below would measure a failed analysis\n",
                         n, probe.imd3_dbc);
          return 1;
        }

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += imdmeas_analyze (m, x, n).imd3_dbc;
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_an[k][r] = elapsed_sec (&t0, &t1);
        }
      char name[64];
      (void)snprintf (name, sizeof name, "analyze[n=%zu]", n);
      jm_bench_add (&_bench, name, t_an[k], ITERATIONS, 1);
      printf ("  %-22s %8.1f us/capture  %7.2f ns/sample\n", name,
              min_sec (t_an[k], ITERATIONS) * 1e6,
              min_sec (t_an[k], ITERATIONS) / (double)n * 1e9);

      imdmeas_destroy (m);
      free (x);
    }

  printf ("\n  16x the capture costs %.1fx the time. The product search is\n"
          "  a bounded lookup at known frequencies, so what a two-tone\n"
          "  sweep pays per point is the transform, not the search.\n",
          min_sec (t_an[2], ITERATIONS) / min_sec (t_an[0], ITERATIONS));

  (void)sink;
  jm_bench_write_json (&_bench, "imdmeas");
  return 0;
}
