/* bench_frame_meter_core.c — no step() to benchmark */
#include "frame_meter/frame_meter_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  frame_meter_state_t *obj    = frame_meter_create (200, 0.99);
  jm_bench_t           _bench = { 0 };

  printf ("=== frame_meter benchmark ===\n");
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

  jm_bench_write_json (&_bench, "frame_meter");
  frame_meter_destroy (obj);
  return 0;
}
