/* bench_symsync_core.c — the Gardner symbol-timing synchronizer.
 *
 *   steps — end-to-end throughput (MSa/s) of the per-sample integer-NCO
 *           strobe + Farrow interpolate + per-symbol Gardner/PI update over a
 *           64k oversampled block.
 */
#include "jm_bench.h"
#include "symsync/symsync_core.h"
#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200
#define SPS 4

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  float complex *x   = malloc (BENCH_N * sizeof (*x));
  float complex *out = malloc (BENCH_N * sizeof (*out));
  if (!x || !out)
    return 1;
  /* simple oversampled BPSK-ish stream (content does not affect timing cost)
   */
  uint32_t st = 1u;
  for (int k = 0; k < BENCH_N; k++)
    {
      if ((k % SPS) == 0)
        {
          st ^= st << 13;
          st ^= st >> 17;
          st ^= st << 5;
        }
      x[k] = (st & 1u) ? -1.0f : 1.0f;
    }

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== symsync benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  symsync_state_t *s = symsync_create (SPS, 0.01, 0.707, FARROW_CUBIC);
  symsync_steps (s, x, SPS * 64, out, BENCH_N); /* warmup */

  double times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      symsync_reset (s);
      clock_gettime (CLOCK_MONOTONIC, &t0);
      symsync_steps (s, x, BENCH_N, out, BENCH_N);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "steps", times, ITERATIONS, BENCH_N);
  double sum = 0.0;
  for (int r = 0; r < ITERATIONS; r++)
    sum += times[r];
  printf ("  steps    %8.1f MSa/s\n",
          (double)BENCH_N / (sum / ITERATIONS) / 1e6);

  jm_bench_write_json (&_bench, "symsync");
  symsync_destroy (s);
  free (x);
  free (out);
  return 0;
}
