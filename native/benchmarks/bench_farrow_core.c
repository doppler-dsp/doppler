/* bench_farrow_core.c — the Farrow fractional-delay interpolator.
 *
 *   delay[order] — throughput (MSa/s) of push + eval at a constant fractional
 *                  delay over a 64k block, per interpolator order.
 */
#include "doppler/dp_complex.h"
#include "doppler/farrow/farrow_core.h"
#include "jm_bench.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  float _Complex *x   = malloc (BENCH_N * sizeof (*x));
  float _Complex *out = malloc (BENCH_N * sizeof (*out));
  if (!x || !out)
    return 1;
  for (int i = 0; i < BENCH_N; i++)
    x[i] = cexpf ((float)(2.0 * M_PI * 0.05 * i) * I);

  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  printf ("=== farrow benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  const char *names[3] = { "delay_linear", "delay_parabolic", "delay_cubic" };
  for (int order = 0; order <= 2; order++)
    {
      dp_farrow_state_t *f = dp_farrow_create (order);
      dp_farrow_delay (f, x, 64, 0.5, out, BENCH_N); /* warmup */
      double times[ITERATIONS];
      for (int r = 0; r < ITERATIONS; r++)
        {
          dp_farrow_reset (f);
          t0 = jm_bench_now_ns ();
          dp_farrow_delay (f, x, BENCH_N, 0.5, out, BENCH_N);
          t1       = jm_bench_now_ns ();
          times[r] = jm_bench_elapsed_sec (t0, t1);
        }
      jm_bench_add (&_bench, names[order], times, ITERATIONS, BENCH_N);
      double sum = 0.0;
      for (int r = 0; r < ITERATIONS; r++)
        sum += times[r];
      printf ("  %-16s %8.1f MSa/s\n", names[order],
              (double)BENCH_N / (sum / ITERATIONS) / 1e6);
      dp_farrow_destroy (f);
    }

  jm_bench_write_json (&_bench, "farrow");
  free (x);
  free (out);
  return 0;
}
