/**
 * delay_doppler_batch_demo.c — many delay-Doppler maps in one call.
 *
 * A receiver searching a carrier offset tries one frame under several
 * hypotheses and keeps the best.  Here a code delayed by 37 samples arrives
 * on a carrier 2.4 code-bins away from nominal: each hypothesis removes one
 * candidate offset (0, 0.5, ... 3.5 bins) from a copy of the frame, the
 * copies go to DelayDoppler as ONE batch, and the per-map peaks pick the
 * hypothesis that left the signal most compact.
 *
 * Why there is a search at all: a carrier left uncorrected within each
 * period costs the code correlation its Dirichlet factor.  A whole code bin
 * nulls it, and near one bin the peak sinks under the code's sidelobes, so
 * those maps do not even find the delay.  Within half a bin the loss is at
 * most 3.9 dB (docs/design/corr2d-hypotheses.md section 3), and the delay is
 * found.  What a C caller manages, and the Python binding does for you:
 *
 *   - create once, with the worker count (0 = every core);
 *   - the batch is H frames back to back, n_in = H * ny * nx;
 *   - every output buffer is the caller's, sized by the _max_out calls,
 *     and a short one is refused with SIZE_MAX and nothing written;
 *   - a batch never touches the dwell, so execute() carries on after it;
 *   - destroy once.
 *
 * Build and run:
 *   cmake --build build
 *   ./build/native/examples/delay_doppler_batch_demo
 */

#include <doppler/corr2d/corr2d_core.h>

#include "doppler/dp_complex.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum
{
  NX    = 128, /* code length, samples per period  */
  NY    = 16,  /* periods per frame (slow time)    */
  DELAY = 37,  /* the code's delay, samples        */
  H     = 8    /* carrier hypotheses               */
};

static const double OFFSET_BINS = 2.4; /* true carrier offset, 1/nx units */
static const double STEP_BINS   = 0.5; /* hypothesis spacing              */

/* A +-1 code from a fixed xorshift32, so every run is the same. */
static void
make_code (float _Complex *code)
{
  unsigned s = 0x2683u;
  for (size_t p = 0; p < NX; p++)
    {
      s ^= s << 13;
      s ^= s >> 17;
      s ^= s << 5;
      code[p] = (s >> 31) ? 1.0f : -1.0f;
    }
}

int
main (void)
{
  float _Complex code[NX];
  make_code (code);

  /* The received frame: the delayed code on a carrier OFFSET_BINS / NX
     cycles/sample off nominal. */
  float _Complex rx[NY * NX];
  for (size_t m = 0; m < (size_t)NY * NX; m++)
    rx[m] = code[(m % NX + NX - DELAY) % NX]
            * cexpf ((float)(2.0 * M_PI * OFFSET_BINS / NX * (double)m) * I);

  /* Hypothesis h removes h * STEP_BINS: H corrected copies, back to back. */
  float _Complex *batch = malloc ((size_t)H * NY * NX * sizeof *batch);
  for (size_t h = 0; h < H; h++)
    for (size_t m = 0; m < (size_t)NY * NX; m++)
      batch[h * (size_t)NY * NX + m]
          = rx[m]
            * cexpf (
                (float)(-2.0 * M_PI * STEP_BINS * (double)h / NX * (double)m)
                * I);

  dp_corr2d_state_t *dd
      = dp_corr2d_create_delay_doppler (code, NX, NY, 1, 0, 0);

  /* One record per map: its peak power, where it is, and its mean. */
  const size_t n_in = (size_t)H * NY * NX;
  const size_t nrec = dp_corr2d_execute_peaks_max_out (dd, n_in);
  float        peak[H], mean[H];
  uint32_t     row[H], col[H];
  if (dp_corr2d_execute_peaks (dd, batch, n_in, peak, row, col, mean, nrec)
      != H)
    return 1;

  size_t best = 0;
  printf ("hypothesis  removed (bins)  peak/mean  delay\n");
  for (size_t h = 0; h < H; h++)
    {
      printf ("  %2zu          %4.1f        %8.1f   %3u\n", h,
              STEP_BINS * (double)h, (double)(peak[h] / mean[h]), col[h]);
      if (peak[h] / mean[h] > peak[best] / mean[best])
        best = h;
    }
  printf ("best: hypothesis %zu (%.1f bins removed; true offset %.1f)\n", best,
          STEP_BINS * (double)best, OFFSET_BINS);

  /* Every map too, into the caller's buffer, sized by its own query. */
  const size_t    n_all = dp_corr2d_execute_all_max_out (dd, n_in);
  float _Complex *maps  = malloc (n_all * sizeof *maps);
  const size_t    wrote = dp_corr2d_execute_all (dd, batch, n_in, maps, n_all);
  /* A buffer one value short is refused, and nothing is written. */
  const size_t short_call
      = dp_corr2d_execute_all (dd, batch, n_in, maps, n_all - 1);
  printf ("execute_all wrote %zu values (%d maps); one short: %s\n", wrote, H,
          short_call == SIZE_MAX ? "refused" : "ACCEPTED");

  /* The dwell never moved. */
  printf ("dwell count after the batches: %zu\n", dd->count);

  /* The hypothesis nearest the true offset wins, and every hypothesis that
     leaves less than half a code bin uncorrected finds the delay. */
  int ok = best == (size_t)(OFFSET_BINS / STEP_BINS + 0.5) && wrote == n_all
           && short_call == SIZE_MAX && dd->count == 0;
  for (size_t h = 0; h < H; h++)
    if (fabs (OFFSET_BINS - STEP_BINS * (double)h) < 0.5)
      ok &= col[h] == DELAY;

  dp_corr2d_destroy (dd);
  free (maps);
  free (batch);
  return ok ? 0 : 1;
}
