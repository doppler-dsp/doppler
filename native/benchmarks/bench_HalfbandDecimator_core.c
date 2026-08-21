/* bench_HalfbandDecimator_core.c — decimate-by-two, at the object.
 *
 * Two things at once, both from doppler#893 and #891.
 *
 * It was an unfilled jm scaffold that recorded nothing (#891). And the
 * measurement it should have carried was living one level DOWN, in
 * `bench_hbdecim_core.c`, against the `hbdecim` c_dep this object composes
 * — a benchmark at a granularity jm has no concept of, which is why nothing
 * ran it and why the component string in it had been wrong
 * (`"hbdecim_core"`, writing a JSON filename no collector opens) without
 * anyone noticing. A caller constructs a `HalfbandDecimator`, so that is
 * where the cost belongs.
 *
 * The block sweep is carried up from that file verbatim in intent: a
 * halfband decimator's per-sample cost is not flat in block size, because
 * the delay-line management and the call itself amortise differently at 1k
 * than at 800k, and a caller choosing a block size is choosing a point on
 * that curve.
 *
 * Timing is MIN over rounds, not mean, after a WARMUP_S settle.
 */
#include "HalfbandDecimator/HalfbandDecimator_core.h"
#include "jm_bench.h"
#include <complex.h>
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
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

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
  struct timespec t0, t1;
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

  /* Capped at 2 * HBDECIM_MAX_OUT: the OBJECT will not emit more than
     HBDECIM_MAX_OUT per call, so a larger block is silently truncated.
     bench_hbdecim_core.c swept to 819200 because the c_dep beneath has no
     such limit -- measuring a block size no caller can actually request,
     which is the level error doppler#893 is about. */
  const size_t blocks[4]
      = { 1024, 8192, HBDECIM_MAX_OUT, 2 * (size_t)HBDECIM_MAX_OUT };
  float complex *x   = malloc (blocks[3] * sizeof *x);
  float complex *out = malloc (blocks[3] * sizeof *out);
  if (!x || !out)
    return 1;
  for (size_t i = 0; i < blocks[3]; i++)
    {
      double p = 0.017 * (double)i;
      x[i]     = (float)cos (p) + (float)sin (p * 1.3) * I;
    }

  printf ("=== HalfbandDecimator benchmark ===\n");
  printf ("%d-tap halfband, %d samples/round, %d rounds\n\n", N_TAPS,
          TOTAL_PER_ROUND, ITERATIONS);

  static double t_ex[4][ITERATIONS];

  for (int k = 0; k < 4; k++)
    {
      const size_t block = blocks[k];
      const size_t reps  = TOTAL_PER_ROUND / block;

      HalfbandDecimator_state_t *d = HalfbandDecimator_create (h, N_TAPS);
      if (!d)
        {
          (void)fprintf (stderr,
                         "bench_HalfbandDecimator: create(%d taps) NULL\n",
                         N_TAPS);
          return 1;
        }
      size_t cap = HalfbandDecimator_execute_max_out (d);
      if (cap == 0 || cap > block)
        cap = block;

      /* Decimate-by-two must emit about half its input; a short or empty
         return would mean the rows below time a path no caller takes. */
      size_t got = HalfbandDecimator_execute (d, x, block, out, cap);
      if (got < block / 4)
        {
          (void)fprintf (stderr,
                         "bench_HalfbandDecimator: block=%zu emitted %zu "
                         "(expected ~%zu) — the timings below would measure "
                         "a short path\n",
                         block, got, block / 2);
          return 1;
        }

      struct timespec w0, w1;
      clock_gettime (CLOCK_MONOTONIC, &w0);
      do
        {
          for (size_t r = 0; r < reps; r++)
            sink += HalfbandDecimator_execute (d, x, block, out, cap);
          clock_gettime (CLOCK_MONOTONIC, &w1);
        }
      while (elapsed_sec (&w0, &w1) < WARMUP_S);

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          for (size_t j = 0; j < reps; j++)
            sink += HalfbandDecimator_execute (d, x, block, out, cap);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_ex[k][r] = elapsed_sec (&t0, &t1);
        }
      char name[64];
      (void)snprintf (name, sizeof name, "execute[block=%zu]", block);
      jm_bench_add (&_bench, name, t_ex[k], ITERATIONS, TOTAL_PER_ROUND);
      double sec = min_sec (t_ex[k], ITERATIONS);
      printf ("  %-26s %7.2f ns/sample  %8.1f MSa/s\n", name,
              sec / (double)TOTAL_PER_ROUND * 1e9,
              (double)TOTAL_PER_ROUND / sec / 1e6);
      HalfbandDecimator_destroy (d);
    }

  printf ("\n  block 1024 -> %zu costs %.2fx per sample: the same total\n"
          "  work, so the difference is per-call overhead and delay-line\n"
          "  management amortising. The sweep stops at 2 * HBDECIM_MAX_OUT\n"
          "  (%d) because the object emits at most that many samples per\n"
          "  call -- ask for more and the rest is silently not processed.\n",
          blocks[3],
          min_sec (t_ex[3], ITERATIONS) / min_sec (t_ex[0], ITERATIONS),
          HBDECIM_MAX_OUT);

  (void)sink;
  free (x);
  free (out);
  jm_bench_write_json (&_bench, "HalfbandDecimator");
  return 0;
}
