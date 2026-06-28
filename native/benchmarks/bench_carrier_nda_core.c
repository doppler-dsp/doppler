/* bench_carrier_nda_core.c — the NDA M-th-power carrier loop.
 *
 * Benchmarks the two ways to run the loop over a sample block:
 *   step   — the inline composition hot path a receiver inlines per sample
 *            (carrier_nda_wipeoff -> _arm_step -> _steer)
 *   steps  — the block carrier_nda_steps() (the Python face)
 *
 * Both do identical per-sample work (steps() is the same three inline calls in
 * a loop); the pair captures any block-call overhead vs the inlined path.
 */
#include "carrier_nda/carrier_nda_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200
#define TWOPI 6.283185307179586

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static void
report (const char *name, const double *times)
{
  double s = 0.0;
  for (int r = 0; r < ITERATIONS; r++)
    s += times[r];
  printf ("  %-8s %8.1f MSa/s\n", name,
          (double)BENCH_N / (s / ITERATIONS) / 1e6);
}

int
main (void)
{
  float complex *in  = malloc (BENCH_N * sizeof (*in));
  float complex *out = malloc (BENCH_N * sizeof (*out));
  if (!in || !out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  /* A rotating carrier — the per-sample loop work is data-independent. */
  for (int i = 0; i < BENCH_N; i++)
    in[i] = (float complex)cexp (I * TWOPI * 0.002 * (double)i);

  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  printf ("=== carrier_nda benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* --- step: the inline composition hot path (receiver per-sample loop). ---
   */
  {
    carrier_nda_state_t *c    = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    volatile float       sink = 0.0f;
    for (int i = 0; i < 16; i++) /* warmup */
      {
        float complex d = carrier_nda_wipeoff (c, in[i]);
        double        pe, lk;
        if (carrier_nda_arm_step (c, d, &pe, &lk))
          carrier_nda_steer (c, pe);
        sink += crealf (d);
      }
    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < BENCH_N; i++)
          {
            float complex d = carrier_nda_wipeoff (c, in[i]);
            double        pe, lk;
            if (carrier_nda_arm_step (c, d, &pe, &lk))
              carrier_nda_steer (c, pe);
            out[i] = d;
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        sink += crealf (out[BENCH_N - 1]);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "step", times, ITERATIONS, BENCH_N);
    report ("step", times);
    (void)sink;
    carrier_nda_destroy (c);
  }

  /* --- steps: the block API (Python face). --- */
  {
    carrier_nda_state_t *c = carrier_nda_create (0.01, 0.707, 0.0, 8, 4, 4);
    carrier_nda_steps (c, in, 16, out, 16); /* warmup */
    double times[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        carrier_nda_steps (c, in, BENCH_N, out, BENCH_N);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "steps", times, ITERATIONS, BENCH_N);
    report ("steps", times);
    carrier_nda_destroy (c);
  }

  jm_bench_write_json (&_bench, "carrier_nda");
  free (in);
  free (out);
  return 0;
}
