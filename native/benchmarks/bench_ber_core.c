/* bench_ber_core.c — the measurement stack, whose cost lands on the HARNESS.
 *
 * Nothing here runs in a receiver's data path, which is exactly why it went
 * unmeasured: `ber` is the layer every test, sweep and validation report is
 * built on, so its cost is paid by the suite rather than by a link. A sweep
 * that scores 40 Es/N0 points x 5 seeds calls `ber_evm_db` 200 times over
 * blocks this size, and `make characterize` exists because those sweeps got
 * slow enough to need their own target.
 *
 * Two shapes, and they are not comparable:
 *
 *   evm_db          O(n) over the received symbols -- a hard decision, a
 *                   rotation estimate and an error accumulation per symbol.
 *                   This is what a sweep pays per point.
 *   theory_ser/ber  closed form, one call per point. Nanoseconds, and here
 *                   only to establish that they ARE nanoseconds: if a
 *                   sweep is slow it is not these.
 *   qfunc           the primitive under both, and under every threshold in
 *                   `detection`. Worth its own row because a change to it
 *                   moves everything above.
 *
 * `ber_settle_syms` is deliberately absent: it is arithmetic on two doubles
 * with no loop, so timing it would measure the clock.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "ber/ber_core.h"
#include "jm_bench.h"
#include "mpsk/mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 100
/* Scalar closed forms need an inner loop of their own -- one call is far
   below the clock's resolution. */
#define SCALAR_N 100000

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

  uint8_t       *sym = malloc (BENCH_N);
  float complex *rx  = malloc (BENCH_N * sizeof *rx);
  if (!sym || !rx)
    return 1;

  printf ("=== ber benchmark ===\n");
  printf ("block = %d symbols, %d rounds\n\n", BENCH_N, ITERATIONS);

  /* A QPSK stream with a little noise on it, so the hard decisions inside
     evm_db are not all exact and the error accumulation has something to
     accumulate. Deterministic: an LFSR for the symbols, a fixed wobble for
     the error, so the measurement repeats. */
  uint32_t lfsr = 0xC0DEu;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr   = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      sym[i] = (uint8_t)(lfsr & 3u);
    }
  mpsk_map (sym, BENCH_N, rx, 4);
  for (int i = 0; i < BENCH_N; i++)
    rx[i] += (float)(0.05 * sin (i * 0.7)) + (float)(0.05 * cos (i * 1.3)) * I;

  static double t_evm[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += ber_evm_db (rx, BENCH_N, 0, BENCH_N, 4);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_evm[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "evm_db", t_evm, ITERATIONS, BENCH_N);
  {
    double s = min_sec (t_evm, ITERATIONS);
    printf ("  %-16s %8.2f ns/sym   %8.3f ms per %d-symbol window\n", "evm_db",
            s / (double)BENCH_N * 1e9, s * 1e3, BENCH_N);
  }

  static double t_ser[ITERATIONS], t_ber[ITERATIONS], t_q[ITERATIONS];

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < SCALAR_N; i++)
        sink += ber_theory_ser (4, 1.0 + (double)(i & 15) * 0.5);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_ser[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "theory_ser", t_ser, ITERATIONS, SCALAR_N);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < SCALAR_N; i++)
        sink += ber_theory_ber (4, 1.0 + (double)(i & 15) * 0.5);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_ber[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "theory_ber", t_ber, ITERATIONS, SCALAR_N);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < SCALAR_N; i++)
        sink += ber_qfunc (0.5 + (double)(i & 31) * 0.1);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_q[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "qfunc", t_q, ITERATIONS, SCALAR_N);

  printf ("  %-16s %8.2f ns/call\n", "theory_ser",
          min_sec (t_ser, ITERATIONS) / SCALAR_N * 1e9);
  printf ("  %-16s %8.2f ns/call\n", "theory_ber",
          min_sec (t_ber, ITERATIONS) / SCALAR_N * 1e9);
  printf ("  %-16s %8.2f ns/call\n", "qfunc",
          min_sec (t_q, ITERATIONS) / SCALAR_N * 1e9);

  /* Compare CALL against CALL. Dividing evm_db's ns/SYMBOL by a closed
     form's ns/CALL is apples to oranges -- it reported "2x" for two
     quantities five orders of magnitude apart. */
  printf ("\n  A 40-point x 5-seed sweep pays evm_db 200 times: %.2f s of\n"
          "  scoring alone, before any signal is generated. One closed-form\n"
          "  call is %.0fx cheaper than one evm_db window, so a slow sweep\n"
          "  is never the theory curves.\n",
          min_sec (t_evm, ITERATIONS) * 200.0,
          min_sec (t_evm, ITERATIONS)
              / (min_sec (t_ser, ITERATIONS) / (double)SCALAR_N));

  (void)sink;
  free (sym);
  free (rx);
  jm_bench_write_json (&_bench, "ber");
  return 0;
}
