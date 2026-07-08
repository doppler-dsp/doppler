/* bench_hbdecim_q15_core.c — execute() throughput for HalfbandDecimatorQ15 */
#include "hbdecim_q15/hbdecim_q15_core.h"
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

/* Minimal 19-tap halfband FIR branch (compact non-zero form). */
static const float H19[19]
    = { 0.001579f,  -0.004676f, 0.010443f,  -0.020175f, 0.035799f,
        -0.060866f, 0.104113f,  -0.197538f, 0.631601f,  0.631601f,
        -0.197538f, 0.104113f,  -0.060866f, 0.035799f,  -0.020175f,
        0.010443f,  -0.004676f, 0.001579f,  0.0f };

int
main (void)
{
  hbdecim_q15_state_t *obj = hbdecim_q15_create (19, H19);
  if (!obj)
    {
      fprintf (stderr, "create failed\n");
      return 1;
    }

  int16_t *in  = calloc (2 * BENCH_N, sizeof (int16_t));
  int16_t *out = calloc (2 * BENCH_N, sizeof (int16_t));
  if (!in || !out)
    {
      fprintf (stderr, "alloc failed\n");
      return 1;
    }
  for (size_t i = 0; i < 2 * BENCH_N; i++)
    in[i] = (int16_t)(i & 0x7fff);

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== hbdecim_q15 benchmark ===\n");
  printf ("block = %d IQ pairs,  %d iterations\n\n", BENCH_N, ITERATIONS);

  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    {
      hbdecim_q15_execute (obj, in, BENCH_N, out, BENCH_N);
      hbdecim_q15_reset (obj);
    }
  clock_gettime (CLOCK_MONOTONIC, &t1);

  double dt = elapsed_sec (&t0, &t1) / ITERATIONS;
  printf ("execute %d IQ pairs: %.3f ms  (%.0f Msps)\n", BENCH_N, dt * 1e3,
          (double)BENCH_N / dt / 1e6);

  jm_bench_write_json (&_bench, "hbdecim_q15");
  free (in);
  free (out);
  hbdecim_q15_destroy (obj);
  return 0;
}
