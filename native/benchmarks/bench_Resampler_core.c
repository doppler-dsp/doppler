/* bench_Resampler_core.c — no step() to benchmark */
#include "Resampler/Resampler_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  Resampler_state_t *obj = Resampler_create (0.0);
  struct timespec    t0, t1;
  jm_bench_t         _bench = { 0 };

  printf ("=== Resampler benchmark ===\n");
  printf ("  (no step(); methods below)\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* bench: reset() */
  {
    double _times_reset[ITERATIONS];
    for (int i = 0; i < 16; i++)
      Resampler_reset (obj);
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          Resampler_reset (obj);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        _times_reset[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "reset", _times_reset, ITERATIONS, BENCH_N);
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_reset[r];
      printf ("  reset()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }
  jm_bench_write_json (&_bench, "Resampler");
  Resampler_destroy (obj);
  return 0;
}
