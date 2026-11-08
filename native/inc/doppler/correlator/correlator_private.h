/**
 * @file correlator_private.h
 * @brief The correlator's detection layer: the ring, the noise estimate,
 *        the one peak picker and the framed push.
 *
 * Not part of the public API.  The correlator's detector flavors and the
 * acquisition engine include it (docs/design/correlator.md section 4: the
 * stream and detection layers, once).
 */
#ifndef DP_CORRELATOR_PRIVATE_H
#define DP_CORRELATOR_PRIVATE_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* det_noise_mode_t and det_peak_t are the correlator's public types. */
#include "doppler/correlator/correlator_core.h"
#include "doppler/f32_buffer/f32_buffer_core.h"
#include "doppler/util/util_core.h"

/* Create a dp_f32_t ring of at least cap_min complex samples.
 * dp_f32_create requires the byte count to be page-aligned, which varies
 * by OS (4 KiB on Linux/Windows, 16 KiB on macOS).  We start at the
 * smallest power-of-2 >= cap_min and double until create succeeds. */
static inline dp_f32_t *
det_ring_create (size_t cap_min)
{
  size_t cap = dp_next_pow_two (cap_min > 1 ? cap_min : 1);
  dp_f32_t *ring = NULL;
  while (!ring)
    {
      ring = dp_f32_create (cap);
      if (!ring)
        {
          cap <<= 1;
          if (cap > ((size_t)1 << 28))
            return NULL; /* refuse > 256 M samples */
        }
    }
  return ring;
}

static int
det_cmp_f32_asc (const void *a, const void *b)
{
  float fa = *(const float *)a;
  float fb = *(const float *)b;
  return (fa > fb) - (fa < fb);
}

/**
 * @brief The per-chunk noise aggregates -- MEAN, MIN, MAX -- over bins
 *        &#91;lo, hi&#93;, needing no scratch buffer.
 *
 * MEDIAN has no per-chunk form (it must sort the whole range), so it is not
 * computed here and returns 0; use det_noise_estimate() for it. A caller that
 * works a chunk at a time (acq's parallel tiles) calls THIS, so it cannot
 * even ask for a median without a scratch buffer: the invariant is in the
 * signature, not in a NULL passed to something that would dereference it.
 *
 * @param mag   Magnitude vector (length >= hi+1).
 * @param lo    First bin, inclusive.
 * @param hi    Last bin, inclusive.
 * @param mode  DET_NOISE_MEAN, DET_NOISE_MIN or DET_NOISE_MAX.
 * @return      The aggregate; 0 if lo > hi or mode is DET_NOISE_MEDIAN.
 */
static inline float
det_noise_chunk (const float *mag, size_t lo, size_t hi, det_noise_mode_t mode)
{
  if (lo > hi)
    return 0.0f;
  switch (mode)
    {
    case DET_NOISE_MEAN:
      {
        float s = 0.0f;
        for (size_t i = lo; i <= hi; i++)
          s += mag[i];
        return s / (float)(hi - lo + 1);
      }
    case DET_NOISE_MIN:
      {
        float m = mag[lo];
        for (size_t i = lo + 1; i <= hi; i++)
          if (mag[i] < m)
            m = mag[i];
        return m;
      }
    case DET_NOISE_MAX:
      {
        float m = mag[lo];
        for (size_t i = lo + 1; i <= hi; i++)
          if (mag[i] > m)
            m = mag[i];
        return m;
      }
    case DET_NOISE_MEDIAN:
      break; /* no per-chunk form: see above */
    }
  return 0.0f;
}

/**
 * @brief Aggregate |corr| over bins &#91;lo, hi&#93; using the selected mode.
 *
 * Returns 0 if lo > hi (empty range) — the caller maps that to test_stat=0.
 *
 * @param mag     Magnitude vector (length >= hi+1).
 * @param lo      First bin, inclusive.
 * @param hi      Last bin, inclusive.
 * @param scratch Caller-allocated buffer of length >= (hi-lo+1) floats;
 *                used only for DET_NOISE_MEDIAN (avoids a heap alloc per
 *                push).
 * @param mode    Aggregation mode.
 * @return        Aggregated noise estimate, or 0 if lo > hi.
 */
