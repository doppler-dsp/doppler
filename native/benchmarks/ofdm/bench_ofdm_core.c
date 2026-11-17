/* bench_ofdm_core.c -- what one OFDM symbol costs to modulate.
 *
 * Each row renders a run of OFDM symbols through `dp_ofdm_modulate` and
 * reports the cost per OFDM symbol. The layouts are generic ones, with four
 * fixed pilots and a 7/8 occupancy, at the transform lengths a wide range of
 * OFDM signals use; none is any standard's. The cost is bounded below by the
 * inverse transform, so the rows say how much the placement, the prefix and
 * the window add to it.
 *
 *   make bench BENCH_ARGS="--c-only ofdm"
 *
 * Timing is MIN over rounds, not mean: benchmark noise is one-sided.
 */
#include "doppler/ofdm/ofdm_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITERATIONS 100

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

/* A layout of `count` OFDM symbols of one class: n_fft/8 null subcarriers at
   each edge and DC, four pilots, the rest data; prefix n_fft/4. */
static dp_ofdm_state_t *
layout (uint32_t n, uint32_t window, uint32_t count)
{
  dp_ofdm_state_t *o   = dp_ofdm_create ();
  uint8_t         *map = malloc (n);
  if (!o || !map)
    exit (1);
  for (uint32_t i = 0; i < n; i++)
    map[i] = (i >= n / 16 && i < n - n / 16 && i != n / 2) ? OFDM_DATA
                                                           : OFDM_NULL;
  const float _Complex pol[2] = { 1.0f, -1.0f };
  if (dp_ofdm_add_class (o, n, map, n, 0u, n / 4, window, 1.0, 1u) != 0
      || dp_ofdm_add_polarity (o, 0u, pol, 2u) != 0)
    exit (1);
  const int32_t pk[4] = { -(int32_t)(n / 4), -(int32_t)(n / 8),
                          (int32_t)(n / 8), (int32_t)(n / 4) };
  for (int i = 0; i < 4; i++)
    if (dp_ofdm_add_fixed (o, 0u, OFDM_ALL_PHASES, pk[i], 1.0) != 0)
      exit (1);
  if (dp_ofdm_add_run (o, 0u, count, OFDM_CP_DEFAULT, 0u, 0u) != 0)
    exit (1);
  free (map);
  return o;
}

int
main (void)
{
  jm_bench_t _bench = { 0 };
  printf ("=== ofdm benchmark ===\n");
  printf ("per OFDM symbol, %d rounds\n\n", ITERATIONS);

  static const uint32_t N[4] = { 64, 256, 1024, 4096 };
  static double         t[5][ITERATIONS];
  const struct
  {
    uint32_t    ni, window;
    const char *name;
  } rows[5] = {
    { 0, 0, "modulate[n_fft=64]" },   { 0, 2, "modulate[n_fft=64,window=2]" },
    { 1, 0, "modulate[n_fft=256]" },  { 2, 0, "modulate[n_fft=1024]" },
    { 3, 0, "modulate[n_fft=4096]" },
  };

  for (int row = 0; row < 5; row++)
    {
      const uint32_t n = N[rows[row].ni];
      /* about 64k samples a round whatever the length */
      const uint32_t   count = 65536u / (n + n / 4);
      dp_ofdm_state_t *o     = layout (n, rows[row].window, count);
      const size_t     np    = dp_ofdm_points_remaining (o);
      float _Complex  *pts   = malloc (np * sizeof *pts);
      float _Complex *out = malloc ((size_t)count * (n + n / 4) * sizeof *out);
      if (!pts || !out)
        return 1;
      for (size_t i = 0; i < np; i++)
        pts[i] = (float)(1 - 2 * (int)(i & 1u)) * 0.7f
                 + (float)(1 - 2 * (int)((i >> 1) & 1u)) * 0.7f * I;
      const size_t cap = (size_t)count * (n + n / 4);
      for (int r = 0; r < ITERATIONS; r++)
        {
          dp_ofdm_reset (o);
          const uint64_t t0 = jm_bench_now_ns ();
          if (dp_ofdm_modulate (o, pts, np, out, cap) != cap)
            return 1;
          const uint64_t t1 = jm_bench_now_ns ();
          t[row][r]         = jm_bench_elapsed_sec (t0, t1);
        }
      jm_bench_add_unit (&_bench, rows[row].name, t[row], ITERATIONS, count,
                         "symbol");
      const double s = min_sec (t[row], ITERATIONS);
      printf ("  %-30s %9.1f ns/OFDM symbol  %8.2f ns/sample\n",
              rows[row].name, s / count * 1e9,
              s / ((double)count * (n + n / 4)) * 1e9);
      free (pts);
      free (out);
      dp_ofdm_destroy (o);
    }

  jm_bench_write_json (&_bench, "ofdm");
  return 0;
}
