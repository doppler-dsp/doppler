/**
 * @file bench_accumulator.c
 * @brief Throughput benchmark for dp_acc_f32_* and dp_acc_cf64_* hot loops.
 *
 * Reports throughput in MSamples/s and GFlops/s (1 FMA = 2 flops) for:
 *   - add   (1D array accumulation)
 *   - madd  (1D multiply-accumulate)
 *   - add2d (2D array accumulation, polyphase bank shape)
 *   - madd2d (2D MAC, polyphase bank shape)
 *
 * Run with: ./bench_accumulator_c
 */

#include <dp/accumulator.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ── Parameters ──────────────────────────────────────────────────────────── */

/* Polyphase bank dimensions (matches resampler defaults) */
#define NUM_PHASES  4096
#define NUM_TAPS    19

/* 1-D block: enough for cache-pressure to show                              */
#define BLOCK       (NUM_PHASES * NUM_TAPS)   /* 77 824 elements */

#define ITERATIONS  500

/* ── Timing ──────────────────────────────────────────────────────────────── */

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

/* ── f32 benchmarks ──────────────────────────────────────────────────────── */

static void
bench_f32_add (const float *x)
{
  dp_acc_f32_t *acc = dp_acc_f32_create ();
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < ITERATIONS; i++)
    dp_acc_f32_add (acc, x, BLOCK);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * BLOCK / sec / 1e6;
  double gfl = msa / 1e3;   /* 1 add per element */
  printf ("  f32   add    %9.1f MSa/s  %6.2f GFlop/s  (sum=%.3e)\n",
          msa, gfl, (double)dp_acc_f32_get (acc));
  dp_acc_f32_destroy (acc);
}

static void
bench_f32_madd (const float *x, const float *h)
{
  dp_acc_f32_t *acc = dp_acc_f32_create ();
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < ITERATIONS; i++)
    dp_acc_f32_madd (acc, x, h, BLOCK);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * BLOCK / sec / 1e6;
  double gfl = msa * 2.0 / 1e3;
  printf ("  f32   madd   %9.1f MSa/s  %6.2f GFlop/s  (sum=%.3e)\n",
          msa, gfl, (double)dp_acc_f32_get (acc));
  dp_acc_f32_destroy (acc);
}

static void
bench_f32_add2d (const float *x)
{
  dp_acc_f32_t *acc = dp_acc_f32_create ();
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < ITERATIONS; i++)
    dp_acc_f32_add2d (acc, x, NUM_PHASES, NUM_TAPS);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * NUM_PHASES * NUM_TAPS / sec / 1e6;
  double gfl = msa / 1e3;
  printf ("  f32   add2d  %9.1f MSa/s  %6.2f GFlop/s"
          "  [%d×%d]  (sum=%.3e)\n",
          msa, gfl, NUM_PHASES, NUM_TAPS,
          (double)dp_acc_f32_get (acc));
  dp_acc_f32_destroy (acc);
}

static void
bench_f32_madd2d (const float *x, const float *h)
{
  dp_acc_f32_t *acc = dp_acc_f32_create ();
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < ITERATIONS; i++)
    dp_acc_f32_madd2d (acc, x, h, NUM_PHASES, NUM_TAPS);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * NUM_PHASES * NUM_TAPS / sec / 1e6;
  double gfl = msa * 2.0 / 1e3;
  printf ("  f32   madd2d %9.1f MSa/s  %6.2f GFlop/s"
          "  [%d×%d]  (sum=%.3e)\n",
          msa, gfl, NUM_PHASES, NUM_TAPS,
          (double)dp_acc_f32_get (acc));
  dp_acc_f32_destroy (acc);
}

/* ── cf64 benchmarks ─────────────────────────────────────────────────────── */

static void
bench_cf64_add (const dp_cf64_t *x)
{
  dp_acc_cf64_t *acc = dp_acc_cf64_create ();
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < ITERATIONS; i++)
    dp_acc_cf64_add (acc, x, BLOCK);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * BLOCK / sec / 1e6;
  double gfl = msa * 2.0 / 1e3;   /* 2 adds per complex sample */
  dp_cf64_t v = dp_acc_cf64_get (acc);
  printf ("  cf64  add    %9.1f MSa/s  %6.2f GFlop/s  (sum=%.3e+%.3ej)\n",
          msa, gfl, v.i, v.q);
  dp_acc_cf64_destroy (acc);
}

static void
bench_cf64_madd (const dp_cf64_t *x, const float *h)
{
  dp_acc_cf64_t *acc = dp_acc_cf64_create ();
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < ITERATIONS; i++)
    dp_acc_cf64_madd (acc, x, h, BLOCK);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * BLOCK / sec / 1e6;
  double gfl = msa * 4.0 / 1e3;
  dp_cf64_t v = dp_acc_cf64_get (acc);
  printf ("  cf64  madd   %9.1f MSa/s  %6.2f GFlop/s  (sum=%.3e+%.3ej)\n",
          msa, gfl, v.i, v.q);
  dp_acc_cf64_destroy (acc);
}

static void
bench_cf64_madd2d (const dp_cf64_t *x, const float *h)
{
  dp_acc_cf64_t *acc = dp_acc_cf64_create ();
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int i = 0; i < ITERATIONS; i++)
    dp_acc_cf64_madd2d (acc, x, h, NUM_PHASES, NUM_TAPS);

  clock_gettime (CLOCK_MONOTONIC, &t1);
  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * NUM_PHASES * NUM_TAPS / sec / 1e6;
  double gfl = msa * 4.0 / 1e3;
  dp_cf64_t v = dp_acc_cf64_get (acc);
  printf ("  cf64  madd2d %9.1f MSa/s  %6.2f GFlop/s"
          "  [%d×%d]  (sum=%.3e+%.3ej)\n",
          msa, gfl, NUM_PHASES, NUM_TAPS, v.i, v.q);
  dp_acc_cf64_destroy (acc);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int
main (void)
{
  /* Allocate and fill input buffers */
  float      *xf = (float *)     malloc (BLOCK * sizeof (float));
  float      *hf = (float *)     malloc (BLOCK * sizeof (float));
  dp_cf64_t  *xc = (dp_cf64_t *) malloc (BLOCK * sizeof (dp_cf64_t));

  if (!xf || !hf || !xc)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  for (int i = 0; i < BLOCK; i++)
    {
      xf[i] = (float)i * 1e-6f;
      hf[i] = (float)(i + 1) * 1e-6f;
      xc[i].i = (double)i * 1e-6;
      xc[i].q = (double)(i + 1) * 1e-6;
    }

  printf ("=== accumulator benchmark ===\n");
  printf ("block = %d elements,  %d iterations\n\n", BLOCK, ITERATIONS);

  printf ("── f32 ───────────────────────────────────────────────────\n");
  bench_f32_add   (xf);
  bench_f32_madd  (xf, hf);
  bench_f32_add2d (xf);
  bench_f32_madd2d (xf, hf);

  printf ("\n── cf64 ──────────────────────────────────────────────────\n");
  bench_cf64_add   (xc);
  bench_cf64_madd  (xc, hf);
  bench_cf64_madd2d (xc, hf);

  printf ("\n");

  free (xf);
  free (hf);
  free (xc);
  return 0;
}