static inline float
det_noise_estimate (const float *mag, size_t lo, size_t hi, float *scratch,
                    det_noise_mode_t mode)
{
  if (mode != DET_NOISE_MEDIAN)
    return det_noise_chunk (mag, lo, hi, mode);
  if (lo > hi)
    return 0.0f;
  const size_t count = hi - lo + 1;
  memcpy (scratch, mag + lo, count * sizeof (float));
  qsort (scratch, count, sizeof (float), det_cmp_f32_asc);
  return scratch[count / 2];
}

/**
 * @brief The maximum of a surface, iterated with exclusion zones: every
 *        peak above a gate, strongest first, at most `max_peaks` of them.
 *
 * The one argmax under both detectors (docs/design/async-dsss-receiver.md
 * §7.1). Each pick is the largest unmasked cell; if it is not above
 * `gate` the list ends there (the gate is `eta` in the surface's own units,
 * so a second peak is another draw from the same cells against the same
 * union bound -- the threshold does not change with the list). A pick's
 * zone -- `excl_rows` either side along the rows and `excl_cols` along the
 * columns, CIRCULAR on both axes, since every surface this serves is an FFT
 * bin axis by a circular correlation lag axis -- is masked so the emitter
 * just reported cannot be reported again from its own shoulders; outside
 * the zone a second emitter has its own maximum. The zone is therefore the
 * detector's resolution, and it is the caller's to size from the code and
 * the dwell (one Doppler bin by one chip: the main lobe's first nulls).
 *
 * `mask` is the caller's, `ny * nx` bytes, initialised by the caller: 0 for
 * a candidate cell, non-zero for one that is never a candidate (a Doppler
 * band the engine does not search). On return every listed peak's zone is
 * marked as well. Nothing here allocates, and the cost is `max_peaks`
 * scans of the surface plus the zones -- the duration rule of §5.1.
 *
 * **`mask` may be NULL when `max_peaks` is 1.** The mask exists to carry a
 * pick's zone to the next pick; with one pick there is no next, and every
 * cell is a candidate. The call is then the classic detector's own loop --
 * one pass, one compare per cell, nothing written -- rather than a mask
 * cleared over the surface and read back once per cell for a zone that is
 * never applied. Measured (doppler#1208): the masked form at one peak cost
 * `detector2d::push` 22-43%. A NULL mask with `max_peaks > 1` is a caller
 * error and lists one peak.
 *
 * @param surf      The surface, row-major `ny x nx`.
 * @param ny, nx    Its geometry.
 * @param gate      A peak must exceed this (strictly) to be listed.
 * @param excl_rows Zone half-width along rows (0 = the row alone).
 * @param excl_cols Zone half-width along columns (0 = the column alone).
 * @param mask      `ny * nx` bytes, 0 = candidate; updated in place.
 * @param out       Receives up to `max_peaks` peaks, strongest first.
 * @param max_peaks Capacity of `out`.
 * @return          Peaks listed (0 when nothing exceeds the gate).
 */
static inline size_t
det_peak_list (const float *surf, size_t ny, size_t nx, float gate,
               size_t excl_rows, size_t excl_cols, uint8_t *mask,
               det_peak_t *out, size_t max_peaks);

/**
 * @brief One scan of det_peak_list(): the first maximum of `surf` over the
 *        cells `[k0, k1)` that `mask` leaves as candidates.
 *
 * Returns the cell, or `k1` when no cell in the range is a candidate. A
 * NaN cell is never a candidate (docs/design/correlator.md, U2), on either
 * path: a range of NaNs returns `k1`, as a fully masked one does. The
 * pick is the FIRST maximum (strict `>`), so a caller that cuts the surface
 * into chunks, scans each, and merges the chunks' picks in order with the
 * same strict `>` makes exactly the pick one scan over the whole surface
 * makes -- which is what lets the acquisition engine run the scan per tile
 * on its pool and merge serially, bit-identical at any thread count
 * (doppler#1243). `mask` may be NULL: every cell is then a candidate, and
 * the loop is the plain argmax on purpose -- a four-lane unrolled form runs
 * 4x faster in isolation but moves detector2d::push by nothing measurable
 * (doppler#1208), so the simple one stays.
 *
 * With a mask the surface can be large (an acquisition dwell over a wide
 * Doppler span is millions of cells, scanned once a pick), and there the
 * plain loop was 37% of a full-span AsyncDsssPool push (30.4 of 81.6 ms). So the masked scan is split
 * into blocks: a lane-wise pass finds each block's largest candidate, and
 * the plain loop, seeded with the pick so far, runs only on a block whose
 * largest candidate beats it. The pick is the plain loop's, NaN cells
 * included; test_correlator_detector_2d.c holds it to that loop over
 * multi-block surfaces. `isnan` is reliable here because every doppler build passes
 * -fno-finite-math-only (CMakeLists.txt, with its canary); a consumer of
 * this installed header under a bare -ffast-math loses the NaN rule.
 */
