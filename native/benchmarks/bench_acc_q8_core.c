#include "acc_q8/acc_q8_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  int8_t *in = malloc (BENCH_N * sizeof (int8_t));
  if (!in)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  for (int i = 0; i < BENCH_N; i++)
    in[i] = (int8_t)(i);

  acc_q8_state_t *obj = acc_q8_create (0);

  /* warmup */
  for (int i = 0; i < 16; i++)
    acc_q8_step (obj, in[i]);

  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  printf ("=== acc_q8 benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      for (int i = 0; i < BENCH_N; i++)
        acc_q8_step (obj, in[i]);
      t1             = jm_bench_now_ns ();
      _times_step[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "step", _times_step, ITERATIONS, BENCH_N);
  {
    double _s = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      _s += _times_step[r];
    printf ("  step()   %8.1f MSa/s\n",
            (double)BENCH_N / (_s / ITERATIONS) / 1e6);
  }
  double _times_steps[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      acc_q8_steps (obj, in, BENCH_N);
      t1              = jm_bench_now_ns ();
      _times_steps[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "steps", _times_steps, ITERATIONS, BENCH_N);
  {
    double _s = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      _s += _times_steps[r];
    printf ("  steps()  %8.1f MSa/s\n",
            (double)BENCH_N / (_s / ITERATIONS) / 1e6);
  }

  jm_bench_write_json (&_bench, "acc_q8");
  acc_q8_destroy (obj);
  free (in);

  return 0;
}
