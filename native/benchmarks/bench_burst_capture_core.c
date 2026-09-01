/**
 * bench_burst_capture_core.c — what a capture costs per sample.
 *
 * `push()` is the only hot path, and it does three separable things: it
 * writes the ring, it drives the acquisition child, and it refines and copies
 * out a window when one completes. The two cases are measured separately
 * because they answer different questions — the SEARCH cost is what a
 * receiver pays on every sample of a quiet stream, and the BURST cost is what
 * a hit adds on top.
 *
 * The persistent flavour is measured against the anonymous one on the same
 * stream, because "does backing the ring with a file cost anything" is the
 * question the feature has to answer and a claim nobody times is prose.
 */
#include "burst_capture/burst_capture_core.h"
#include "jm_bench.h"
#include "pn/pn_core.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ACQ_SF 31u
#define DATA_SF 8u
#define REPS 4u
#define SPC 4u
#define PAYLOAD_SYMS 61u
#define BURST_LEN ((REPS * ACQ_SF + PAYLOAD_SYMS * DATA_SF) * SPC)
#define BENCH_N 65536
#define ITERATIONS 30

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static float
csign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

static uint8_t code[ACQ_SF];
static uint8_t dcode[DATA_SF];

static void
build_codes (void)
{
  pn_state_t *pn = pn_create (pn_mls_poly (5), 1u, 5u, 0);
  for (size_t i = 0; i < ACQ_SF; i++)
    code[i] = pn ? pn_step (pn) : (uint8_t)(i & 1u);
  if (pn)
    pn_destroy (pn);
  for (size_t i = 0; i < DATA_SF; i++)
    dcode[i] = (uint8_t)((i >> 1) & 1u);
}

static float complex x_quiet[BENCH_N];
static float complex x_bursts[BENCH_N];
static float complex out[16 * BURST_LEN];

/** @brief Noise, plus @p n_at bursts spread evenly through the block. */
static void
build_stream (float complex *y, size_t n, size_t n_bursts)
{
  uint32_t st = 12345u;
  for (size_t i = 0; i < n; i++)
    {
      /* A cheap deterministic LCG: this is a benchmark, so what matters is
         that both flavours see the SAME samples, not the distribution. */
      st       = st * 1664525u + 1013904223u;
      float re = (float)((int32_t)(st >> 8) % 1000) * 2.0e-5f;
      st       = st * 1664525u + 1013904223u;
      float im = (float)((int32_t)(st >> 8) % 1000) * 2.0e-5f;
      y[i]     = re + im * I;
    }
  if (!n_bursts)
    return;
  size_t stride = n / n_bursts;
  for (size_t k = 0; k < n_bursts; k++)
    {
      size_t at = k * stride;
      size_t j  = at;
      for (size_t r = 0; r < REPS && j + 1 < n; r++)
        for (size_t c = 0; c < ACQ_SF; c++)
          for (size_t s = 0; s < SPC && j < n; s++)
            y[j++] += csign (code[c]);
      for (size_t m = 0; m < PAYLOAD_SYMS && j < n; m++)
        {
          float a = csign ((uint8_t)(m & 1u));
          for (size_t c = 0; c < DATA_SF; c++)
            for (size_t s = 0; s < SPC && j < n; s++)
              y[j++] += a * csign (dcode[c]);
        }
    }
}

/** @brief Time ITERATIONS pushes of @p x through a fresh capture each round.
 */
static void
time_push (jm_bench_t *b, const char *name, const char *path,
           const float complex *x, double *times)
{
  struct timespec t0, t1;
  for (int r = 0; r < ITERATIONS; r++)
    {
      burst_capture_state_t *c
          = path ? burst_capture_create_backed (path, code, ACQ_SF, BURST_LEN,
                                                REPS, SPC, 1.0e6, 55.0, 0.0,
                                                1e-3, 0.9, 0)
                 : burst_capture_create (code, ACQ_SF, BURST_LEN, REPS, SPC,
                                         1.0e6, 55.0, 0.0, 1e-3, 0.9, 0);
      if (!c)
        return;
      clock_gettime (CLOCK_MONOTONIC, &t0);
      (void)burst_capture_push (c, x, BENCH_N, out, sizeof out / sizeof *out);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
      burst_capture_destroy (c);
    }
  jm_bench_add (b, name, times, ITERATIONS, BENCH_N);
}

int
main (void)
{
  build_codes ();
  build_stream (x_quiet, BENCH_N, 0);
  build_stream (x_bursts, BENCH_N, 4);

  jm_bench_t _bench = { 0 };
  printf ("=== burst_capture benchmark ===\n");
  printf ("block = %d samples, %d iterations, burst_len = %u\n\n", BENCH_N,
          ITERATIONS, (unsigned)BURST_LEN);

  static double times[ITERATIONS];
  time_push (&_bench, "push[quiet]", NULL, x_quiet, times);
  time_push (&_bench, "push[4 bursts]", NULL, x_bursts, times);

  /* The persistent flavour on the SAME stream: the comparison is the point,
     so the two rows are directly subtractable. */
  char path[256];
  (void)snprintf (path, sizeof path, "/tmp/dp_bench_burst_capture_%d.cf32",
                  (int)getpid ());
  remove (path);
  time_push (&_bench, "push[4 bursts, file-backed]", path, x_bursts, times);
  remove (path);

  jm_bench_write_json (&_bench, "burst_capture");
  return 0;
}