static size_t
det_peak_scan (const float *surf, const uint8_t *mask, size_t k0, size_t k1)
{
  size_t best = k1;
  if (!mask)
    {
      /* The seed is the first cell that is not NaN: nothing compares
         larger than a NaN, so a NaN seed would hold the pick against every
         later cell (U2: a NaN is never a peak). On finite data the seed
         is k0 and the loop is the plain argmax. */
      best = k0;
      while (best < k1 && isnan (surf[best]))
        best++;
      if (best >= k1)
        return k1;
      for (size_t k = best + 1; k < k1; k++)
        if (surf[k] > surf[best])
          best = k;
      return best;
    }
  /* Masked: the same loop, run only on the blocks that can move `best`.
     A block's largest candidate (and whether it has one) is a lane-wise
     pass the compiler vectorizes; the plain loop then runs on a block only
     when that largest candidate beats the pick so far (or there is no pick
     yet and the block has a candidate), seeded with the pick so far -- so
     the result is the plain loop's, cell for cell. A NaN enters the lane
     pass as -INFINITY, like a masked cell: the plain loop never takes a
     NaN either, and mapping it out here means no max
     instruction the compiler picks under -ffast-math (FMAX propagates a
     NaN, FMAXNM drops it) ever sees one.
     On noise the pick moves in a handful of blocks, so the cost is the one
     vector pass. */
  enum
  {
    LANES = 8,
    BLOCK = 256
  };
  for (size_t b0 = k0; b0 < k1; b0 += BLOCK)
    {
      const size_t b1 = k1 - b0 < BLOCK ? k1 : b0 + BLOCK;
      float        top[LANES];
      uint8_t      any[LANES];
      for (size_t l = 0; l < LANES; l++)
        {
          top[l] = -INFINITY;
          any[l] = 0;
        }
      size_t k = b0;
      for (; k + LANES <= b1; k += LANES)
        for (size_t l = 0; l < LANES; l++)
          {
            /* x != x needs NaNs honoured: every doppler build passes
               -fno-finite-math-only after -ffast-math. This header is
               installed; a consumer compiling it under a bare -ffast-math
               (or clang-cl /fp:fast) folds the test away. */
            const float x = surf[k + l];
            const float v = mask[k + l] || x != x ? -INFINITY : x;
            top[l]        = v > top[l] ? v : top[l];
            any[l] |= (uint8_t)!mask[k + l];
          }
      for (size_t l = 0; k < b1; k++, l++)
        {
          const float x = surf[k];
          const float v = mask[k] || x != x ? -INFINITY : x;
          top[l]        = v > top[l] ? v : top[l];
          any[l] |= (uint8_t)!mask[k];
        }
      float   m = top[0];
      uint8_t a = any[0];
      for (size_t l = 1; l < LANES; l++)
        {
          m = top[l] > m ? top[l] : m;
          a |= any[l];
        }
      if (best == k1 ? !a : !(m > surf[best]))
        continue;
      /* U2 lives in the seed alone, as on the unmasked path: the first
         candidate that is not NaN becomes the pick, and from there on a
         NaN never wins `surf[k] > surf[best]`, so the loop is the plain
         one and pays no per-cell NaN test. */
      for (k = b0; k < b1; k++)
        if (!mask[k]
            && (best == k1 ? !isnan (surf[k]) : surf[k] > surf[best]))
          best = k;
    }
  return best;
}

/**
 * @brief The exclusion zone of a pick at `(r, c)`, marked into `mask`:
 *        `excl_rows` either side along the rows and `excl_cols` along the
 *        columns, CIRCULAR on both axes (an FFT bin axis by a circular
 *        correlation lag axis), each half-width clamped to half the axis.
 */
