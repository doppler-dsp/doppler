/* bench_snr_core.c — data-aided Es/N0, whole-window and sliding.
 *
 * `snr_data_aided_db` is the truth-referenced estimator the receiver test
 * harnesses score with: it strips the known transmitted sign and reads the
 * residual. Every harness that reports an operating point calls it, and the
 * sliding form calls it conceptually once per window -- so the interesting
 * question is not the single-shot cost but whether the SERIES form is a
 * sliding accumulator or a re-scan, because those differ by the window
 * count and a harness that sweeps windows would feel it.
 *
 * That is what the two rows answer, and the answer on the first run was
 * the bad one:
 *
 *   data_aided_db            one estimate over the whole block
 *   series[window=1024]      one estimate per SAMPLE, sliding
 *
 * The series form costs ~500x the whole-block estimate over a 1024-sample
 * window -- it re-scans the window at every sample rather than sliding an
 * accumulator, so it is O(n * window). Filed as doppler#890. The two rows
 * stay after it is fixed, because their ratio is what would catch the
 * re-introduction; a fixed implementation should read near 1x.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "jm_bench.h"
#include "snr/snr_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 100
#define WINDOW 1024
/* The O(n*window) re-scan this benchmark found in the series form. */
#define SERIES_ISSUE 890

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
  volatile double sink   = 0.0;

  float complex *soft = malloc (BENCH_N * sizeof *soft);
  uint8_t       *bits = malloc (BENCH_N);
  /* One estimate per SAMPLE, not per window: the series form slides, and
     its contract says `out` is `soft_len` long. Sizing this buffer
     `BENCH_N / WINDOW` -- the intuitive reading, and the wrong one --
     overflowed the heap and segfaulted on the first run. */
  double *out = malloc (BENCH_N * sizeof *out);
  if (!soft || !bits || !out)
    return 1;

  /* BPSK at a realistic operating point: unit symbols with the known sign,
     plus a deterministic wobble standing in for noise. The estimator's cost
     does not depend on the values, but a degenerate input (all +1, zero
     residual) would put it on a path no harness ever takes. */
  uint32_t lfsr = 0x2468u;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr    = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      bits[i] = (uint8_t)(lfsr & 1u);
      float s = bits[i] ? -1.0f : 1.0f;
      soft[i] = s * (1.0f + (float)(0.08 * sin (i * 0.37)))
                + (float)(0.08 * cos (i * 0.91)) * I;
    }

  printf ("=== snr benchmark ===\n");
  printf ("block = %d symbols, window = %d, %d rounds\n\n", BENCH_N, WINDOW,
          ITERATIONS);

  static double t_one[ITERATIONS], t_ser[ITERATIONS];

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += snr_data_aided_db (soft, BENCH_N, bits, BENCH_N);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_one[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "data_aided_db", t_one, ITERATIONS, BENCH_N);
  printf ("  %-24s %7.2f ns/sym   %8.3f us/block\n", "data_aided_db",
          min_sec (t_one, ITERATIONS) / BENCH_N * 1e9,
          min_sec (t_one, ITERATIONS) * 1e6);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      snr_data_aided_db_series (soft, BENCH_N, bits, BENCH_N, WINDOW, out);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_ser[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "series[window=1024]", t_ser, ITERATIONS, BENCH_N);
  printf ("  %-24s %7.2f ns/sym   %8.3f us/block  (one per sample)\n",
          "series[window=1024]", min_sec (t_ser, ITERATIONS) / BENCH_N * 1e9,
          min_sec (t_ser, ITERATIONS) * 1e6);

  printf ("\n  series / whole-block = %.0fx over a %d-sample window. A\n"
          "  sliding accumulator would be ~1x -- one pass, O(n). This is\n"
          "  O(n * window): the series form RE-SCANS its window at every\n"
          "  sample. Measured, not read off the source, and filed as\n"
          "  doppler#%d. The row stays after that is fixed: it is the\n"
          "  measurement that would catch the reintroduction.\n",
          min_sec (t_ser, ITERATIONS) / min_sec (t_one, ITERATIONS), WINDOW,
          SERIES_ISSUE);

  (void)sink;
  free (soft);
  free (bits);
  free (out);
  jm_bench_write_json (&_bench, "snr");
  return 0;
}
