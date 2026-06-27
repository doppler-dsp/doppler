/* bench_acq_core.c — no step() to benchmark */
#include "acq/acq_core.h"
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
  acq_state_t *obj
      = acq_create (NULL, 0, 1, 1, 1.0e6, 45.0, 0.0, 1e-3, 0.9, 0, 1);
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== acq benchmark ===\n");
  printf ("  (no step(); methods below)\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  jm_bench_write_json (&_bench, "acq");
  acq_destroy (obj);
  return 0;
}
