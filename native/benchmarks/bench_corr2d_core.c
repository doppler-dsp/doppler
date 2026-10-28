/* bench_corr2d_core.c — single-row-reference fast path vs. the general
 * 2-D path, at a DSSS-acquisition-realistic grid.  A single-row reference
 * (the acq/detector2d shape: a code replica with no Doppler content of its
 * own) takes corr2d's fast path (ny independent length-nx 1-D FFTs); a
 * genuinely multi-row reference takes the general path (full (ny,nx) 2-D
 * FFT forward and inverse).  This is the durable regression gate for the
 * fast-path optimization (see docs/design/corr2d-interpolated-inverse.md).
 */
#include "doppler/corr2d/corr2d_core.h"
#include "doppler/dp_complex.h"
#include "jm_bench.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 50

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
  jm_bench_t _bench = { 0 };
  uint64_t   t0, t1;
  double     times[ITERATIONS];

  /* ny=16, nx=2046 mirrors docs/design/dsss-acquisition.md §7's worked
   * case (L=1023, spc=2 -> code_bins=2046; doppler_bins=16). */
  const size_t ny = 16, nx = 2046, n = ny * nx;
  uint32_t     seed = 42;

  printf ("=== corr2d benchmark ===\n");
  printf ("grid = %zu x %zu (%zu samples/frame), %d iterations\n\n", ny, nx, n,
          ITERATIONS);

  float _Complex *in  = malloc (n * sizeof *in);
  float _Complex *out = malloc (n * sizeof *out);
  for (size_t k = 0; k < n; k++)
    in[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

  /* ── single-row reference: the acq/detector2d shape -- fast path ──────── */
  {
    float _Complex *ref = calloc (n, sizeof *ref);
    for (size_t j = 0; j < nx; j++)
      ref[j] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

    dp_corr2d_state_t *obj = dp_corr2d_create (ref, ny, nx, 1, 1, 0, 0, -1);
    dp_corr2d_execute (obj, in, n, out, n); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        dp_corr2d_execute (obj, in, n, out, n);
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add_unit (&_bench, "single_row_fast_path", times, ITERATIONS, n,
                       JM_BENCH_SAMPLE);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  single-row (fast path)    %8.1f MSa/s  (fast_path=%d)\n",
            (double)n / (sum / ITERATIONS) / 1e6, obj->fast_path);

    free (ref);
    dp_corr2d_destroy (obj);
  }

  /* ── genuinely multi-row reference -- general 2-D path ────────────────── */
  {
    float _Complex *ref = malloc (n * sizeof *ref);
    for (size_t k = 0; k < n; k++)
      ref[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

    dp_corr2d_state_t *obj = dp_corr2d_create (ref, ny, nx, 1, 1, 0, 0, -1);
    dp_corr2d_execute (obj, in, n, out, n); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        dp_corr2d_execute (obj, in, n, out, n);
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add_unit (&_bench, "multi_row_general_path", times, ITERATIONS, n,
                       JM_BENCH_SAMPLE);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  multi-row (general path)  %8.1f MSa/s  (fast_path=%d)\n",
            (double)n / (sum / ITERATIONS) / 1e6, obj->fast_path);

    free (ref);
    dp_corr2d_destroy (obj);
  }

  free (in);
  free (out);

  /* ── delay-Doppler flavor (#2666): raw stream in, centered map out ──────
   * The row correlations of the fast path above, then the slow-time FFT
   * down every delay column, at 1 thread and every online core (0). */
  {
    const struct
    {
      size_t      ny, nx;
      int         nthreads;
      const char *name;
    } dd_cases[] = {
      { 32, 1024, 1, "delay_doppler_32x1024_1t" },
      { 256, 4096, 1, "delay_doppler_256x4096_1t" },
      { 256, 4096, 0, "delay_doppler_256x4096_all" },
    };
    for (size_t c = 0; c < sizeof dd_cases / sizeof *dd_cases; c++)
      {
        const size_t    dny = dd_cases[c].ny, dnx = dd_cases[c].nx;
        const size_t    dn   = dny * dnx;
        float _Complex *code = malloc (dnx * sizeof *code);
        float _Complex *x    = malloc (dn * sizeof *x);
        float _Complex *map  = malloc (dn * sizeof *map);
        for (size_t j = 0; j < dnx; j++)
          code[j] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;
        for (size_t k = 0; k < dn; k++)
          x[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;
        dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (
            code, dnx, dny, 1, dd_cases[c].nthreads, 0);
        dp_corr2d_execute (dd, x, dn, map, dn); /* warmup */
        double sum = 0.0;
        for (int r = 0; r < ITERATIONS; r++)
          {
            t0 = jm_bench_now_ns ();
            dp_corr2d_execute (dd, x, dn, map, dn);
            t1       = jm_bench_now_ns ();
            times[r] = jm_bench_elapsed_sec (t0, t1);
            sum += times[r];
          }
        jm_bench_add_unit (&_bench, dd_cases[c].name, times, ITERATIONS, dn,
                           JM_BENCH_SAMPLE);
        printf ("  %-28s %8.2f ms  (%d workers)\n", dd_cases[c].name,
                sum / ITERATIONS * 1e3, dd->nworkers);
        dp_corr2d_destroy (dd);
        free (code);
        free (x);
        free (map);
      }
  }

  jm_bench_write_json (&_bench, "corr2d");
  return 0;
}
