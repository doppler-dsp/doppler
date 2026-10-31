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
#include <math.h>
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

/* Batches (#2683): H maps in one call against H execute() calls.  The
 * request's geometry: a 64 x 512 delay-Doppler map, H = 48 frames.  The
 * per-map work is the same either way, so what a batch can save is the
 * per-call overhead and, on a pool, the dispatches: whole maps per worker
 * against each map split across every worker.  Setup (create, stimulus,
 * buffers) is outside the timed region. */
static void
bench_batches (jm_bench_t *bench, uint32_t *seed)
{
  enum
  {
    BREPS = 10
  };
  const size_t    bny = 64, bnx = 512, H = 48, bn = bny * bnx;
  float _Complex *code = malloc (bnx * sizeof *code);
  float _Complex *x    = malloc (H * bn * sizeof *x);
  float _Complex *maps = malloc (H * bn * sizeof *maps);
  float          *pw   = malloc (bn * sizeof *pw);
  uint32_t       *idx  = malloc (bn * sizeof *idx);
  for (size_t j = 0; j < bnx; j++)
    code[j] = _rand_uniform (seed) + _rand_uniform (seed) * I;
  for (size_t k = 0; k < H * bn; k++)
    x[k] = _rand_uniform (seed) + _rand_uniform (seed) * I;
  const struct
  {
    int         nthreads;
    const char *name[3];
  } bc[] = {
    { 1,
      { "batch_64x512_h48_execute_loop_1t", "batch_64x512_h48_all_1t",
        "batch_64x512_h48_power_max_1t" } },
    { 0,
      { "batch_64x512_h48_execute_loop_all", "batch_64x512_h48_all_all",
        "batch_64x512_h48_power_max_all" } },
  };
  double bt[BREPS];
  for (size_t c = 0; c < sizeof bc / sizeof *bc; c++)
    {
      dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (
          code, bnx, bny, 1, bc[c].nthreads, 0);
      /* warmup: one of each, which also makes the batch scratch */
      dp_corr2d_execute (dd, x, bn, maps, bn);
      dp_corr2d_execute_all (dd, x, H * bn, maps, H * bn);
      for (int which = 0; which < 3; which++)
        {
          double best = 1e30;
          for (int r = 0; r < BREPS; r++)
            {
              const uint64_t t0 = jm_bench_now_ns ();
              if (which == 0)
                for (size_t h = 0; h < H; h++)
                  dp_corr2d_execute (dd, x + h * bn, bn, maps + h * bn, bn);
              else if (which == 1)
                dp_corr2d_execute_all (dd, x, H * bn, maps, H * bn);
              else
                dp_corr2d_execute_power_max (dd, x, H * bn, pw, idx, bn);
              bt[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
              best  = bt[r] < best ? bt[r] : best;
            }
          jm_bench_add_unit (bench, bc[c].name[which], bt, BREPS, H * bn,
                             JM_BENCH_SAMPLE);
          printf ("  %-36s %8.2f ms  (%d workers)\n", bc[c].name[which],
                  best * 1e3, dd->nworkers);
        }
      dp_corr2d_destroy (dd);
    }
  free (code);
  free (x);
  free (maps);
  free (pw);
  free (idx);
}

/* The hypothesis bank (#2683 phase 3): one frame through 12 carriers (three
 * whole bins x L = 4), against the frame batch of 12 hand-corrected copies
 * that a caller would otherwise build, and the coarse search ruling 9
 * prefers: L = 2 alone, and bank_detect's L = 2 search with an L = 4 refine
 * on a frame holding one signal.  64 x 512, one thread; setup (create, the
 * copies, the stimulus) is outside the timed region. */
static void
bench_bank (jm_bench_t *bench, uint32_t *seed)
{
  enum
  {
    KREPS = 10
  };
  const size_t    ny = 64, nx = 512, n = ny * nx, H = 12;
  float _Complex *code   = malloc (nx * sizeof *code);
  float _Complex *x      = malloc (n * sizeof *x);
  float _Complex *copies = malloc (H * n * sizeof *copies);
  float          *pw     = malloc (n * sizeof *pw);
  uint32_t       *idx    = malloc (n * sizeof *idx);
  float          *rp     = malloc (n * sizeof *rp);
  int32_t        *rg     = malloc (n * sizeof *rg);
  uint32_t       *ra = malloc (n * sizeof *ra), *rr = malloc (n * sizeof *rr);
  uint32_t       *rc = malloc (n * sizeof *rc);
  for (size_t j = 0; j < nx; j++)
    code[j] = (_xorshift32 (seed) & 1u) ? 1.0f : -1.0f;
  /* the code at lag 77 on a carrier a quarter bin off nominal, in noise */
  for (size_t m = 0; m < n; m++)
    {
      const double a = 2.0 * 3.14159265358979323846 * 0.25 * (double)m / nx;
      x[m]           = 0.1f * code[(m % nx + nx - 77) % nx]
                           * ((float)cos (a) + (float)sin (a) * I)
                       + _rand_uniform (seed) + _rand_uniform (seed) * I;
    }
  for (size_t h = 0; h < H; h++) /* carrier g = h - 4, on a 1/(4 nx) grid */
    for (size_t i = 0; i < ny; i++)
      for (size_t p = 0; p < nx; p++)
        {
          const double a = -2.0 * 3.14159265358979323846 * ((double)h - 4.0)
                           * (double)p / (4.0 * nx);
          copies[h * n + i * nx + p]
              = x[i * nx + p] * ((float)cos (a) + (float)sin (a) * I);
        }
  dp_corr2d_state_t *dd
      = dp_corr2d_create_delay_doppler (code, nx, ny, 1, 1, 0);
  /* a candidate threshold at half the coarse peak: one signal, one record */
  dp_corr2d_bank_power_max (dd, x, n, 2, -2, 3, NULL, 0, pw, idx, n);
  float top = 0.0f;
  for (size_t k = 0; k < n; k++)
    top = pw[k] > top ? pw[k] : top;
  const float        t = 0.5f * top;
  static const char *name[4]
      = { "bank_64x512_c12_corrected_batch_1t", "bank_64x512_c12_l4_1t",
          "bank_64x512_c6_l2_1t", "bank_64x512_detect_l2_l4_1t" };
  double bt[KREPS];
  double best[4];
  size_t recs = 0;
  for (int which = 0; which < 4; which++)
    {
      best[which] = 1e30;
      for (int r = 0; r < KREPS; r++)
        {
          const uint64_t t0 = jm_bench_now_ns ();
          if (which == 0)
            dp_corr2d_execute_power_max (dd, copies, H * n, pw, idx, n);
          else if (which == 1)
            dp_corr2d_bank_power_max (dd, x, n, 4, -4, 7, NULL, 0, pw, idx, n);
          else if (which == 2)
            dp_corr2d_bank_power_max (dd, x, n, 2, -2, 3, NULL, 0, pw, idx, n);
          else
            recs = dp_corr2d_bank_detect (dd, x, n, 2, 4, -2, 3, t, t, NULL, 0,
                                          rp, rg, ra, rr, rc, n);
          bt[r]       = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
          best[which] = bt[r] < best[which] ? bt[r] : best[which];
        }
      jm_bench_add_unit (bench, name[which], bt, KREPS, n, JM_BENCH_SAMPLE);
      printf ("  %-36s %8.2f ms\n", name[which], best[which] * 1e3);
    }
  printf ("  the bank is %.2fx the corrected batch; L = 2 is %.0f%% of L = 4;"
          "\n  detect 2 -> 4 refines %zu candidate(s) for %.2f ms over L = 2"
          "\n",
          best[0] / best[1], 100.0 * best[2] / best[1], recs,
          (best[3] - best[2]) * 1e3);
  dp_corr2d_destroy (dd);
  free (code), free (x), free (copies), free (pw), free (idx), free (rp);
  free (rg), free (ra), free (rr), free (rc);
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

  bench_batches (&_bench, &seed);
  bench_bank (&_bench, &seed);

  jm_bench_write_json (&_bench, "corr2d");
  return 0;
}
