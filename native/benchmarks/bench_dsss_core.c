/* bench_dsss_core.c — benchmarks for the dsss module's
 * free functions (gh-1034).
 *
 * There is one function, and the question worth asking about it is not "how
 * fast is a branch and a subtraction" — it is whether the SHAPE of the
 * design costs anything.
 *
 * `dp_fftfreq_index` is a `static inline` in clib_common.h so every C caller
 * inlines it; `bin_to_signed` is a thin wrapper in its own translation unit
 * so the Python face can call the same code. The wrapper therefore CANNOT be
 * inlined by a C caller, and the two numbers below are what that costs. If
 * they are close, a C caller may use either freely; if the wrapper is much
 * slower, hot C paths should stay on the inline — which is exactly the
 * guidance the acquisition engine's per-hypothesis loop needs.
 */
#include "dsss/dsss_core.h"
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
  jm_bench_t _bench = { 0 };

  printf ("=== dsss benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  struct timespec t0, t1;
  double          times[ITERATIONS];
  /* A grid size that is not a compile-time constant, so neither call can be
   * folded away; the accumulator is volatile for the same reason. */
  volatile long sink  = 0;
  size_t        nbins = (size_t)(8 + (BENCH_N & 0));

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      long acc = 0;
      for (int i = 0; i < BENCH_N; i++)
        acc += dp_fftfreq_index ((size_t)i % nbins, nbins);
      sink = acc;
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "dp_fftfreq_index (inlined)", times, ITERATIONS,
                BENCH_N);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      long acc = 0;
      for (int i = 0; i < BENCH_N; i++)
        acc += bin_to_signed ((size_t)i % nbins, nbins);
      sink = acc;
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "bin_to_signed (wrapper, not inlinable)", times,
                ITERATIONS, BENCH_N);

  (void)sink;
  jm_bench_write_json (&_bench, "dsss");
  return 0;
}
