/* bench_cic_core.c — CIC decimation filter benchmark */
#include "cic/cic_core.h"
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
  /* R=32, N=4 (fixed), M=1 (fixed) — typical SDR first-stage decimator */
  cic_state_t    *obj = cic_create (32);
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  float complex *in  = calloc (BENCH_N, sizeof (float complex));
  float complex *out = calloc (BENCH_N, sizeof (float complex));
  for (int i = 0; i < BENCH_N; i++)
    in[i] = 0.5f + 0.0f * _Complex_I;

  printf ("=== cic benchmark ===\n");
  printf ("R=32 N=4 M=1,  block=%d samples,  %d iterations\n\n", BENCH_N,
          ITERATIONS);

  /* --- decimate (65536 in → ~2048 out) --- */
  {
    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        cic_reset (obj);
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          {
            /* inline to avoid call overhead per-sample */
          }
        cic_reset (obj);
        clock_gettime (CLOCK_MONOTONIC, &t0);
        cic_decimate (obj, in, BENCH_N, out);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "decimate", times, ITERATIONS, BENCH_N);
  }

  jm_bench_write_json (&_bench, "cic");
  free (in);
  free (out);
  cic_destroy (obj);
  return 0;
}
