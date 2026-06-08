#include "i16u64_to_f32/i16u64_to_f32_core.h"
#include "jm_bench.h"
#include <complex.h>
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
  uint64_t *in = malloc (BENCH_N * sizeof (uint64_t));
  if (!in)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  float *out = malloc (BENCH_N * sizeof (float));
  if (!out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  for (int i = 0; i < BENCH_N; i++)
    in[i] = (uint64_t)(i);

  i16u64_to_f32_state_t *obj = i16u64_to_f32_create (32768.0f);

  /* volatile sink prevents DCE of the step() loop */
  volatile float _sink;

  /* warmup */
  for (int i = 0; i < 16; i++)
    _sink = i16u64_to_f32_step (obj, in[i]);

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== i16u64_to_f32 benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        _sink = i16u64_to_f32_step (obj, in[i]);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      _times_step[r] = elapsed_sec (&t0, &t1);
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
      clock_gettime (CLOCK_MONOTONIC, &t0);
      i16u64_to_f32_steps (obj, in, out, BENCH_N);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      _times_steps[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "steps", _times_steps, ITERATIONS, BENCH_N);
  {
    double _s = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      _s += _times_steps[r];
    printf ("  steps()  %8.1f MSa/s\n",
            (double)BENCH_N / (_s / ITERATIONS) / 1e6);
  }

  jm_bench_write_json (&_bench, "i16u64_to_f32");
  i16u64_to_f32_destroy (obj);
  free (in);
  free (out);
  return 0;
}
