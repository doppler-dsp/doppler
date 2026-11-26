/* bench_timing_est_core.c — one estimate over a 64k block */
#include "doppler/dp_complex.h"
#include "doppler/timing_est/timing_est_core.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

int
main (void)
{
  dp_timing_est_state_t *obj    = dp_timing_est_create (4.0);
  jm_bench_t             _bench = { 0 };

  printf ("=== timing_est benchmark ===\n");
  /* gh-806: "methods below" was a promise, not a fact -- a component whose
   * methods are all variable_output / out_type / varargs / codec has no
   * benchable shape among them, and this line read as though it did.  What
   * actually got measured is reported by jm_bench_write_json(). */
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* One estimate over the block, per iteration. The stream's power peaks
   * 0.2 symbols into each symbol, and the estimate is checked after the
   * timing: an estimator that returned early would look faster. */
  float _Complex *x = malloc (BENCH_N * sizeof *x);
  if (!obj || !x)
    return 1;
  for (int i = 0; i < BENCH_N; i++)
    {
      double p = 1.0 + 0.5 * cos (6.283185307179586 * ((double)i / 4.0 - 0.2));
      double a = 0.2 * (double)i;
      x[i] = CMPLXF ((float)(sqrt (p) * cos (a)), (float)(sqrt (p) * sin (a)));
    }
  uint64_t             t0, t1;
  static double        times[ITERATIONS];
  dp_timing_estimate_t e = { 0 };
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0       = jm_bench_now_ns ();
      e        = dp_timing_est_estimate (obj, x, BENCH_N);
      t1       = jm_bench_now_ns ();
      times[r] = jm_bench_elapsed_sec (t0, t1);
    }
  if (fabs (e.tau - 0.2) > 1e-3 || e.symbols != BENCH_N / 4.0)
    {
      fprintf (stderr, "bench_timing_est_core: wrong estimate (tau %g)\n",
               e.tau);
      return 1;
    }
  jm_bench_add_unit (&_bench, "estimate", times, ITERATIONS, BENCH_N,
                     JM_BENCH_SAMPLE);
  free (x);

  jm_bench_write_json (&_bench, "timing_est");
  dp_timing_est_destroy (obj);
  return 0;
}
