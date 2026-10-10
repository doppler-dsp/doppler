/**
 * @file det_private.h
 * @brief Shared internals for detector_core.c and detector2d_core.c.
 *
 * Not part of the public API.  Include after the module's own header so
 * that det_noise_mode_t is already defined via the DET_NOISE_MODE_T_DEFINED
 * guard in detector_core.h / detector2d_core.h.
 */
#ifndef DET_PRIVATE_H
#define DET_PRIVATE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* det_noise_mode_t must be visible before this header is included. */
#include "doppler/f32_buffer/f32_buffer_core.h"
#include "doppler/util/util_core.h"

#ifndef DET_NOISE_MODE_T_DEFINED
#  error "Include detector_core.h or detector2d_core.h before det_private.h"
#endif

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

/* det_peak_t (one listed peak) is public in detector2d_core.h; the 1-D
   detector includes neither, so define it here under the same guard. */
#ifndef DET_PEAK_T_DEFINED
#define DET_PEAK_T_DEFINED
typedef struct
{
  size_t row;
  size_t col;
  float  value;
} det_peak_t;
#endif

/**
 * @brief The maximum of a surface, iterated with exclusion zones: every
 *        peak above a gate, strongest first, at most `max_peaks` of them.
 *
 * The one argmax under both detectors (docs/design/async-dsss-receiver.md
 * §7.1, §8 (a)). Each pick is the largest unmasked cell; if it is not above
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
 * Returns the cell, or `k1` when no cell in the range is a candidate. The
 * pick is the FIRST maximum (strict `>`), so a caller that cuts the surface
 * into chunks, scans each, and merges the chunks' picks in order with the
 * same strict `>` makes exactly the pick one scan over the whole surface
 * makes -- which is what lets the acquisition engine run the scan per tile
 * on its pool and merge serially, bit-identical at any thread count
 * (doppler#1243). `mask` may be NULL: every cell is then a candidate, and
 * the loop is the plain argmax on purpose -- a four-lane unrolled form runs
 * 4x faster in isolation but moves detector2d::push by nothing measurable
 * (doppler#1208), so the simple one stays.
 */
static size_t
det_peak_scan (const float *surf, const uint8_t *mask, size_t k0, size_t k1)
{
  size_t best = k1;
  if (!mask)
    {
      if (k0 >= k1)
        return k1;
      best = k0;
      for (size_t k = k0 + 1; k < k1; k++)
        if (surf[k] > surf[best])
          best = k;
      return best;
    }
  for (size_t k = k0; k < k1; k++)
    if (!mask[k] && (best == k1 || surf[k] > surf[best]))
      best = k;
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
      if (!(surf[best] > gate))
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
 * dp_detector_push() and dp_detector2d_push() are this, with their own
 * step. Each frame yields at most one result, so the framer is fed only
 * what completes as many WHOLE frames as @p result has room for -- the room
 * times n, less the carry already held, and no partial frame past them --
 * and every frame fed is drained before the next feed. That makes a batch
 * exact rather than an estimate: a batch can never write past the room
 * (each frame takes at most one slot) or strand a whole frame in the
 * framer (all are drained). Break either and the push overfills @p result
 * or strands frames; acq breaks the first (a dump reports several peaks),
 * which is why it has its own drain.
 *
 * Once @p result is full the push takes NOTHING more, not even a partial
 * frame of carry, so a push that stopped full stops on a frame boundary:
 * a caller that cannot resume (Python's one call per push) stays frame-
 * aligned instead of shifted by the carry it never sees. A push with no
 * room takes nothing at all, so a resume loop needs room for at least one.
 * Input that runs out mid-frame is the carry, held for the next call.
 *
 * @param fr           The object's framer, bound at frame and hop @p n.
 * @param n            Samples per frame.
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
det_framed_push (dp_f32_framer_t *fr, size_t n, const float _Complex *in,
                 size_t n_in, void *result, size_t max_results,
                 det_frame_step_fn step, void *obj, size_t *consumed)
{
  size_t ndet = 0, off = 0;
  while (ndet < max_results)
    {
      const size_t room = max_results - ndet;
      size_t       take = n_in - off;
      /* No more than completes `room` whole frames: frames tile the stream
         at hop n, so the carry is the framer's pending count. */
      if (room <= SIZE_MAX / n)
        {
          const size_t upto = room * n - dp_f32_framer_pending (fr);
          if (take > upto)
            take = upto;
        }
      if (take)
        off += dp_f32_framer_feed_view (fr, in + off, take, room);
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
  *consumed = off;
  return ndet;
}

#endif /* DET_PRIVATE_H */