static void
det_peak_zone (uint8_t *mask, size_t ny, size_t nx, size_t r, size_t c,
               size_t excl_rows, size_t excl_cols)
{
  const size_t rh = excl_rows < ny / 2 ? excl_rows : ny / 2;
  const size_t ch = excl_cols < nx / 2 ? excl_cols : nx / 2;
  for (size_t dr = 0; dr <= 2 * rh; dr++)
    {
      size_t rr = (r + ny + dr - rh) % ny;
      for (size_t dc = 0; dc <= 2 * ch; dc++)
        mask[rr * nx + (c + nx + dc - ch) % nx] = 1;
    }
}

static inline size_t
det_peak_list (const float *surf, size_t ny, size_t nx, float gate,
               size_t excl_rows, size_t excl_cols, uint8_t *mask,
               det_peak_t *out, size_t max_peaks)
{
  const size_t n     = ny * nx;
  size_t       count = 0;
  if (!mask)
    {
      /* One peak, no zone to carry: the argmax, and the same pick the
         masked loop below makes (strict `>`, so the first maximum wins). */
      if (n == 0)
        return 0;
      const size_t best = det_peak_scan (surf, NULL, 0, n);
      if (best == n || !(surf[best] > gate))
        return 0;
      out[0].row   = best / nx;
      out[0].col   = best % nx;
      out[0].value = surf[best];
      return 1;
    }
  while (count < max_peaks)
    {
      const size_t best = det_peak_scan (surf, mask, 0, n);
      if (best == n || !(surf[best] > gate))
        break;
      const size_t r = best / nx, c = best % nx;
      out[count].row   = r;
      out[count].col   = c;
      out[count].value = surf[best];
      count++;
      det_peak_zone (mask, ny, nx, r, c, excl_rows, excl_cols);
    }
  return count;
}

/* ── The framed drain, once ────────────────────────────────────────────── */

/**
 * @brief One frame's work for det_framed_push(): correlate @p frame and, on
 *        a dump that passes the gate, write ONE result at index @p slot of
 *        @p result. Returns 1 if it wrote one, 0 if not -- never more.
 */
typedef int (*det_frame_step_fn) (void *obj, const float _Complex *frame,
                                  void *result, size_t slot);

/**
 * @brief The detectors' push: any chunk in through the ring's framer, frames
 *        of @p n at hop @p n out through @p step, never a lost input.
 *
 * dp_correlator_push() is this, with the detector flavors' step.
 * Each frame yields at most one result, so the framer is fed only what
 * completes as many WHOLE frames as @p result has room for, and no partial
 * frame past them -- dp_f32_framer_feed_frames(), the rule acq shares -- and
 * every frame fed is drained before the next feed. That makes a batch exact
 * rather than an estimate: a batch can never write past the room (each frame
 * takes at most one slot) or strand a whole frame in the framer (all are
 * drained). Break either and the push overfills @p result or strands frames;
 * acq breaks the first (a dump reports several peaks),
 * which is why it has its own drain.
 *
 * Once @p result is full -- and with no room it is full from the start --
 * the push takes the rest of its input only if the rest completes no
 * frame (dp_f32_framer_feed_carry()): that rest is then the carry, taken
 * without loss. Otherwise it
 * takes nothing more, so a push that stops short of its input stops on a
 * frame boundary, never part-way into a frame it cannot report. A caller
 * that cannot resume (Python: one call per push) loses only what a push
 * leaves, and stays frame-aligned when that is a whole number of frames.
 * A resume loop needs room for at least one: with none, a push takes
 * nothing that completes a frame. Input that runs out mid-frame is the
 * carry, held for the next call.
 *
 * @param fr           The object's framer.
 * @param in           Input samples.
 * @param n_in         Samples in @p in.
 * @param result       The caller's results, handed to @p step.
 * @param max_results  Room in @p result.
 * @param step         The object's per-frame work.
 * @param obj          Handed to @p step.
 * @param consumed     Set to the samples of @p in this push took.
 * @return Results written.
 */
static inline size_t
det_framed_push (dp_f32_framer_t *fr, const float _Complex *in, size_t n_in,
                 void *result, size_t max_results, det_frame_step_fn step,
                 void *obj, size_t *consumed)
{
  size_t ndet = 0, off = 0;
  while (ndet < max_results)
    {
      off += dp_f32_framer_feed_frames_view (fr, in + off, n_in - off,
                                             max_results - ndet);
      size_t                drained = 0;
      const float _Complex *frame; /* into the ring, contiguous across wrap */
      while ((frame = dp_f32_framer_next_view (fr)) != NULL)
        {
          drained++;
          ndet += (size_t)step (obj, frame, result, ndet);
        }
      if (!drained)
        break; /* the input is used up: the rest of a frame is the carry */
    }
  /* Full: the rest is the carry if it completes no frame, else the push
     stops on the boundary. */
  if (ndet == max_results)
    off += dp_f32_framer_feed_carry_view (fr, in + off, n_in - off);
  *consumed = off;
  return ndet;
}

