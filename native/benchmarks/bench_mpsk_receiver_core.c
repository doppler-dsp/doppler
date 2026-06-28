/* bench_mpsk_receiver_core.c — no step() to benchmark */
#include "jm_bench.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
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
  mpsk_receiver_state_t *obj = mpsk_receiver_create (
      4, 8, 4, 0, 0.35, 8, 0.01, 0.707, 0.01, 0, 0.5, 0.0, 100, 0);
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== mpsk_receiver benchmark ===\n");
  printf ("  (no step(); methods below)\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  jm_bench_write_json (&_bench, "mpsk_receiver");
  mpsk_receiver_destroy (obj);
  return 0;
}
