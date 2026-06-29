/* bench_ppe_core.c — time the polynomial-phase estimate in both regimes:
 *   "doppler" — max_rate = 0, a single FFT (near-static Doppler);
 *   "chirp"   — max_rate > 0, the coherent (rate x freq) dechirp-bank surface.
 * Reports samples/s (ops = N / mean-per-estimate). */
#include "jm_bench.h"
#include "ppe/ppe_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 200

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static void
synth (float complex *y, size_t L, double f, double r)
{
  for (size_t m = 0; m < L; m++)
    {
      double ph
          = 2.0 * M_PI * (f * (double)m + 0.5 * r * (double)m * (double)m);
      y[m] = (float)cos (ph) + (float)sin (ph) * I;
    }
}

static void
bench_one (jm_bench_t *b, const char *name, size_t L, double max_rate)
{
  ppe_state_t   *p = ppe_create (L, max_rate);
  float complex *y = malloc (L * sizeof *y);
  if (!p || !y)
    return;
  synth (y, L, 0.05, max_rate > 0.0 ? 1e-5 : 0.0);
  ppe_result_t e = ppe_estimate (p, y, L); /* warm */

  struct timespec t0, t1;
  double          times[ITERATIONS];
  for (int i = 0; i < ITERATIONS; i++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      e = ppe_estimate (p, y, L);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[i] = elapsed_sec (&t0, &t1);
    }
  printf ("  %-8s L=%5zu n_rate=%5zu  f=%.4f r=%.2e snr=%.0f\n", name, L,
          p->n_rate, e.freq_norm, e.rate_norm, e.snr_db);
  jm_bench_add (b, name, times, ITERATIONS, (int)L);
  free (y);
  ppe_destroy (p);
}

int
main (void)
{
  jm_bench_t _bench = { 0 };
  printf ("=== ppe benchmark ===\n");
  bench_one (&_bench, "doppler", 4096, 0.0); /* single FFT */
  bench_one (&_bench, "chirp", 512, 5e-5);   /* dechirp-bank surface */
  jm_bench_write_json (&_bench, "ppe");
  return 0;
}
