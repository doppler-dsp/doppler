/* bench_awgn_core.c — AWGN throughput benchmark */
#include "doppler/awgn/awgn_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 200

static void
bench_n (int n, int iters, jm_bench_t *bench)
{
  dp_awgn_state_t *g   = dp_awgn_create (0, 1.0f);
  float _Complex  *buf = malloc ((size_t)n * sizeof *buf);
  double           times[ITERATIONS];

  dp_awgn_generate (g, (size_t)n, buf, (size_t)n); /* warm up */

  uint64_t t0, t1;
  for (int i = 0; i < iters; i++)
    {
      t0 = jm_bench_now_ns ();
      dp_awgn_generate (g, (size_t)n, buf, (size_t)n);
      t1       = jm_bench_now_ns ();
      times[i] = jm_bench_elapsed_sec (t0, t1);
    }

  double mean = 0;
  for (int i = 0; i < iters; i++)
    mean += times[i];
  mean /= iters;

  printf ("  AWGN(n=%-7d)   %6.1f MSa/s   (%5.1f µs/call)\n", n,
          (double)n / mean / 1e6, mean * 1e6);

  char label[64];
  snprintf (label, sizeof label, "generate(n=%d)", n);
  jm_bench_add (bench, label, times, iters, (size_t)n);

  free (buf);
  dp_awgn_destroy (g);
}

int
main (void)
{
  jm_bench_t bench = { 0 };
  printf ("=== awgn benchmark ===\n");
  printf ("block sizes, %d iterations\n\n", ITERATIONS);

  bench_n (1024, ITERATIONS, &bench);
  bench_n (4096, ITERATIONS, &bench);
  bench_n (65536, ITERATIONS, &bench);

  jm_bench_write_json (&bench, "awgn");
  return 0;
}
