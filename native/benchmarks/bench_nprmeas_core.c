/* bench_nprmeas_core.c — no step() to benchmark */
#include "jm_bench.h"
#include "nprmeas/nprmeas_core.h"
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
  nprmeas_state_t *obj = nprmeas_create (8192, 1.0, 1.0, 0, 90.0);
  struct timespec  t0, t1;
  jm_bench_t       _bench = { 0 };

  printf ("=== nprmeas benchmark ===\n");
  printf ("  (no step(); methods below)\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  jm_bench_write_json (&_bench, "nprmeas");
  nprmeas_destroy (obj);
  return 0;
}
