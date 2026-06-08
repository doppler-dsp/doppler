#include "acc_cf64/acc_cf64_core.h"
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
  double complex *in = malloc (BENCH_N * sizeof (double complex));
  if (!in)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  for (int i = 0; i < BENCH_N; i++)
    in[i] = (double)(i) + 0.0 * I;

  acc_cf64_state_t *obj = acc_cf64_create (0.0 + 0.0 * I);

  /* warmup */
  for (int i = 0; i < 16; i++)
    acc_cf64_step (obj, in[i]);

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== acc_cf64 benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  double _times_step[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        acc_cf64_step (obj, in[i]);
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
      acc_cf64_steps (obj, in, BENCH_N);
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

  /* bench: get() */
  {
    double                  _times_get[ITERATIONS];
    volatile double complex get_sink;
    for (int i = 0; i < 16; i++)
      get_sink = acc_cf64_get (obj);
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          get_sink = acc_cf64_get (obj);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        _times_get[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "get", _times_get, ITERATIONS, BENCH_N);
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_get[r];
      printf ("  get()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }

  /* bench: dump() */
  {
    double                  _times_dump[ITERATIONS];
    volatile double complex dump_sink;
    for (int i = 0; i < 16; i++)
      dump_sink = acc_cf64_dump (obj);
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          dump_sink = acc_cf64_dump (obj);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        _times_dump[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "dump", _times_dump, ITERATIONS, BENCH_N);
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_dump[r];
      printf ("  dump()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }

  /* bench: madd() */
  {
    double _times_madd[ITERATIONS];
    for (int i = 0; i < 16; i++)
      acc_cf64_madd (obj, NULL, 0, NULL, 0);
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          acc_cf64_madd (obj, NULL, 0, NULL, 0);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        _times_madd[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "madd", _times_madd, ITERATIONS, BENCH_N);
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_madd[r];
      printf ("  madd()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }

  /* bench: add2d() */
  {
    double _times_add2d[ITERATIONS];
    for (int i = 0; i < 16; i++)
      acc_cf64_add2d (obj, NULL, 0);
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          acc_cf64_add2d (obj, NULL, 0);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        _times_add2d[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "add2d", _times_add2d, ITERATIONS, BENCH_N);
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_add2d[r];
      printf ("  add2d()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }

  /* bench: madd2d() */
  {
    double _times_madd2d[ITERATIONS];
    for (int i = 0; i < 16; i++)
      acc_cf64_madd2d (obj, NULL, 0, NULL, 0);
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          acc_cf64_madd2d (obj, NULL, 0, NULL, 0);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        _times_madd2d[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "madd2d", _times_madd2d, ITERATIONS, BENCH_N);
    {
      double _s = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        _s += _times_madd2d[r];
      printf ("  madd2d()  %8.1f MSa/s\n",
              (double)BENCH_N / (_s / ITERATIONS) / 1e6);
    }
  }
  jm_bench_write_json (&_bench, "acc_cf64");
  acc_cf64_destroy (obj);
  free (in);

  return 0;
}
