/* bench_burst_capture_core.c — no step() to benchmark */
#include "burst_capture/burst_capture_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  burst_capture_state_t *obj = burst_capture_create (
      NULL, 0, 8192, 5, 4, 1000000.0, 50.0, 0.0, 1e-3, 0.9);
  jm_bench_t _bench = { 0 };

  printf ("=== burst_capture benchmark ===\n");
  /* gh-806: "methods below" was a promise, not a fact -- a component whose
   * methods are all variable_output / out_type / varargs / codec has no
   * benchable shape among them, and this line read as though it did.  What
   * actually got measured is reported by jm_bench_write_json(). */
  printf ("  (no step())\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* TODO: benchmark this component.
   * jm did not generate a timing loop: this component has no
   * step() and no methods, so there is nothing it could time for
   * you.
   *
   * The pattern — uncomment and adapt. `jm_bench_add` is what puts a
   * measurement into the JSON; without one this target writes an empty
   * "benchmarks": [] array.
   *
   *   static double
   *   elapsed_sec(struct timespec *t0, struct timespec *t1)
   *   {
   *       return (double)(t1->tv_sec - t0->tv_sec)
   *              + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
   *   }
   *
   *   struct timespec t0, t1;
   *   double times[ITERATIONS];
   *   for (int r = 0; r < ITERATIONS; r++) {
   *       clock_gettime(CLOCK_MONOTONIC, &t0);
   *       ... call the method BENCH_N times ...
   *       clock_gettime(CLOCK_MONOTONIC, &t1);
   *       times[r] = elapsed_sec(&t0, &t1);
   *   }
   *   jm_bench_add(&_bench, "<name>", times, ITERATIONS, BENCH_N);
   */

  jm_bench_write_json (&_bench, "burst_capture");
  burst_capture_destroy (obj);
  return 0;
}
