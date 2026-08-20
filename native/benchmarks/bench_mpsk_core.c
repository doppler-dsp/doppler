/* bench_mpsk_core.c — the constellation, which every M-PSK link runs twice.
 *
 * These are the library's bits<->symbol kernels: one `mpsk_map` per
 * transmitted symbol, one `mpsk_demap` or `mpsk_soft_demap` per received
 * one. They are small, they are unavoidable, and until this file existed
 * nothing timed them -- the `mpsk` component is a module of free functions
 * rather than a jm object, so `jm bench` cannot see it even now that one is
 * written. Run it by hand until just-makeit#1023 ships:
 *
 *   cmake --build build --target bench_mpsk_core && ./build/bench_mpsk_core
 *
 * What the numbers are for:
 *
 *   map / demap        the hard-decision pair, per symbol, at M = 2, 4, 8
 *   diff_map/demap     the differential pair, which carries a running
 *                      reference and so cannot vectorise the way the
 *                      memoryless one can -- the gap is the cost of
 *                      differential encoding, stated rather than assumed
 *   soft_demap         per-BIT LLRs, the input a soft decoder needs. It
 *                      produces bps times as many outputs as demap and is
 *                      the one that shows up in a receiver profile
 *
 * Swept over M because the per-symbol cost and the per-BIT cost diverge:
 * 8PSK moves three bits per symbol, so a per-symbol number flatters it by
 * three against BPSK. Both columns are printed for that reason.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "jm_bench.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 100

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

static void
report (const char *name, const double *t, int m)
{
  double s   = min_sec (t, ITERATIONS);
  double ns  = s / (double)BENCH_N * 1e9;
  int    bps = mpsk_bits_per_symbol (m);
  printf ("  %-22s %7.2f ns/sym  %7.2f ns/bit  %8.1f Msym/s\n", name, ns,
          ns / (double)bps, (double)BENCH_N / s / 1e6);
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  uint8_t       *sym = malloc (BENCH_N);
  uint8_t       *out = malloc (BENCH_N);
  float complex *iq  = malloc (BENCH_N * sizeof *iq);
  float         *llr = malloc ((size_t)BENCH_N * 3 * sizeof *llr);
  if (!sym || !out || !iq || !llr)
    return 1;

  printf ("=== mpsk benchmark ===\n");
  printf ("block = %d symbols, %d rounds\n\n", BENCH_N, ITERATIONS);

  const int M[3] = { 2, 4, 8 };

  for (int mi = 0; mi < 3; mi++)
    {
      const int m   = M[mi];
      const int bps = mpsk_bits_per_symbol (m);

      /* Symbols spread over the whole alphabet. A constant symbol would let
         the branch predictor and the slicer's atan2 see one input forever,
         which is not what a modulated stream looks like. */
      uint32_t lfsr = 0xBEEFu;
      for (int i = 0; i < BENCH_N; i++)
        {
          lfsr   = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
          sym[i] = (uint8_t)(lfsr & (uint32_t)(m - 1));
        }
      mpsk_map (sym, BENCH_N, iq, m);

      char          name[64];
      static double t_map[3][ITERATIONS], t_dem[3][ITERATIONS];
      static double t_dmap[3][ITERATIONS], t_ddem[3][ITERATIONS];
      static double t_soft[3][ITERATIONS];

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          mpsk_map (sym, BENCH_N, iq, m);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_map[mi][r] = elapsed_sec (&t0, &t1);
        }
      (void)snprintf (name, sizeof name, "map[m=%d]", m);
      jm_bench_add (&_bench, name, t_map[mi], ITERATIONS, BENCH_N);
      report (name, t_map[mi], m);

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          mpsk_demap (iq, BENCH_N, out, m);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_dem[mi][r] = elapsed_sec (&t0, &t1);
        }
      (void)snprintf (name, sizeof name, "demap[m=%d]", m);
      jm_bench_add (&_bench, name, t_dem[mi], ITERATIONS, BENCH_N);
      report (name, t_dem[mi], m);

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          mpsk_diff_map (sym, BENCH_N, iq, m);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_dmap[mi][r] = elapsed_sec (&t0, &t1);
        }
      (void)snprintf (name, sizeof name, "diff_map[m=%d]", m);
      jm_bench_add (&_bench, name, t_dmap[mi], ITERATIONS, BENCH_N);
      report (name, t_dmap[mi], m);

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          mpsk_diff_demap (iq, BENCH_N, out, m);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_ddem[mi][r] = elapsed_sec (&t0, &t1);
        }
      (void)snprintf (name, sizeof name, "diff_demap[m=%d]", m);
      jm_bench_add (&_bench, name, t_ddem[mi], ITERATIONS, BENCH_N);
      report (name, t_ddem[mi], m);

      mpsk_map (sym, BENCH_N, iq, m);
      const size_t n_llr = (size_t)BENCH_N * (size_t)bps;
      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          mpsk_soft_demap (iq, BENCH_N, llr, n_llr, m, 0.1f);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_soft[mi][r] = elapsed_sec (&t0, &t1);
        }
      (void)snprintf (name, sizeof name, "soft_demap[m=%d]", m);
      jm_bench_add (&_bench, name, t_soft[mi], ITERATIONS, BENCH_N);
      report (name, t_soft[mi], m);

      printf ("\n");
    }

  free (sym);
  free (out);
  free (iq);
  free (llr);
  jm_bench_write_json (&_bench, "mpsk");
  return 0;
}
