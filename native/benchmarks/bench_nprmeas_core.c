/* bench_nprmeas_core.c — noise-power-ratio, per capture.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * NPR is a whole-capture measurement: window, FFT, integrate the active
 * band, integrate the notch, take the ratio. The geometry arguments
 * (active band, notch, guard) select which bins go in each sum, so they
 * change the integration work but not the transform -- which means the
 * capture size, not the geometry, should set the cost. This measures both
 * so that claim is a result rather than an assumption.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "jm_bench.h"
#include "nprmeas/nprmeas_core.h"
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

  printf ("=== nprmeas benchmark ===\n");
  printf ("%d rounds\n\n", ITERATIONS);

  const size_t  caps[3] = { 4096, 16384, 65536 };
  static double t_an[3][ITERATIONS];

  for (int k = 0; k < 3; k++)
    {
      const size_t n = caps[k];
      float       *x = malloc (n * sizeof *x);
      if (!x)
        return 1;

      /* Band-limited noise with a notch: the signal NPR is defined on.
         A flat tone would put the notch integral on the noise floor and
         measure a degenerate ratio. */
      uint32_t lfsr = 0x3C5Au;
      for (size_t i = 0; i < n; i++)
        {
          lfsr     = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
          double u = (double)(lfsr & 0xFFFFu) / 65535.0 - 0.5;
          x[i]     = (float)(0.3 * u);
        }

      nprmeas_state_t *m = nprmeas_create (n, 1.0, 1.0, 0, 90.0);
      if (!m)
        {
          (void)fprintf (stderr, "bench_nprmeas: create(n=%zu) NULL\n", n);
          return 1;
        }

      /* Geometry in normalised frequency: an active band across most of
         the spectrum with a notch inside it, plus a guard. */
      const double lo = 0.05, hi = 0.45, nlo = 0.20, nhi = 0.24, guard = 0.005;

      npr_meas_t probe = nprmeas_analyze (m, x, n, lo, hi, nlo, nhi, guard);
      if (!isfinite (probe.npr_db))
        {
          (void)fprintf (stderr,
                         "bench_nprmeas: n=%zu gave npr_db %.3f — the "
                         "timings below would measure a failed analysis\n",
                         n, probe.npr_db);
          return 1;
        }

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += nprmeas_analyze (m, x, n, lo, hi, nlo, nhi, guard).npr_db;
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_an[k][r] = elapsed_sec (&t0, &t1);
        }
      char name[64];
      (void)snprintf (name, sizeof name, "analyze[n=%zu]", n);
      jm_bench_add (&_bench, name, t_an[k], ITERATIONS, 1);
      printf ("  %-22s %8.1f us/capture  %7.2f ns/sample\n", name,
              min_sec (t_an[k], ITERATIONS) * 1e6,
              min_sec (t_an[k], ITERATIONS) / (double)n * 1e9);

      nprmeas_destroy (m);
      free (x);
    }

  printf ("\n  16x the capture costs %.1fx the time. The geometry arguments\n"
          "  choose which bins are summed, not how many are transformed, so\n"
          "  the capture size is what a caller trades against sweep speed.\n",
          min_sec (t_an[2], ITERATIONS) / min_sec (t_an[0], ITERATIONS));

  (void)sink;
  jm_bench_write_json (&_bench, "nprmeas");
  return 0;
}
