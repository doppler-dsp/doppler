/* bench_cvt_core.c — benchmarks for the cvt module's
 * free functions (gh-1034).
 *
 * The question these ask: what does a frame field's expansion cost per bit,
 * and how much of that is the STRING form's parsing?
 *
 * int_to_bin and hex_to_bin produce identical bits for any value both can
 * express — that agreement is pinned in test_cvt_core — so timing them side
 * by side measures exactly one thing: the price of the text. That is the
 * number behind preferring `int_to_bin(0x1ACFFC1DULL, 32, ...)` over
 * `hex_to_bin("1ACFFC1D", ...)` for a literal that fits in 64 bits, and it
 * is worth having rather than assuming.
 *
 * bin_to_nrz is the other shape: no parsing at all, one branch per bit, and
 * it runs over a whole frame rather than a marker.
 *
 * A `volatile` sink prevents the loops being optimised away.
 */
#include "cvt/cvt_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  double          times[ITERATIONS];
  volatile size_t sink = 0;

  static uint8_t bits[BENCH_N];
  static float   nrz[BENCH_N];
  uint8_t        marker[64];

  printf ("=== cvt benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* A 32-bit literal, expanded from a value. The unit is one marker. */
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        sink += int_to_bin (0x1ACFFC1DULL, 32u, marker, sizeof marker,
                            DP_BITORDER_BIG);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "int_to_bin", times, ITERATIONS, BENCH_N);

  /* The same 32 bits, from text. The difference IS the parsing. */
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < BENCH_N; i++)
        sink
            += hex_to_bin ("1ACFFC1D", marker, sizeof marker, DP_BITORDER_BIG);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "hex_to_bin", times, ITERATIONS, BENCH_N);

  /* A whole frame's worth of bits to symbols, one branch per bit. */
  for (int i = 0; i < BENCH_N; i++)
    bits[i] = (uint8_t)(i & 1u);
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += bin_to_nrz (bits, BENCH_N, nrz, BENCH_N);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "bin_to_nrz", times, ITERATIONS, BENCH_N);

  (void)sink;
  jm_bench_write_json (&_bench, "cvt");
  return 0;
}