/* ── The search flavors' halves of the shared functions (#2752 3.4b) ───────
 * correlator_core.c's search section defines them; dp_correlator_push, _reset, _destroy and
 * the state triplet dispatch to them on the flavor (correlator_core.c), so
 * a caller names only the one function. */
/**
 * @brief dp_correlator_push() on a search flavor.
 * @param st A search flavor (non-NULL).
 * @param x Input, @p n_in complex samples.
 * @param n_in Samples in @p x.
 * @param out Hits, room for @p max_out.
 * @param max_out Capacity of @p out.
 * @return Hits written.
 */
size_t dp__search_push (dp_correlator_state_t *st, const float _Complex *x,
                        size_t n_in, dp_correlator_hit_t *out,
                        size_t max_out);
/**
 * @brief dp_correlator_reset() on a search flavor.
 * @param st A search flavor (non-NULL).
 */
void dp__search_reset (dp_correlator_state_t *st);
/**
 * @brief dp_correlator_destroy() on a search flavor.
 * @param st A search flavor; may be NULL.
 */
void dp__search_destroy (dp_correlator_state_t *st);
/**
 * @brief dp_correlator_consumed() on a search flavor.
 * @param st A search flavor (non-NULL).
 * @return Samples the last push took.
 */
size_t dp__search_consumed (const dp_correlator_state_t *st);
/**
 * @brief dp_correlator_state_bytes() on a search flavor.
 * @param st A search flavor (non-NULL).
 * @return The blob size, bytes.
 */
size_t dp__search_state_bytes (const dp_correlator_state_t *st);
/**
 * @brief dp_correlator_get_state() on a search flavor.
 * @param st A search flavor (non-NULL).
 * @param blob Destination, dp__search_state_bytes() bytes.
 */
void dp__search_get_state (const dp_correlator_state_t *st, void *blob);
/**
 * @brief dp_correlator_state_check() on a search flavor.
 * @param st A search flavor (non-NULL).
 * @param blob Candidate blob.
 * @return DP_OK or DP_ERR_INVALID; @p st is never written.
 */
int dp__search_state_check (const dp_correlator_state_t *st,
                            const void *blob);
/**
 * @brief dp_correlator_set_state() on a search flavor.
 * @param st A search flavor (non-NULL).
 * @param blob Source blob.
 * @return DP_OK, or DP_ERR_INVALID with @p st unchanged.
 */
int dp__search_set_state (dp_correlator_state_t *st, const void *blob);

/* ── The search flavors' rules, unit-tested as rules (#2752 3.4b) ───────── */

/* The rate hypotheses an engine at coherent depth `D` really searches.
 *
 * The de-chirp is exp(-j pi a t^2) over the block's epochs, t = (i - (D-1)/2)
 * T. At D = 1 that is t = 0, and at D = 2 it is t = +-T/2: equal t^2, so every
 * hypothesis multiplies the block by ONE constant phase and the surfaces are
 * identical. N looks that are one look must not be priced as N (the gate
 * rose 3.868 -> 4.214 for nothing), run N times (N times the cost), or pick
 * among (a strict > keeps hypothesis 0, which reports -R whatever the
 * carrier does: 1240 of 1240 hits at a true rate of 0, doppler#2221
 * review). A rate is invisible to a block of one or two epochs; the count
 * there is 1 and the reported rate is 0. Here rather than in search_core.c so
 * the bench, reporting the schedule the engine runs, passes the same
 * count. */
static inline size_t
search_rate_count_at (size_t rate_hyps, size_t D)
{
  return D > 2 ? rate_hyps : 1;
}

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
search_rate_better (size_t h, float stat, float best)
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
 * the map's ramp walks the tile's. Returns the native row that wins;
 * the pick's own row when the engine is not block-coherent and tiled. */
