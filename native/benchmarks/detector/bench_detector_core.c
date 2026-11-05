/* bench_detector_core.c -- what the detector adds to the correlator, and
 * whether the caller's chunk size is free.
 *
 * `dp_detector_push` accepts a chunk of ANY length. It writes into a
 * double-mapped ring, drains whole n-sample frames through the correlator
 * underneath, and on each int-dump computes peak-over-noise and decides
 * whether to emit. Nothing in that signature warns that a caller feeding
 * 64 samples at a time might be paying for the privilege -- and a caller
 * bridging a hardware block size to a frame size has no choice about it.
 *
 * So the first axis is chunk size at constant total work: the same 65536
 * samples pushed as one call, as frame-sized calls, and as chunks far
 * smaller than a frame. The correlator work is identical in all three;
 * only the ring bookkeeping and the call count change.
 *
 * The second axis is what a DETECTION costs. Threshold 0.0 fires on every
 * dump; a threshold nothing can reach fires on none. Both compute the same
 * statistic -- the peak search over n bins and the noise aggregate over
 * the noise band -- so the difference is only the emit, and a large gap
 * would mean the emit path is doing more than it looks like.
 *
 * The noise mode is the third thing worth knowing and is deliberately NOT
 * swept here: "median" sorts a scratch copy of the noise band and the other
 * three are a single pass, so it is a different algorithm rather than a
 * configuration of this one. Left for whoever needs it, named here so the
 * absence is a decision rather than an oversight.
 *
 * Three short frames besides (#2752 step 3.1b): the PN periods 31, 63 and
 * 127, one frame a push, where per-call overhead is a large share of the
 * work. They are the baseline the one correlator's 1-D detector flavor must
 * meet (docs/design/correlator.md, U1). Each detects an m-sequence delayed
 * by PLANT samples, and checks after its timing that a push reports one
 * hit at that lag, or the bench fails.
 */
#include "doppler/detector/detector_core.h"
#include "doppler/dp_complex.h"
#include "dp_bench.h"
#include "dp_bench_pn.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 60
#define FRAME 1024
#define TOTAL 65536
#define MAX_RESULTS 256
#define DWELL 1

/* Chunk sizes: well under a frame, exactly a frame, and many frames. */
#define N_CFG 4
static const size_t chunk[N_CFG] = { 64, FRAME, 16384, FRAME };
/* The last configuration repeats the frame-sized chunk with a threshold
   nothing reaches, so the pair differs only in whether it emits. */
static const float thresh[N_CFG] = { 0.0f, 0.0f, 0.0f, 1e30f };
#define FIRING_FRAME_IDX 1
#define SILENT_FRAME_IDX 3

/* The short rows: register widths 5, 6 and 7, so periods 31, 63 and 127. */
#define N_SMALL 3
static const uint32_t small_bits[N_SMALL] = { 5, 6, 7 };
#define PLANT 5   /* the stream's delay, so the hit's lag */
#define REPS 64   /* frames a round, one a push */
#define GATE 3.0f /* peak over noise: an m-sequence clears it by far */

/* The short rows: REPS one-frame pushes a round, then checked. A fresh push
   of one frame must report exactly one hit, at PLANT. Returns 0, or 1 when a
   row does not detect what it was timed detecting. */
static int
bench_small (jm_bench_t *bench)
{
  static double t[N_SMALL][ITERATIONS];
  static float _Complex in[REPS * 127];
  float _Complex ref[127];
  det_result_t hit[2];
  char         name[72];
  for (int s = 0; s < N_SMALL; s++)
    {
      const size_t n = dp_bench_pn_nrz (small_bits[s], ref, 127);
      if (n == 0)
        return 1;
      for (size_t i = 0; i < REPS * n; i++)
        in[i] = ref[(i % n + n - PLANT) % n];
      dp_detector_state_t *d
          = dp_detector_create (ref, n, 1, 1, n - 1, DET_NOISE_MEAN, GATE, 1);
      if (!d)
        return 1;
      for (int r = 0; r < ITERATIONS; r++)
        {
          dp_detector_reset (d);
          const uint64_t t0 = jm_bench_now_ns ();
          for (int q = 0; q < REPS; q++)
            (void)dp_detector_push (d, in + (size_t)q * n, n, hit, 2);
          const uint64_t t1 = jm_bench_now_ns ();
          t[s][r]           = jm_bench_elapsed_sec (t0, t1);
        }
      dp_detector_reset (d);
      const size_t nh = dp_detector_push (d, in, n, hit, 2);
      dp_detector_destroy (d);
      if (nh != 1 || hit[0].lag != PLANT)
        {
          (void)fprintf (stderr,
                         "bench_detector: frame=%zu gave %zu hit(s), lag "
                         "%zu, not one at %d\n",
                         n, nh, nh ? hit[0].lag : 0, PLANT);
          return 1;
        }
      (void)snprintf (name, sizeof name, "push[frame=%zu]", n);
      dp_bench_record (bench, name, t[s], ITERATIONS, REPS * n, "sample");
    }
  return 0;
}

