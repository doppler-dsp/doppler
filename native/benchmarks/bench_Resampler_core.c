/* bench_Resampler_core.c — no step() to benchmark */
#include "doppler/Resampler/Resampler_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  dp_Resampler_state_t *obj = dp_Resampler_create (0.0);
  uint64_t              t0, t1;
  jm_bench_t            _bench = { 0 };

  printf ("=== Resampler benchmark ===\n");
  printf ("  (no step(); methods below)\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* bench: reset() */
  {
    double _times_reset[ITERATIONS];
    for (int i = 0; i < 16; i++)
      dp_Resampler_reset (obj);
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < BENCH_N; i++)
          dp_Resampler_reset (obj);
        t1              = jm_bench_now_ns ();
        _times_reset[r] = jm_bench_elapsed_sec (t0, t1);
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
  dp_Resampler_destroy (obj);
  return 0;
}
