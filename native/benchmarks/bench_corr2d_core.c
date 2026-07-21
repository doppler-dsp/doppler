/* bench_corr2d_core.c — single-row-reference fast path vs. the general
 * 2-D path, at a DSSS-acquisition-realistic grid.  A single-row reference
 * (the acq/detector2d shape: a code replica with no Doppler content of its
 * own) takes corr2d's fast path (ny independent length-nx 1-D FFTs); a
 * genuinely multi-row reference takes the general path (full (ny,nx) 2-D
 * FFT forward and inverse).  This is the durable regression gate for the
 * fast-path optimization (see docs/design/corr2d-interpolated-inverse.md).
 */
#include "corr2d/corr2d_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 50

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static uint32_t
_xorshift32 (uint32_t *s)
{
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

static float
_rand_uniform (uint32_t *s)
{
  return ((float)(_xorshift32 (s) % 20001u) - 10000.0f) / 10000.0f;
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  double          times[ITERATIONS];

  /* ny=16, nx=2046 mirrors docs/design/dsss-acquisition.md §7's worked
   * case (L=1023, spc=2 -> code_bins=2046; doppler_bins=16). */
  const size_t ny = 16, nx = 2046, n = ny * nx;
  uint32_t     seed = 42;

  printf ("=== corr2d benchmark ===\n");
  printf ("grid = %zu x %zu (%zu samples/frame), %d iterations\n\n", ny, nx, n,
          ITERATIONS);

  float complex *in  = malloc (n * sizeof *in);
  float complex *out = malloc (n * sizeof *out);
  for (size_t k = 0; k < n; k++)
    in[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

  /* ── single-row reference: the acq/detector2d shape -- fast path ──────── */
  {
    float complex *ref = calloc (n, sizeof *ref);
    for (size_t j = 0; j < nx; j++)
      ref[j] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

    corr2d_state_t *obj = corr2d_create (ref, ny, nx, 1, 1, 0, 0);
    corr2d_execute (obj, in, n, out); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        corr2d_execute (obj, in, n, out);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "single_row_fast_path", times, ITERATIONS, n);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  single-row (fast path)    %8.1f MSa/s  (fast_path=%d)\n",
            (double)n / (sum / ITERATIONS) / 1e6, obj->fast_path);

    free (ref);
    corr2d_destroy (obj);
  }

  /* ── genuinely multi-row reference -- general 2-D path ────────────────── */
  {
    float complex *ref = malloc (n * sizeof *ref);
    for (size_t k = 0; k < n; k++)
      ref[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

    corr2d_state_t *obj = corr2d_create (ref, ny, nx, 1, 1, 0, 0);
    corr2d_execute (obj, in, n, out); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        corr2d_execute (obj, in, n, out);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "multi_row_general_path", times, ITERATIONS, n);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  multi-row (general path)  %8.1f MSa/s  (fast_path=%d)\n",
            (double)n / (sum / ITERATIONS) / 1e6, obj->fast_path);

    free (ref);
    corr2d_destroy (obj);
  }

  free (in);
  free (out);
  jm_bench_write_json (&_bench, "corr2d");
  return 0;
}
