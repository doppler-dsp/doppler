/* bench_async_dsss_receiver_core.c — no step() to benchmark */
#include "async_dsss_receiver/async_dsss_receiver_core.h"
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
  async_dsss_receiver_state_t *obj = async_dsss_receiver_create (
      NULL, 0, 1000000.0, 1000.0, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
      4, 14.0, 64, 8, true, 100000, 0.0);
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== async_dsss_receiver benchmark ===\n");
  printf ("  (no step(); methods below)\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  jm_bench_write_json (&_bench, "async_dsss_receiver");
  async_dsss_receiver_destroy (obj);
  return 0;
}
