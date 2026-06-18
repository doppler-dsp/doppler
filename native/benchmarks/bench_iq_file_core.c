/* bench_iq_file_core.c — no step() to benchmark */
#include "iq_file/iq_file_core.h"
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
  iq_file_state_t *obj = iq_file_create (NULL, 0, 0);
  struct timespec  t0, t1;
  jm_bench_t       _bench = { 0 };

  printf ("=== iq_file benchmark ===\n");
  printf ("  (no step(); methods below)\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  jm_bench_write_json (&_bench, "iq_file");
  iq_file_destroy (obj);
  return 0;
}
