#include "doppler/dp_complex.h"
#include "doppler/i16u32_to_f32/i16u32_to_f32_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  uint32_t *in = malloc (BENCH_N * sizeof (uint32_t));
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
    in[i] = (uint32_t)(i);

  dp_i16u32_to_f32_state_t *obj = dp_i16u32_to_f32_create (32768.0f);

  /* volatile sink prevents DCE of the step() loop */
  volatile float _sink;

  /* warmup */
  for (int i = 0; i < 16; i++)
    _sink = dp_i16u32_to_f32_step (obj, in[i]);

  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  printf ("=== i16u32_to_f32 benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      for (int i = 0; i < BENCH_N; i++)
        _sink = dp_i16u32_to_f32_step (obj, in[i]);
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
      dp_i16u32_to_f32_steps (obj, in, out, BENCH_N);
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

  jm_bench_write_json (&_bench, "i16u32_to_f32");
  dp_i16u32_to_f32_destroy (obj);
  free (in);
  free (out);
  return 0;
}
