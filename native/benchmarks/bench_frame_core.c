/* bench_frame_core.c — materialising a frame, and checking one.
 *
 * This file was a jm scaffold until now: a `TODO: benchmark this component`
 * and no `jm_bench_add` call, so it built, ran, and wrote
 * `"benchmarks": []`. `frame` IS one of jm's components, so `jm bench` was
 * running it faithfully every time and collecting nothing -- which is why
 * scripts/check_bench_coverage.py now fails a benchmark that records no
 * measurement, and not only a component that has no benchmark file.
 *
 * What a caller pays:
 *
 *   bits[1]        materialise one frame's bits -- preamble repeats, the
 *                  sync word, the payload, the CRC
 *   bits[16]       sixteen frames in one call, which is how a waveform
 *                  generator asks. Divided by 16 in the report, so the two
 *                  rows are directly comparable and the difference is the
 *                  per-call overhead a caller avoids by batching
 *   crc_ok         the receive-side check, per frame
 *
 * The frame here is a 1024-bit payload behind a 64-bit sync word with a
 * 32-bit preamble repeated four times and a CRC -- a plausible small
 * telemetry frame rather than a degenerate one, because a frame with an
 * empty payload would measure the call and not the copy.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "frame/frame_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N_PRE 32
#define PRE_REPS 4
#define N_SYNC 64
#define N_PAY 1024
#define BATCH 16
#define ITERATIONS 200

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

  static uint8_t pre[N_PRE], sync[N_SYNC], pay[N_PAY];
  uint32_t       lfsr = 0x51F0u;
  for (size_t i = 0; i < N_PRE; i++)
    pre[i] = (uint8_t)(i & 1u);
  for (size_t i = 0; i < N_SYNC; i++)
    sync[i] = (uint8_t)((0x1ACFFC1Du >> (i % 32)) & 1u);
  for (size_t i = 0; i < N_PAY; i++)
    {
      lfsr   = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      pay[i] = (uint8_t)(lfsr & 1u);
    }

  /* Every generator argument zeroed: the literal case, which is what a
     caller with real data has. Same shape as test_frame_core.c's
     `lit_frame`, deliberately -- the benchmark should measure the frame the
     tests pin. */
  frame_state_t *f = frame_create (
      0, pre, N_PRE, 0, PRE_REPS, 0, 0, 0, 0, 0, 0, 0, 0, 0, sync, N_SYNC, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, pay, N_PAY, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
  if (!f)
    return 1;

  const size_t nb1 = frame_bits_max_out (f, 1);
  const size_t nbB = frame_bits_max_out (f, BATCH);
  uint8_t     *out = malloc (nbB);
  if (!out)
    return 1;

  printf ("=== frame benchmark ===\n");
  printf ("frame = %zu bits (%d-bit preamble x%d, %d-bit sync, %d-bit "
          "payload, CRC), %d rounds\n\n",
          nb1, N_PRE, PRE_REPS, N_SYNC, N_PAY, ITERATIONS);

  static double t_one[ITERATIONS], t_bat[ITERATIONS], t_crc[ITERATIONS];

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += frame_bits (f, 1, out, nb1);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_one[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "bits[1]", t_one, ITERATIONS, 1);
  printf ("  %-14s %9.3f us/frame  %8.2f Mbit/s\n", "bits[1]",
          min_sec (t_one, ITERATIONS) * 1e6,
          (double)nb1 / min_sec (t_one, ITERATIONS) / 1e6);

  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += frame_bits (f, BATCH, out, nbB);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_bat[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "bits[16]", t_bat, ITERATIONS, BATCH);
  printf ("  %-14s %9.3f us/frame  %8.2f Mbit/s\n", "bits[16]",
          min_sec (t_bat, ITERATIONS) / BATCH * 1e6,
          (double)nbB / min_sec (t_bat, ITERATIONS) / 1e6);

  frame_bits (f, 1, out, nb1);
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      sink += (size_t)frame_crc_ok (f, out, nb1);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_crc[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "crc_ok", t_crc, ITERATIONS, 1);
  printf ("  %-14s %9.3f us/frame\n", "crc_ok",
          min_sec (t_crc, ITERATIONS) * 1e6);

  printf ("\n  batching %d frames costs %.2f us/frame against %.2f us for\n"
          "  one -- the gap is per-call overhead, and it is what a generator\n"
          "  asking frame by frame pays for the convenience.\n",
          BATCH, min_sec (t_bat, ITERATIONS) / BATCH * 1e6,
          min_sec (t_one, ITERATIONS) * 1e6);

  (void)sink;
  free (out);
  frame_destroy (f);
  jm_bench_write_json (&_bench, "frame");
  return 0;
}
