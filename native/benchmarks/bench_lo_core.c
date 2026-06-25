/* bench_lo_core.c — the integer-NCO carrier oscillator.
 *
 * Benchmarks the three ways to generate a phasor stream:
 *   step   — inline single-sample lo_step()      (tracking-loop hot path)
 *   steps  — block lo_steps()                     (bulk synthesis, SIMD)
 *   cexpf  — double-phase accumulator + cexpf()   (the baseline lo replaces)
 *
 * The point of the integer NCO is that step is at least as fast as the
 * cexpf baseline (usually much faster — a LUT load vs a transcendental)
 * AND has bounded, predictable phase; this bench captures the throughput.
 */
#include "jm_bench.h"
#include "lo/lo_core.h"
#include <complex.h>
#include <math.h>
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

static void
report (const char *name, const double *times)
{
  double s = 0.0;
  for (int r = 0; r < ITERATIONS; r++)
    s += times[r];
  printf ("  %-8s %8.1f MSa/s\n", name,
          (double)BENCH_N / (s / ITERATIONS) / 1e6);
}

int
main (void)
{
  float complex *out = malloc (BENCH_N * sizeof (*out));
  if (!out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== lo benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* --- step: inline single-sample lo_step() (the loop hot path) --- */
  {
    lo_state_t     s;
    volatile float sink = 0.0f;
    lo_init (&s, 0.123);
    for (int i = 0; i < 16; i++)
      sink += crealf (lo_step (&s)); /* warmup */

    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          out[i] = lo_step (&s);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        sink += crealf (out[BENCH_N - 1]);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "step", times, ITERATIONS, BENCH_N);
    report ("step", times);
    (void)sink;
  }

  /* --- steps: block generator (SIMD bulk path) --- */
  {
    lo_state_t *lo = lo_create (0.123);
    lo_steps (lo, 16, out); /* warmup */

    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        lo_steps (lo, BENCH_N, out);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "steps", times, ITERATIONS, BENCH_N);
    report ("steps", times);
    lo_destroy (lo);
  }

  /* --- cexpf baseline: double-phase accumulator + cexpf() --- */
  {
    volatile float sink  = 0.0f;
    double         phase = 0.0, w = 0.123 * 2.0 * M_PI;
    for (int i = 0; i < 16; i++)
      {
        sink += crealf (cexpf ((float)phase * I));
        phase += w;
      }
    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          {
            out[i] = cexpf ((float)phase * I);
            phase += w;
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        sink += crealf (out[BENCH_N - 1]);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "cexpf_baseline", times, ITERATIONS, BENCH_N);
    report ("cexpf", times);
    (void)sink;
  }

  jm_bench_write_json (&_bench, "lo");
  free (out);
  return 0;
}
