/* bench_acquire_core.c — the acquire module's one free function
 * (gh-1034), as a C caller pays for it through the wrapper.
 *
 * `bin_to_signed` is a thin wrapper over the `static inline`
 * `dp_fftfreq_index` (clib_common.h), in its own translation unit so the
 * Python face can call the same code. A C caller therefore cannot inline it,
 * and this row is the price of one call: what a hot C loop pays if it uses
 * the wrapper instead of the inline.
 *
 * There is no inline twin. With a grid size the compiler can see, the
 * inlined loop is folded and vectorized, so it timed a loop rather than a
 * call (about one cycle per "call") and could not be compared with this row
 * (doppler#2386).
 */
#include "doppler/acquire/acquire_core.h"
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

  printf ("=== acquire benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  uint64_t t0, t1;
  double   times[ITERATIONS];
  /* The call sits in another translation unit, so it cannot be folded
   * away; the accumulator is volatile so the loop's result is kept. */
  volatile long sink  = 0;
  const size_t  nbins = 8;

  for (int r = 0; r < ITERATIONS; r++)
    {
      t0       = jm_bench_now_ns ();
      long acc = 0;
      for (int i = 0; i < BENCH_N; i++)
        acc += dp_bin_to_signed ((size_t)i % nbins, nbins);
      sink     = acc;
      t1       = jm_bench_now_ns ();
      times[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add_unit (&_bench, "bin_to_signed (wrapper, not inlinable)", times,
                     ITERATIONS, BENCH_N, JM_BENCH_CALL);

  (void)sink;
  jm_bench_write_json (&_bench, "acquire");
  return 0;
}
