/* bench_detector_core.c -- what the detector adds to the correlator, and
 * whether the caller's chunk size is free.
 *
 * `detector_push` accepts a chunk of ANY length. It writes into a
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
 */
#include "detector/detector_core.h"
#include "dp_bench.h"
#include <complex.h>
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

int
main (void)
{
  jm_bench_t        _bench = { 0 };
  struct timespec   t0, t1;
  static double     t[N_CFG][ITERATIONS];
  detector_state_t *det[N_CFG] = { 0 };
  float complex    *ref = NULL, *in = NULL;
  det_result_t     *res = NULL;
  char              name[72];

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
      ref[i]         = (float complex) (cos (p) + sin (p) * I);
    }
  for (size_t i = 0; i < TOTAL; i++)
    {
      const double p = 1e-4 * (double)(i % FRAME) * (double)(i % FRAME);
      in[i]          = (float complex) (cos (p) + sin (p) * I);
    }

  for (int c = 0; c < N_CFG; c++)
    {
      det[c] = detector_create (ref, FRAME, DWELL, 1, FRAME - 1,
                                DET_NOISE_MEAN, thresh[c], 1);
      if (!det[c])
        return 1;
    }

  printf ("=== detector (corr + ring + peak/noise gate) ===\n");
  printf ("frame = %d, %d samples per round, %d rounds, min over rounds\n\n",
          FRAME, TOTAL, ITERATIONS);

  DP_BENCH_SETTLE ((void)detector_push (det[0], in, FRAME, res, MAX_RESULTS));

  /* Rounds outside, chunk sizes inside: three of these rows are read as
     multiples of a fourth, so one thermal step must not land on one. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        detector_reset (det[c]);
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (size_t off = 0; off < TOTAL; off += chunk[c])
          (void)detector_push (det[c], in + off, chunk[c], res, MAX_RESULTS);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
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
    detector_destroy (det[c]);
  free (ref);
  free (in);
  free (res);
  jm_bench_write_json (&_bench, "detector");
  return 0;
}
