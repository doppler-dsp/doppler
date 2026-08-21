/* bench_detector2d_core.c -- the shape penalty, one layer up from the FFT.
 *
 * `bench_fft2d_core.c` shows that a separable 2-D transform is not priced
 * by its bin count: at a constant number of bins, an elongated array costs
 * well over a square one, because one of the two passes strides. This file
 * asks whether that survives the detector wrapped around it -- the ring,
 * the peak search over the whole surface, and the noise aggregate over the
 * noise band are all O(n) and identical across shapes, so they dilute the
 * transform's shape penalty by a factor this measures rather than predicts.
 *
 * That matters because the caller's choice is real. A range-Doppler map is
 * "pulses by range gates"; how those multiply out is set by the waveform,
 * and a designer trading PRF against gate count is choosing this row
 * whether or not they know it.
 *
 * Three shapes at a constant 16384 bins, plus one repeat of the square
 * shape with a threshold nothing reaches -- so the pair differs only in
 * whether a detection is emitted. In 1-D that pair came out free; a 2-D
 * emit carries a row and a column instead of one lag, and this says
 * whether that changes anything.
 */
#include "detector2d/detector2d_core.h"
#include "dp_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 60
#define BINS 16384
#define FRAMES 4
#define MAX_RESULTS 64
#define DWELL 1

#define N_CFG 4
static const size_t cfg_ny[N_CFG] = { 16, 128, 1024, 128 };
static const size_t cfg_nx[N_CFG] = { 1024, 128, 16, 128 };
/* The last configuration repeats the square shape with a threshold nothing
   reaches, so the pair differs only in whether it emits. */
static const float cfg_thresh[N_CFG] = { 0.0f, 0.0f, 0.0f, 1e30f };
#define SQUARE_IDX 1
#define SILENT_IDX 3

int
main (void)
{
  jm_bench_t          _bench = { 0 };
  struct timespec     t0, t1;
  static double       t[N_CFG][ITERATIONS];
  detector2d_state_t *det[N_CFG] = { 0 };
  float complex      *ref = NULL, *in = NULL;
  det_result2d_t     *res = NULL;
  char                name[72];

  ref = malloc (BINS * sizeof *ref);
  in  = malloc ((size_t)FRAMES * BINS * sizeof *in);
  res = malloc (MAX_RESULTS * sizeof *res);
  if (!ref || !in || !res)
    return 1;

  /* A chirp reference and the same chirp in the stream: a real peak, so
     the surface search and the noise aggregate see representative data. */
  for (size_t i = 0; i < BINS; i++)
    {
      const double p = 1e-5 * (double)i * (double)i;
      ref[i]         = (float complex) (cos (p) + sin (p) * I);
    }
  for (size_t i = 0; i < (size_t)FRAMES * BINS; i++)
    {
      const double q = 1e-5 * (double)(i % BINS) * (double)(i % BINS);
      in[i]          = (float complex) (cos (q) + sin (q) * I);
    }

  for (int c = 0; c < N_CFG; c++)
    {
      det[c] = detector2d_create (ref, cfg_ny[c], cfg_nx[c], DWELL, 1,
                                  BINS - 1, DET_NOISE_MEAN, cfg_thresh[c], 1);
      if (!det[c])
        return 1;
    }

  printf ("=== detector2d (fft2d corr + ring + peak/noise gate) ===\n");
  printf ("%d bins per frame, %d frames per round, %d rounds, min over "
          "rounds\n\n",
          BINS, FRAMES, ITERATIONS);

  DP_BENCH_SETTLE (
      (void)detector2d_push (det[SQUARE_IDX], in, BINS, res, MAX_RESULTS));

  /* Rounds outside, shapes inside: three shapes of one bin count are read
     against each other, so drift must land on all of them. */
  for (int r = 0; r < ITERATIONS; r++)
    for (int c = 0; c < N_CFG; c++)
      {
        detector2d_reset (det[c]);
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int f = 0; f < FRAMES; f++)
          (void)detector2d_push (det[c], in + (size_t)f * BINS, BINS, res,
                                 MAX_RESULTS);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t[c][r] = dp_bench_elapsed (&t0, &t1);
      }

  for (int c = 0; c < N_CFG; c++)
    {
      (void)snprintf (name, sizeof name, "push[%zux%zu%s]", cfg_ny[c],
                      cfg_nx[c], cfg_thresh[c] > 0.0f ? ",no-detections" : "");
      dp_bench_record (&_bench, name, t[c], ITERATIONS, (size_t)FRAMES * BINS,
                       "sample");
    }

  printf ("\n  shape cost at a constant %d bins (%zux%zu = 1.00x):\n", BINS,
          cfg_ny[SQUARE_IDX], cfg_nx[SQUARE_IDX]);
  for (int c = 0; c < N_CFG; c++)
    {
      if (c == SILENT_IDX)
        continue;
      printf ("    %4zu x %-5zu  %.2fx\n", cfg_ny[c], cfg_nx[c],
              dp_bench_min (t[c], ITERATIONS)
                  / dp_bench_min (t[SQUARE_IDX], ITERATIONS));
    }
  printf ("  Compare this spread against bench_fft2d_core's. The detector\n"
          "  adds O(n) work that is identical across shapes, so a SMALLER\n"
          "  spread here than there is the wrapper diluting the\n"
          "  transform's penalty, not the penalty going away.\n");
  printf ("\n  emitting a detection on every dump costs %.2fx over\n"
          "  emitting none, same shape, same statistic.\n",
          dp_bench_min (t[SQUARE_IDX], ITERATIONS)
              / dp_bench_min (t[SILENT_IDX], ITERATIONS));

  for (int c = 0; c < N_CFG; c++)
    detector2d_destroy (det[c]);
  free (ref);
  free (in);
  free (res);
  jm_bench_write_json (&_bench, "detector2d");
  return 0;
}
