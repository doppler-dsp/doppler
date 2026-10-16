/**
 * bench_burst_capture_core.c — what a capture costs per sample.
 *
 * `push()` is the only hot path: it writes the ring, drives the
 * acquisition child, and refines and copies out a window when one
 * completes. The row times all three on a block holding four bursts, and
 * every timed push must hand back exactly four windows, or the row would
 * time a capture that found nothing.
 *
 * One row (doppler#2386): a quiet stream and a file-backed ring read the
 * same per sample as this one, so they are not separate rows.
 */
#include "doppler/burst_capture/burst_capture_core.h"
#include "doppler/cvt/cvt_core.h"
#include "doppler/pn/pn_core.h"
#include "jm_bench.h"

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ACQ_SF 31u
#define DATA_SF 8u
#define REPS 4u
#define SPC 4u
#define PAYLOAD_SYMS 61u
#define BURST_LEN ((REPS * ACQ_SF + PAYLOAD_SYMS * DATA_SF) * SPC)
#define BENCH_N 65536
#define ITERATIONS 30
#define N_BURSTS 4u

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
  dp_pn_state_t *pn = dp_pn_create (pn_mls_poly (5), 1u, 5u, 0);
  for (size_t i = 0; i < ACQ_SF; i++)
    code[i] = pn ? pn_step (pn) : (uint8_t)(i & 1u);
  if (pn)
    dp_pn_destroy (pn);
  for (size_t i = 0; i < DATA_SF; i++)
    dcode[i] = (uint8_t)((i >> 1) & 1u);
}

static float _Complex x_bursts[BENCH_N];
static float _Complex out[16 * BURST_LEN];

/** @brief Noise, plus @p n_at bursts spread evenly through the block. */
static void
build_stream (float _Complex *y, size_t n, size_t n_bursts)
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

/** @brief Time ITERATIONS pushes of @p x through a fresh capture each round;
 *         nonzero when a capture fails to open or a push does not hand back
 *         @p windows windows.
 */
static int
time_push (jm_bench_t *b, const char *name, const float _Complex *x,
           size_t windows, double *times)
{
  uint64_t t0, t1;
  for (int r = 0; r < ITERATIONS; r++)
    {
      /* The preamble is the code's samples: bin_to_nrz, held SPC. */
      static float nrz[ACQ_SF];
      static float _Complex pre[ACQ_SF * SPC];
      (void)dp_bin_to_nrz (code, ACQ_SF, nrz, ACQ_SF);
      for (size_t i = 0; i < ACQ_SF * SPC; i++)
        pre[i] = nrz[i / SPC];
      const double              fs = 1.0e6 * SPC;
      dp_burst_capture_state_t *c
          = dp_burst_capture_create (pre, ACQ_SF * SPC, BURST_LEN, REPS, fs,
                                     55.0, 0.0, 1e-3, 0.9, 0, 0.0, 1);
      if (!c)
        {
          (void)fprintf (stderr, "bench_burst_capture: create NULL\n");
          return 1;
        }
      t0         = jm_bench_now_ns ();
      size_t got = dp_burst_capture_push (c, x, BENCH_N, out,
                                          sizeof out / sizeof *out);
      t1         = jm_bench_now_ns ();
      times[r]   = jm_bench_elapsed_sec (t0, t1);
      dp_burst_capture_destroy (c);
      if (got != windows * BURST_LEN)
        {
          (void)fprintf (stderr,
                         "bench_burst_capture: %s handed back %zu samples, "
                         "not %zu windows of %u -- the row would time a "
                         "capture that missed its bursts\n",
                         name, got, windows, (unsigned)BURST_LEN);
          return 1;
        }
    }
  jm_bench_add_unit (b, name, times, ITERATIONS, BENCH_N, JM_BENCH_SAMPLE);
  return 0;
}

int
main (void)
{
  build_codes ();
  build_stream (x_bursts, BENCH_N, N_BURSTS);

  jm_bench_t _bench = { 0 };
  printf ("=== burst_capture benchmark ===\n");
  printf ("block = %d samples, %d iterations, burst_len = %u\n\n", BENCH_N,
          ITERATIONS, (unsigned)BURST_LEN);

  static double times[ITERATIONS];
  if (time_push (&_bench, "push[4 bursts]", x_bursts, N_BURSTS, times) != 0)
    return 1;

  jm_bench_write_json (&_bench, "burst_capture");
  return 0;
}
