#include "agc/agc_core.h"
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
  float _Complex *in = malloc (BENCH_N * sizeof (float _Complex));
  if (!in)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  float _Complex *out = malloc (BENCH_N * sizeof (float _Complex));
  if (!out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  /* Constant-envelope input: a ramp would push the loop through
     near-zero power and denormal arithmetic, skewing the timing. */
  for (int i = 0; i < BENCH_N; i++)
    in[i] = 0.6f + 0.8f * I;

  agc_state_t *obj = agc_create (0.0, 0.0025, 0.05);

  /* volatile sink prevents DCE of the step() loop */
  volatile float _Complex _sink;

  /* warmup */
  for (int i = 0; i < 16; i++)
    _sink = agc_step (obj, in[i]);

  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  printf ("=== agc benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      for (int i = 0; i < BENCH_N; i++)
        _sink = agc_step (obj, in[i]);
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
      agc_steps (obj, in, out, BENCH_N);
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

  jm_bench_write_json (&_bench, "agc");
  agc_destroy (obj);
  free (in);
  free (out);
  return 0;
}
