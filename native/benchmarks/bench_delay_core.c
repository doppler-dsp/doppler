/* bench_delay_core.c — no step() to benchmark */
#include "delay/delay_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  delay_state_t *obj = delay_create (1);
  uint64_t       t0, t1;
  jm_bench_t     _bench = { 0 };

  printf ("=== delay benchmark ===\n");
  printf ("  (no step(); methods below)\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* bench: push() */
  {
    double _times_push[ITERATIONS];
    for (int i = 0; i < 16; i++)
      delay_push (obj, 0.0 + 0.0 * I);
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < BENCH_N; i++)
          delay_push (obj, 0.0 + 0.0 * I);
        t1             = jm_bench_now_ns ();
        _times_push[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add (&_bench, "push", _times_push, ITERATIONS, BENCH_N);
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_push[r];
      printf ("  push()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }

  /* bench: write() */
  {
    double _times_write[ITERATIONS];
    for (int i = 0; i < 16; i++)
      delay_write (obj, 0.0 + 0.0 * I);
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < BENCH_N; i++)
          delay_write (obj, 0.0 + 0.0 * I);
        t1              = jm_bench_now_ns ();
        _times_write[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add (&_bench, "write", _times_write, ITERATIONS, BENCH_N);
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_write[r];
      printf ("  write()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }
  jm_bench_write_json (&_bench, "delay");
  delay_destroy (obj);
  return 0;
}