int
main (void)
{
  jm_bench_t           _bench = { 0 };
  uint64_t             t0, t1;
  static double        t[N_CFG][ITERATIONS];
  dp_detector_state_t *det[N_CFG] = { 0 };
  float _Complex      *ref = NULL, *in = NULL;
  det_result_t        *res = NULL;
  char                 name[72];

  ref = malloc (FRAME * sizeof *ref);
  in  = malloc (TOTAL * sizeof *in);
  res = malloc (MAX_RESULTS * sizeof *res);
  if (!ref || !in || !res)
    return 1;

  /* A chirp reference and the same chirp in the stream: a real peak, so
     the peak search and the noise aggregate both see representative data
     rather than a flat correlation surface. */
  for (size_t i = 0; i < FRAME; i++)
    {
      const double p = 1e-4 * (double)i * (double)i;
      ref[i]         = (float _Complex) (cos (p) + sin (p) * I);
    }
  for (size_t i = 0; i < TOTAL; i++)
    {
      const double p = 1e-4 * (double)(i % FRAME) * (double)(i % FRAME);
      in[i]          = (float _Complex) (cos (p) + sin (p) * I);
    }

  for (int c = 0; c < N_CFG; c++)
    {
      det[c] = dp_detector_create (ref, FRAME, DWELL, 1, FRAME - 1,
                                   DET_NOISE_MEAN, thresh[c], 1);
      if (!det[c])
        return 1;
    }

  printf ("=== detector (corr + ring + peak/noise gate) ===\n");
  printf ("frame = %d, %d samples per round, %d rounds, min over rounds\n\n",
          FRAME, TOTAL, ITERATIONS);

  DP_BENCH_SETTLE (
      (void)dp_detector_push (det[0], in, FRAME, res, MAX_RESULTS));

  /* Rounds outside, chunk sizes inside: three of these rows are read as
     multiples of a fourth, so one thermal step must not land on one. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        dp_detector_reset (det[c]);
        t0 = jm_bench_now_ns ();
        for (size_t off = 0; off < TOTAL; off += chunk[c])
          (void)dp_detector_push (det[c], in + off, chunk[c], res,
                                  MAX_RESULTS);
        t1      = jm_bench_now_ns ();
        t[c][r] = jm_bench_elapsed_sec (t0, t1);
      }

  for (int c = 0; c < N_CFG; c++)
    {
      (void)snprintf (name, sizeof name, "push[chunk=%zu%s]", chunk[c],
                      thresh[c] > 0.0f ? ",no-detections" : "");
      dp_bench_record (&_bench, name, t[c], ITERATIONS, TOTAL, "sample");
    }

  printf ("\n  chunk size at constant work (%d-sample chunks = 1.00x):\n",
          FRAME);
  for (int c = 0; c < N_CFG; c++)
    {
      if (c == SILENT_FRAME_IDX)
        continue;
      printf ("    %6zu samples per push   %.2fx   (%zu calls)\n", chunk[c],
              dp_bench_min (t[c], ITERATIONS)
                  / dp_bench_min (t[FIRING_FRAME_IDX], ITERATIONS),
              (size_t)TOTAL / chunk[c]);
    }
  printf ("\n  emitting a detection on every dump costs %.2fx over\n"
          "  emitting none, at the same chunk size and the same\n"
          "  statistic. Both compute peak-over-noise; only one writes.\n",
          dp_bench_min (t[FIRING_FRAME_IDX], ITERATIONS)
              / dp_bench_min (t[SILENT_FRAME_IDX], ITERATIONS));

  for (int c = 0; c < N_CFG; c++)
    dp_detector_destroy (det[c]);
  free (ref);
  free (in);
  free (res);
  if (bench_small (&_bench) != 0)
    return 1;
  jm_bench_write_json (&_bench, "detector");
  return 0;
}
