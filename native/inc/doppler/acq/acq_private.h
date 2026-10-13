/**
 * @file acq_private.h
 * @brief Shared internals for acq_core.c and its test.
 *
 * Not part of the public API: nothing here is exported, and nothing here is
 * promised to a caller. It exists so a rule that decides a result (the rate
 * search's pick, the tile-edge alias's hop) can be unit-tested as a rule
 * (test_acq_core.c includes it), the pattern detector/det_private.h follows.
 */
#ifndef ACQ_PRIVATE_H
#define ACQ_PRIVATE_H

#include "doppler/acq/acq_core.h"

#include <math.h>
#include <stddef.h>

/**
 * @brief Does hypothesis @p h's statistic @p stat replace the best so far,
 *        @p best? The rule the rate search picks its winner by.
 *
 * Hypothesis 0 always wins the first place, whatever it reads: a dwell
 * whose every statistic is NaN (a power that overflows float is inf/inf)
 * would otherwise fail `stat > best` at every hypothesis, pick none, and
 * leave the search restoring the PREVIOUS dwell's winning surface. After
 * it, a hypothesis must beat the best strictly, so a NaN never displaces
 * one and ties keep the earlier.
 *
 * Unit-tested as a rule on NaN / inf / finite values, because the
 * end-to-end route to a NaN statistic (a float power overflowing) depends
 * on the compiler's code generation and differs between Release and Debug.
 */
static inline int
acq_rate_better (size_t h, float stat, float best)
{
  return h == 0 || stat > best;
}

/* The tile-edge alias (doppler#1270). A tile de-rotates by its own
 * centre, so an emitter near the edge between two tiles leaves half a
 * span of residual inside the epoch in both, and the slow-time transform
 * folds modulo the epoch rate: the two neighbours read the emitter at the
 * same row index and, measured on the pool's grid at 45 dB-Hz, within
 * 0.03 dB of each other -- the pick is the noise's, one tile low or high,
 * half the time on the edge and one time in six 68 Hz inside it. The
 * block can tell when it is asked at the ROW's frequency: the raw epochs
 * mixed by the row's own frequency correlate with the replica at the
 * pick's code phase with under half a row of residual, and mixed by that
 * frequency one span off the residual is exactly one cycle per epoch, a
 * correlation of zero. Three hypotheses -- the row, one span down, one
 * span up -- summed over the block's epochs non-coherently (a data
 * transition inside an epoch costs every hypothesis the same), each
 * epoch's column walked by the hypothesis's own code rate as
 * acq_tile_epoch walks the tile's. Returns the native row that wins;
 * the pick's own row when the engine is not block-coherent and tiled. */
static inline size_t
acq_resolve_tile_alias (const dp_acq_state_t *st, size_t row, size_t col)
{
  const size_t D = st->coherent_bins, nx = st->code_bins;
  const size_t W = st->window_bins, rows = W * D;
  if (!st->blk_raw || W <= 1 || D <= 1)
    return row;
  const double f0 = (double)dp_fftfreq_index (row, rows) * st->doppler_res_hz;
  const double span = st->fs / (double)nx;
  double       best = -1.0;
  long         win  = 0;
  for (long h = -1; h <= 1; h++)
    {
      const double f = f0 + (double)h * span;
      /* The code-rate walk of this hypothesis, samples per epoch: a
         positive Doppler runs the chip clock fast and the peak sits
         EARLIER each epoch (acq_tile_epoch's convention). */
      const double d  = st->carrier_freq_hz > 0.0
                            ? f / st->carrier_freq_hz * (double)nx
                            : 0.0;
      double       pw = 0.0;
      for (size_t e = 0; e < D; e++)
        {
          const long sh = lround (((double)e - 0.5 * (double)(D - 1)) * d);
          const long c0 = (((long)col - sh) % (long)nx + (long)nx) % (long)nx;
          const double _Complex acc = dp_acq_cell_corr (
              st, st->blk_raw + e * nx, (size_t)c0, f, (double)(e * nx));
          pw += creal (acc) * creal (acc) + cimag (acc) * cimag (acc);
        }
      if (pw > best)
        {
          best = pw;
          win  = h;
        }
    }
  return (size_t)(((long)row + win * (long)D + (long)rows) % (long)rows);
}

#endif /* ACQ_PRIVATE_H */
