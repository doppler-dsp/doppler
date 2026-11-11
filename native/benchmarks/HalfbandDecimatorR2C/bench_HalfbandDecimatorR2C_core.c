/* bench_HalfbandDecimatorR2C_core.c — real-input decimate-by-two, at the
 * object.
 *
 * The real-input sibling of bench_HalfbandDecimator_core.c, swept over the
 * same block sizes so the two compare row for row: same total work per
 * round, MIN over rounds after a WARMUP_S settle. A caller constructs a
 * HalfbandDecimatorR2C, so that is where the cost is measured (#3002).
 */
#include "doppler/HalfbandDecimatorR2C/HalfbandDecimatorR2C_core.h"
#include "doppler/dp_complex.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Same total work at every block size, so the rows compare directly. */
#define TOTAL_PER_ROUND 2048000
#define ITERATIONS 20
#define WARMUP_S 0.25
#define N_TAPS 19

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

int
main (void)
{
  uint64_t        t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  /* A real halfband: odd length, zero even taps away from centre. */
  static float h[N_TAPS];
  for (int i = 0; i < N_TAPS; i++)
    {
      int k = i - N_TAPS / 2;
      if (k == 0)
        h[i] = 0.5f;
      else if (k % 2 == 0)
        h[i] = 0.0f;
      else
        h[i] = (float)(sin (M_PI * k / 2.0) / (M_PI * k)
                       * (0.54 + 0.46 * cos (M_PI * k / (N_TAPS / 2.0))));
    }

  const size_t    blocks[4] = { 1024, 8192, 32768, 65536 };
  float          *x         = malloc (blocks[3] * sizeof *x);
  float _Complex *out       = malloc (blocks[3] * sizeof *out);
  if (!x || !out)
    return 1;
  for (size_t i = 0; i < blocks[3]; i++)
    x[i] = (float)cos (0.017 * (double)i) + (float)sin (0.022 * (double)i);

  printf ("=== HalfbandDecimatorR2C benchmark ===\n");
  printf ("%d-tap halfband, %d samples/round, %d rounds\n\n", N_TAPS,
          TOTAL_PER_ROUND, ITERATIONS);

  static double t_ex[4][ITERATIONS];

  for (int k = 0; k < 4; k++)
    {
      const size_t block = blocks[k];
      const size_t reps  = TOTAL_PER_ROUND / block;

      dp_HalfbandDecimatorR2C_state_t *d
          = dp_HalfbandDecimatorR2C_create (h, N_TAPS);
      if (!d)
        {
          (void)fprintf (stderr,
                         "bench_HalfbandDecimatorR2C: create(%d taps) NULL\n",
                         N_TAPS);
          return 1;
        }
      size_t cap = dp_HalfbandDecimatorR2C_execute_max_out (d, block);

      /* Decimate-by-two must emit about half its input; a short or empty
         return would mean the rows below time a path no caller takes. */
      size_t got = dp_HalfbandDecimatorR2C_execute (d, x, block, out, cap);
      if (got < block / 4)
        {
          (void)fprintf (stderr,
                         "bench_HalfbandDecimatorR2C: block=%zu emitted %zu "
                         "(expected ~%zu) — the timings below would measure "
                         "a short path\n",
                         block, got, block / 2);
          return 1;
        }

      uint64_t w0, w1;
      w0 = jm_bench_now_ns ();
      do
        {
          for (size_t r = 0; r < reps; r++)
            sink += dp_HalfbandDecimatorR2C_execute (d, x, block, out, cap);
          w1 = jm_bench_now_ns ();
        }
      while (jm_bench_elapsed_sec (w0, w1) < WARMUP_S);

      for (int r = 0; r < ITERATIONS; r++)
        {
          t0 = jm_bench_now_ns ();
          for (size_t j = 0; j < reps; j++)
            sink += dp_HalfbandDecimatorR2C_execute (d, x, block, out, cap);
          t1         = jm_bench_now_ns ();
          t_ex[k][r] = jm_bench_elapsed_sec (t0, t1);
        }
      char name[64];
      (void)snprintf (name, sizeof name, "execute[block=%zu]", block);
      jm_bench_add_unit (&_bench, name, t_ex[k], ITERATIONS, TOTAL_PER_ROUND,
                         JM_BENCH_SAMPLE);
      double sec = min_sec (t_ex[k], ITERATIONS);
      printf ("  %-26s %7.2f ns/sample  %8.1f MSa/s\n", name,
              sec / (double)TOTAL_PER_ROUND * 1e9,
              (double)TOTAL_PER_ROUND / sec / 1e6);
      dp_HalfbandDecimatorR2C_destroy (d);
    }

  (void)sink;
  free (x);
  free (out);
  jm_bench_write_json (&_bench, "HalfbandDecimatorR2C");
  return 0;
}
