/* Per-stage time and logical bytes of the search's delay-Doppler map at the
 * #2457 design geometry with a carrier set (F = 4), by thread count.
 * Logical bytes = each stage's buffers read + written once (no cache
 * model): the floor of what the stage must move if nothing stays cached. */
#include "correlator_map.h"
#include "doppler/dp_parallel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double
now (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return t.tv_sec + 1e-9 * t.tv_nsec;
}

static void
nofold (void *ctx, size_t r, const float _Complex *out)
{
  (void)ctx, (void)r, (void)out;
}

int
main (int argc, char **argv)
{
  const size_t W         = argc > 1 ? strtoul (argv[1], 0, 10) : 3;
  const int    sch_force = argc > 2 ? atoi (argv[2]) : -1;
  const char  *tl        = getenv ("THREADS");
  const size_t N = 2046, L = 2048, D = 90, DI = 180, F = DP__DD_MAP_FINE;
  const dp__dd_geom_t g   = { N, L, D, DI, F };
  const double        spb = (double)D * N; /* samples a block */
  float _Complex     *x   = malloc (D * N * sizeof *x),
                 *X       = malloc (D * N * sizeof *X);
  float _Complex *out     = calloc (W * DI * L, sizeof *out);
  size_t         *rows    = malloc (W * DI * sizeof *rows);
  dp__dd_tile_t  *tiles   = calloc (W, sizeof *tiles);
  int32_t        *off     = malloc (W * N * sizeof *off);
  float _Complex *wt      = malloc (W * N * sizeof *wt),
                 *base    = malloc (N * sizeof *base);
  for (size_t i = 0; i < D * N; i++)
    x[i] = (rand () / (float)RAND_MAX - .5f)
           + I * (rand () / (float)RAND_MAX - .5f);
  for (size_t j = 0; j < N; j++)
    base[j] = 1.0f / N;
  for (size_t q = 0; q < W; q++)
    {
      for (size_t i = 0; i < DI; i++)
        rows[q * DI + i] = q * DI + i;
      dp__dd_ramp_table (&g, (double)q * 0.37, base, off + q * N, wt + q * N);
      tiles[q] = (dp__dd_tile_t){ (q * 97) % N, off + q * N, wt + q * N,
                                  rows + q * DI };
    }
  int ts[] = { 1, 2, 4, 8, 12, 16 };
  for (int it = 0; it < 6; it++)
    {
      int nt = ts[it];
      if (tl && atoi (tl) != nt)
        continue;
      dp__dd_map_t *m = dp__dd_map_create (&g, nt);
      dp__dd_map_force (m, sch_force);
      int             sch  = dp__dd_map_pick (m, W, 1, 0);
      size_t          wl   = sch == DP__DD_ROWS_FIRST ? W * D * L : F * DI * N;
      float _Complex *work = malloc (wl * sizeof *work);
      dp_pool_t      *pool = nt > 1 ? dp_pool_create (nt) : NULL;
      double          bf = 1e9, bm = 1e9;
      for (int r = 0; r < 7; r++)
        {
          double t0 = now ();
          dp__dd_map_forward (m, pool, -1, x, X);
          double t1 = now ();
          dp__dd_map_run (m, pool, -1, X, NULL, 1, tiles, W, work, out, nofold,
                          NULL);
          double t2 = now ();
          if (t1 - t0 < bf)
            bf = t1 - t0;
          if (t2 - t1 < bm)
            bm = t2 - t1;
        }
      /* logical bytes a block, cf32 = 8 B */
      double fwdB = 2.0 * D * N * 8;
      double mapB = sch == DP__DD_ROWS_FIRST
                        ? (W * D * N + 2.0 * W * D * L + W * DI * L) * 8
                        : (D * N + 2.0 * F * DI * N + W * DI * L) * 8;
      printf ("W=%2zu %s thr=%2d  fwd %6.2f ns/s %5.1f GB/s  map %6.2f ns/s "
              "%5.1f GB/s  (logical B/sample: fwd %.0f map %.0f)\n",
              W, sch == DP__DD_ROWS_FIRST ? "rows" : "slow", nt,
              bf / spb * 1e9, fwdB / bf / 1e9, bm / spb * 1e9, mapB / bm / 1e9,
              fwdB / spb, mapB / spb);
      dp_pool_destroy (pool);
      free (work);
      dp__dd_map_destroy (m);
    }
  return 0;
}