static inline size_t
search_resolve_tile_alias (const dp_correlator_state_t *st, size_t row, size_t col)
{
  const size_t D = st->search->coherent_bins, nx = st->search->code_bins;
  const size_t W = st->search->window_bins, rows = W * D;
  if (!st->search->blk_raw || W <= 1 || D <= 1)
    return row;
  const double f0 = (double)dp_fftfreq_index (row, rows) * st->search->doppler_res_hz;
  const double span = st->search->fs / (double)nx;
  /* The pick's column on a padded lag axis (lag_bins) is a lag in samples
     of col * code_bins / lag_bins; the raw epochs are correlated at the
     nearest whole sample. */
  if (st->search->lag_bins != nx)
    col = (size_t)lround ((double)col * (double)nx / (double)st->search->lag_bins)
          % nx;
  double       best = -1.0;
  long         win  = 0;
  for (long h = -1; h <= 1; h++)
    {
      const double f = f0 + (double)h * span;
      /* The code-rate walk of this hypothesis, samples per epoch: a
         positive Doppler runs the chip clock fast and the peak sits
         EARLIER each epoch (the map's ramp convention). */
      const double d  = st->search->carrier_freq_hz > 0.0
                            ? f / st->search->carrier_freq_hz * (double)nx
                            : 0.0;
      double       pw = 0.0;
      for (size_t e = 0; e < D; e++)
        {
          const long sh = lround (((double)e - 0.5 * (double)(D - 1)) * d);
          const long c0 = (((long)col - sh) % (long)nx + (long)nx) % (long)nx;
          const double _Complex acc = dp_correlator_cell_corr (
              st, st->search->blk_raw + e * nx, (size_t)c0, f, (double)(e * nx));
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

/**
 * @brief Per-object extra header for an engine's cross-call state.
 *
 * The state blob is the *only* thing a fresh engine needs to continue a
 * stream from `(descriptor, state, input)` — it makes the engine a pure
 * transducer for the elastic fan-out (thread / process / pod).  Standard
 * bytes interface (see dp_state.h); layout, contiguous and flat:
 *
 *   `[ dp_state_hdr_t ] [ uint64_t flavor ] [ search_extra_t ]`
 *   `[ the framer's snapshot ]`   (the carry, fewer than frame_n samples;
 *                                  its own self-validating child blob)
 *   `[ float          nc_surface[n] ]`             (only when n_noncoh > 1)
 *   `[ uint32_t       twins[2 * max_peaks] ]`      (held row, col pairs)
 *
 * Build the byte buffer with dp_correlator_state_bytes(); set_state validates the
 * envelope (magic/version/size) plus n / n_noncoh below, rejecting a
 * mismatch rather than reinterpreting it.
 */
typedef struct
{
  uint16_t has_nc; /**< 1 if `nc_surface[n]` follows the samples.     */
  uint16_t keep_surface;     /**< The keep_surface mutator, 0 or 1
                                  (#2022; version 2).                    */
  uint32_t n_noncoh;         /**< Non-coherent looks (consistency).     */
  uint64_t n;                /**< Frame size; must equal engine's n.    */
  uint64_t samples_consumed; /**< Stream offset framed so far.          */
  uint32_t nc_count;     /**< Looks accumulated in the current dump.    */
  uint32_t max_peaks;    /**< List capacity; must equal the engine's.   */
  uint32_t n_twins;      /**< Last dwell's picks that follow.           */
  uint32_t blk_epoch; /**< v3: epochs gathered in the block being built */
  uint32_t rate_hyps; /**< v6: rate hypotheses searched; must equal the
                           engine's -- it sizes the search and changes
                           what a surface means.                       */
  uint32_t _pad2;     /**< Alignment padding; written as 0.          */
  double carrier_freq_hz; /**< v8: the carrier (a mutator's value,
                               #2022): the block's raw epochs are
                               ramped at its end by the map, so a
                               restore takes the carrier through
                               dp_correlator_set_carrier_freq_hz()'s
                               rebuild.                            */
} search_extra_t;

/** @brief Where a search flavor's @ref search_extra_t sits in its blob: after
 *  the envelope and the flavor word every correlator blob opens with.  The
 *  one offset get_state, the check and the tests read it at. */
#define DP_CORRELATOR_SEARCH_EXTRA_OFF                                       \
  (sizeof (dp_state_hdr_t) + sizeof (uint64_t))

#endif /* DP_CORRELATOR_PRIVATE_H */
