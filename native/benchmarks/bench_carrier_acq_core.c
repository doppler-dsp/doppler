/* bench_carrier_acq_core.c — coarse carrier acquisition, per block.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * Acquisition is not a per-sample cost like the tracking loops: it is a
 * block search that runs at the START of a capture (and again after a loss
 * of lock), so the honest unit is TIME PER BLOCK and the honest question is
 * how it scales with the two knobs a caller sets.
 *
 *   zero_pad   finer frequency resolution for a longer transform
 *   sequential whether blocks are combined sequentially, which trades
 *              latency for sensitivity and changes how many blocks the
 *              search consumes before it decides
 *
 * A receiver's time-to-first-lock is this plus the loops' settling, so this
 * is the half a link budget usually forgets.
 *
 * Timing is MIN over rounds, not mean, after a WARMUP_S settle.
 */
#include "carrier_acq/carrier_acq_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 50
#define WARMUP_S 0.25
#define SAMPLE_RATE_HZ 4.0e6
#define SYMBOL_RATE_HZ 1.0e6
#define MAX_N_BLOCKS 8

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

  float complex *x = malloc (BENCH_N * sizeof *x);
  if (!x)
    return 1;
  /* A BPSK stream with a real offset to find -- an acquisition run on a
     centred carrier searches the same grid but is not the case anyone
     acquires. */
  uint32_t lfsr = 0x31A7u;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr     = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      double s = (lfsr & 1u) ? -1.0 : 1.0;
      double p = 2.0 * M_PI * 0.011 * (double)i;
      x[i]     = (float)(s * cos (p)) + (float)(s * sin (p)) * I;
    }

  printf ("=== carrier_acq benchmark ===\n");
  printf ("fs = %.1f MHz, Rs = %.1f MHz, block = %d, %d rounds\n\n",
          SAMPLE_RATE_HZ / 1e6, SYMBOL_RATE_HZ / 1e6, BENCH_N, ITERATIONS);

  const size_t  pads[2] = { 4, 16 };
  const bool    seqs[2] = { false, true };
  static double t_st[2][2][ITERATIONS];

  for (int p = 0; p < 2; p++)
    for (int q = 0; q < 2; q++)
      {
        carrier_acq_state_t *ca = carrier_acq_create (
            SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, 0.0, pads[p], 0, 0.0f, NULL, 0,
            1e-3, 0.9, 2.0, seqs[q], MAX_N_BLOCKS);
        if (!ca)
          {
            (void)fprintf (stderr,
                           "bench_carrier_acq: create(pad=%zu, seq=%d) "
                           "returned NULL\n",
                           pads[p], (int)seqs[q]);
            return 1;
          }

        struct timespec w0, w1;
        clock_gettime (CLOCK_MONOTONIC, &w0);
        do
          {
            carrier_acq_reset (ca);
            carrier_acq_steps (ca, x, BENCH_N);
            clock_gettime (CLOCK_MONOTONIC, &w1);
          }
        while (elapsed_sec (&w0, &w1) < WARMUP_S);

        for (int r = 0; r < ITERATIONS; r++)
          {
            carrier_acq_reset (ca);
            clock_gettime (CLOCK_MONOTONIC, &t0);
            carrier_acq_steps (ca, x, BENCH_N);
            clock_gettime (CLOCK_MONOTONIC, &t1);
            t_st[p][q][r] = elapsed_sec (&t0, &t1);
          }
        char name[64];
        (void)snprintf (name, sizeof name, "steps[pad=%zu%s]", pads[p],
                        seqs[q] ? ",seq" : "");
        jm_bench_add (&_bench, name, t_st[p][q], ITERATIONS, BENCH_N);
        double sec = min_sec (t_st[p][q], ITERATIONS);
        printf ("  %-22s %8.3f ms/block  %7.2f ns/sample\n", name, sec * 1e3,
                sec / (double)BENCH_N * 1e9);
        carrier_acq_destroy (ca);
      }

  printf ("\n  4x the zero-pad costs %.2fx, but SEQUENTIAL combining is the\n"
          "  bigger lever: %.1fx at pad=4 and %.1fx at pad=16. That is the\n"
          "  sensitivity-for-time trade priced -- it consumes more blocks\n"
          "  before deciding, and each one is searched.\n"
          "\n  All of it is paid ONCE per capture (and again after a loss of\n"
          "  lock), so it belongs in a time-to-first-lock budget beside the\n"
          "  loops' settling time, never in the per-sample throughput the\n"
          "  tracking rows report.\n",
          min_sec (t_st[1][0], ITERATIONS) / min_sec (t_st[0][0], ITERATIONS),
          min_sec (t_st[0][1], ITERATIONS) / min_sec (t_st[0][0], ITERATIONS),
          min_sec (t_st[1][1], ITERATIONS) / min_sec (t_st[1][0], ITERATIONS));

  free (x);
  jm_bench_write_json (&_bench, "carrier_acq");
  return 0;
}
