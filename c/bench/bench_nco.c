/**
 * @file bench_nco.c
 * @brief Throughput benchmark for all dp_nco_execute_* variants.
 *
 * Reports MSamples/s for:
 *   cf32       — free-running, LUT → CF32 output
 *   cf32_ctrl  — FM ctrl port,  LUT → CF32 output
 *   u32        — free-running, raw uint32 phase output
 *   u32_ctrl   — FM ctrl port,  raw uint32 phase output
 *   u32_ovf    — free-running, raw uint32 + carry bit
 *   u32_ovf_ctrl — FM ctrl port, raw uint32 + carry bit
 *
 * Run with: ./bench_nco_c
 */

#include <dp/nco.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BLOCK_SIZE 1048576  /* 1 M samples per iteration             */
#define ITERATIONS 200      /* 200 M samples total per variant        */

/* ------------------------------------------------------------------ */

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
bench_msa (double iters, double block, double sec)
{
  return iters * block / sec / 1e6;
}

/* ------------------------------------------------------------------ */

static double
run_cf32 (dp_nco_t *nco, dp_cf32_t *out)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_nco_execute_cf32 (nco, out, BLOCK_SIZE);
  clock_gettime (CLOCK_MONOTONIC, &t1);
  return bench_msa (ITERATIONS, BLOCK_SIZE, elapsed_sec (&t0, &t1));
}

static double
run_cf32_ctrl (dp_nco_t *nco, const float *ctrl, dp_cf32_t *out)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_nco_execute_cf32_ctrl (nco, ctrl, out, BLOCK_SIZE);
  clock_gettime (CLOCK_MONOTONIC, &t1);
  return bench_msa (ITERATIONS, BLOCK_SIZE, elapsed_sec (&t0, &t1));
}

static double
run_u32 (dp_nco_t *nco, uint32_t *out)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_nco_execute_u32 (nco, out, BLOCK_SIZE);
  clock_gettime (CLOCK_MONOTONIC, &t1);
  return bench_msa (ITERATIONS, BLOCK_SIZE, elapsed_sec (&t0, &t1));
}

static double
run_u32_ctrl (dp_nco_t *nco, const float *ctrl, uint32_t *out)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_nco_execute_u32_ctrl (nco, ctrl, out, BLOCK_SIZE);
  clock_gettime (CLOCK_MONOTONIC, &t1);
  return bench_msa (ITERATIONS, BLOCK_SIZE, elapsed_sec (&t0, &t1));
}

static double
run_u32_ovf (dp_nco_t *nco, uint32_t *out, uint8_t *carry)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_nco_execute_u32_ovf (nco, out, carry, BLOCK_SIZE);
  clock_gettime (CLOCK_MONOTONIC, &t1);
  return bench_msa (ITERATIONS, BLOCK_SIZE, elapsed_sec (&t0, &t1));
}

static double
run_u32_ovf_ctrl (dp_nco_t *nco, const float *ctrl, uint32_t *out,
                  uint8_t *carry)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_nco_execute_u32_ovf_ctrl (nco, ctrl, out, carry, BLOCK_SIZE);
  clock_gettime (CLOCK_MONOTONIC, &t1);
  return bench_msa (ITERATIONS, BLOCK_SIZE, elapsed_sec (&t0, &t1));
}

/* ------------------------------------------------------------------ */

int
main (void)
{
  printf ("=== doppler NCO benchmark ===\n");
  printf ("  block=%d  iters=%d  (%.0f M samples/variant)\n\n",
          BLOCK_SIZE, ITERATIONS,
          (double)BLOCK_SIZE * ITERATIONS / 1e6);

  /* Allocate output buffers */
  dp_cf32_t *cf32_out = malloc (BLOCK_SIZE * sizeof *cf32_out);
  uint32_t  *u32_out  = malloc (BLOCK_SIZE * sizeof *u32_out);
  uint8_t   *carry    = malloc (BLOCK_SIZE * sizeof *carry);

  /* Ctrl signal: low-deviation FM sine wave */
  float *ctrl = malloc (BLOCK_SIZE * sizeof *ctrl);
  for (int i = 0; i < BLOCK_SIZE; i++)
    ctrl[i] = 0.002f
              * (float)sin (2.0 * M_PI * 0.01 * i);

  if (!cf32_out || !u32_out || !carry || !ctrl)
    {
      fprintf (stderr, "allocation failed\n");
      return 1;
    }

  dp_nco_t *nco = dp_nco_create (0.1f);

  printf ("  %-18s  %10s\n", "variant", "MSa/s");
  printf ("  %-18s  %10s\n", "------------------",
          "----------");

  double msa;

  dp_nco_reset (nco);
  msa = run_cf32 (nco, cf32_out);
  printf ("  %-18s  %10.1f\n", "cf32", msa);

  dp_nco_reset (nco);
  msa = run_cf32_ctrl (nco, ctrl, cf32_out);
  printf ("  %-18s  %10.1f\n", "cf32_ctrl", msa);

  dp_nco_reset (nco);
  msa = run_u32 (nco, u32_out);
  printf ("  %-18s  %10.1f\n", "u32", msa);

  dp_nco_reset (nco);
  msa = run_u32_ctrl (nco, ctrl, u32_out);
  printf ("  %-18s  %10.1f\n", "u32_ctrl", msa);

  dp_nco_reset (nco);
  msa = run_u32_ovf (nco, u32_out, carry);
  printf ("  %-18s  %10.1f\n", "u32_ovf", msa);

  dp_nco_reset (nco);
  msa = run_u32_ovf_ctrl (nco, ctrl, u32_out, carry);
  printf ("  %-18s  %10.1f\n", "u32_ovf_ctrl", msa);

  printf ("\n");

  dp_nco_destroy (nco);
  free (cf32_out);
  free (u32_out);
  free (carry);
  free (ctrl);
  return 0;
}
