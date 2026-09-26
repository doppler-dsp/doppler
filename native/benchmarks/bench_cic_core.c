/* bench_cic_core.c — CIC decimation filter benchmark */
#include "doppler/cic/cic_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  /* R=32, N=4 (fixed), M=1 (fixed) — typical SDR first-stage decimator */
  dp_cic_state_t *obj = dp_cic_create (32);
  uint64_t        t0, t1;
  jm_bench_t      _bench = { 0 };

  float _Complex *in  = calloc (BENCH_N, sizeof (float _Complex));
  float _Complex *out = calloc (BENCH_N, sizeof (float _Complex));
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
        dp_cic_reset (obj);
        t0 = jm_bench_now_ns ();
        for (int i = 0; i < BENCH_N; i++)
          {
            /* inline to avoid call overhead per-sample */
          }
        dp_cic_reset (obj);
        t0 = jm_bench_now_ns ();
        dp_cic_decimate (obj, in, BENCH_N, out, BENCH_N);
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add (&_bench, "decimate", times, ITERATIONS, BENCH_N);
  }

  jm_bench_write_json (&_bench, "cic");
  free (in);
  free (out);
  dp_cic_destroy (obj);
  return 0;
}
