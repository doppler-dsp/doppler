/* bench_burst_despreader_core.c — despreading, per chip.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * The despreader runs at the CHIP rate, which is the fastest clock in a
 * DSSS receiver: at spreading factor SF it sees SF times more samples than
 * the symbol layer above it. So a per-symbol number here would flatter it
 * by SF, and both columns are printed for that reason.
 *
 * Swept over spreading factor, because SF is the link's own trade -- more
 * processing gain for more chips -- and the question is whether the
 * despreader's per-chip cost is flat in SF (so gain is bought at exactly
 * the chip rate) or grows with it.
 *
 * Timing is MIN over rounds, not mean, after a WARMUP_S settle.
 */
#include "burst_despreader/burst_despreader_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 50
#define WARMUP_S 0.25
#define SPS 4

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

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  float complex *x   = malloc (BENCH_N * sizeof *x);
  float complex *out = malloc (BENCH_N * sizeof *out);
  if (!x || !out)
    return 1;
  uint32_t lfsr = 0x13C7u;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr     = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      double s = (lfsr & 1u) ? -1.0 : 1.0;
      x[i]     = (float)s + (float)(0.05 * s) * I;
    }

  printf ("=== burst_despreader benchmark ===\n");
  printf ("block = %d samples, sps = %d, %d rounds\n\n", BENCH_N, SPS,
          ITERATIONS);

  const size_t   sfs[3] = { 15, 31, 63 };
  static double  t_st[3][ITERATIONS];
  static uint8_t code[64];

  for (int k = 0; k < 3; k++)
    {
      const size_t sf = sfs[k];
      for (size_t i = 0; i < sf; i++)
        code[i] = (uint8_t)(((i * 2246822519u) >> 31) & 1u);

      burst_despreader_state_t *d
          = burst_despreader_create (code, sf, sf, SPS, 0.0, 0.0, 0.05, 0.01);
      if (!d)
        {
          (void)fprintf (stderr,
                         "bench_burst_despreader: create(sf=%zu) NULL\n", sf);
          return 1;
        }
      size_t cap = burst_despreader_steps_max_out (d);
      if (cap == 0 || cap > BENCH_N)
        cap = BENCH_N;

      size_t got = burst_despreader_steps (d, x, BENCH_N, out, cap);
      if (got == 0)
        {
          (void)fprintf (stderr,
                         "bench_burst_despreader: sf=%zu emitted 0 of %d — "
                         "the timings below would measure a no-op\n",
                         sf, BENCH_N);
          return 1;
        }

      struct timespec w0, w1;
      clock_gettime (CLOCK_MONOTONIC, &w0);
      do
        {
          sink += burst_despreader_steps (d, x, BENCH_N, out, cap);
          clock_gettime (CLOCK_MONOTONIC, &w1);
        }
      while (elapsed_sec (&w0, &w1) < WARMUP_S);

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += burst_despreader_steps (d, x, BENCH_N, out, cap);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_st[k][r] = elapsed_sec (&t0, &t1);
        }
      char name[64];
      (void)snprintf (name, sizeof name, "steps[sf=%zu]", sf);
      jm_bench_add (&_bench, name, t_st[k], ITERATIONS, BENCH_N);
      double sec = min_sec (t_st[k], ITERATIONS);
      printf ("  %-18s %7.2f ns/sample  %8.1f MSa/s  %8.3f Msym/s\n", name,
              sec / (double)BENCH_N * 1e9, (double)BENCH_N / sec / 1e6,
              (double)BENCH_N / (double)(sf * SPS) / sec / 1e6);
      burst_despreader_destroy (d);
    }

  printf ("\n  sf 15 -> 63 costs %.2fx per SAMPLE. Flat means processing\n"
          "  gain is bought at exactly the chip rate and nothing more --\n"
          "  the symbol column falls with SF because each symbol simply\n"
          "  spans more chips.\n",
          min_sec (t_st[2], ITERATIONS) / min_sec (t_st[0], ITERATIONS));

  (void)sink;
  free (x);
  free (out);
  jm_bench_write_json (&_bench, "burst_despreader");
  return 0;
}
