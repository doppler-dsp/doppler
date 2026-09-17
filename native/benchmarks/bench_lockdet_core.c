#include "jm_bench.h"
#include "lockdet/lockdet_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  double *in = malloc (BENCH_N * sizeof (double));
  if (!in)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  int *out = malloc (BENCH_N * sizeof (int));
  if (!out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  /* Alternate runs above/below the band so both state-machine sides and
   * the reset paths all execute (a constant input would settle into one
   * branch and benchmark only the predictor). */
  for (int i = 0; i < BENCH_N; i++)
    in[i] = ((i >> 6) & 1) ? 2.0 : 1.0;

  lockdet_state_t *obj = lockdet_create (1.5, 1.2, 3, 3);

  /* volatile sink prevents DCE of the step() loop */
  volatile int _sink;

  /* warmup */
  for (int i = 0; i < 16; i++)
    _sink = lockdet_step (obj, in[i]);

  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  printf ("=== lockdet benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      for (int i = 0; i < BENCH_N; i++)
        _sink = lockdet_step (obj, in[i]);
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
      lockdet_steps (obj, in, out, BENCH_N);
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

  jm_bench_write_json (&_bench, "lockdet");
  lockdet_destroy (obj);
  free (in);
  free (out);
  return 0;
}
