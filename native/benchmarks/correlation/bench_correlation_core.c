/* bench_correlation_core.c — the correlation module's one free function
 * (gh-1034), as a C caller pays for it.
 *
 * dp_correlator_same_code_phase() is the one same-signal key: the
 * receiver's pick calls it per peak of a dwell, and the pool's duplicate
 * test per pair of holders once a control step. It lives in its own
 * translation unit so the Python face calls the same code, so a C caller
 * cannot inline it, and this row is the price of one call. The callers
 * make a handful of calls a dwell or a step, so this is the number that
 * says the call is not worth an inline copy.
 */
#include "doppler/correlation/correlation_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  jm_bench_t _bench = { 0 };

  printf ("=== correlation benchmark ===\n");
  printf ("block = %d calls,  %d iterations\n\n", BENCH_N, ITERATIONS);

  uint64_t t0, t1;
  double   times[ITERATIONS];
  /* The call sits in another translation unit, so it cannot be folded
   * away; the accumulator is volatile so the loop's result is kept. The
   * phases walk the code, so both branches of the fold are taken. */
  volatile long sink = 0;

  for (int r = 0; r < ITERATIONS; r++)
    {
      t0       = jm_bench_now_ns ();
      long acc = 0;
      for (int i = 0; i < BENCH_N; i++)
        acc += dp_correlator_same_code_phase (0.25 * (double)(i % 4092), 512.0,
                                              1023.0);
      sink     = acc;
      t1       = jm_bench_now_ns ();
      times[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add_unit (&_bench, "same_code_phase (one call, not inlinable)",
                     times, ITERATIONS, BENCH_N, JM_BENCH_CALL);

  (void)sink;
  jm_bench_write_json (&_bench, "correlation");
  return 0;
}
