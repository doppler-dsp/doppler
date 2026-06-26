/* bench_costas_core.c — the Costas carrier-tracking loop.
 *
 * Two numbers:
 *   steps   — end-to-end throughput (MSa/s) of the per-sample wipe-off +
 *             per-symbol integrate-and-dump + loop update over a 64k burst.
 *   acq     — acquisition time: samples until the lock metric first crosses
 *             0.9 from a cold start on a fixed carrier residual.
 */
#include "costas/costas_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200
#define TSAMPS 16

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  /* Build a 64k BPSK-at-symbol-rate burst with a carrier residual. */
  float complex *rx  = malloc (BENCH_N * sizeof (*rx));
  float complex *out = malloc (BENCH_N * sizeof (*out));
  if (!rx || !out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  uint32_t bst   = 1u;
  double   phase = 0.0, w = 0.002 * 2.0 * M_PI;
  for (int s = 0; s < BENCH_N / TSAMPS; s++)
    {
      bst ^= bst << 13;
      bst ^= bst >> 17;
      bst ^= bst << 5;
      float b = (bst & 1u) ? -1.0f : 1.0f;
      for (int i = 0; i < TSAMPS; i++)
        {
          int k = s * TSAMPS + i;
          rx[k] = b * cexpf ((float)phase * I);
          phase += w;
        }
    }

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== costas benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* --- steps throughput --- */
  {
    costas_state_t *c = costas_create (0.05, 0.707, 0.0, TSAMPS);
    costas_steps (c, rx, TSAMPS * 4, out, BENCH_N); /* warmup */

    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        costas_reset (c);
        clock_gettime (CLOCK_MONOTONIC, &t0);
        costas_steps (c, rx, BENCH_N, out, BENCH_N);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "steps", times, ITERATIONS, BENCH_N);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  steps    %8.1f MSa/s\n",
            (double)BENCH_N / (sum / ITERATIONS) / 1e6);
    costas_destroy (c);
  }

  /* --- acquisition time: samples to lock_metric > 0.9 --- */
  {
    costas_state_t *c       = costas_create (0.05, 0.707, 0.0, TSAMPS);
    long            acq_smp = -1;
    for (int s = 0; s < BENCH_N / TSAMPS; s++)
      {
        costas_steps (c, rx + s * TSAMPS, TSAMPS, out, 1);
        if (costas_get_lock_metric (c) > 0.9)
          {
            acq_smp = (long)(s + 1) * TSAMPS;
            break;
          }
      }
    printf ("  acq      %ld samples to lock (%.1f symbols)\n", acq_smp,
            (double)acq_smp / TSAMPS);
    costas_destroy (c);
  }

  jm_bench_write_json (&_bench, "costas");
  free (rx);
  free (out);
  return 0;
}
