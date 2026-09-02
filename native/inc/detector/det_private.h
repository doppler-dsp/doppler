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
#ifndef DET_NOISE_MODE_T_DEFINED
#  error "Include detector_core.h or detector2d_core.h before det_private.h"
#endif

static size_t
next_pow2 (size_t n)
{
  size_t c = 1;
  while (c < n)
    c <<= 1;
  return c;
}

/* Create a dp_f32_t ring of at least cap_min complex samples.
 * dp_f32_create requires the byte count to be page-aligned, which varies
 * by OS (4 KiB on Linux/Windows, 16 KiB on macOS).  We start at the
 * smallest power-of-2 >= cap_min and double until create succeeds. */
static dp_f32_t *
det_ring_create (size_t cap_min)
{
  size_t cap = next_pow2 (cap_min > 1 ? cap_min : 1);
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
static float
det_noise_estimate (const float *mag, size_t lo, size_t hi, float *scratch,
                 det_noise_mode_t mode)
{
  if (lo > hi)
    return 0.0f;
  size_t count = hi - lo + 1;
  switch (mode)
    {
    case DET_NOISE_MEAN:
      {
        float s = 0.0f;
        for (size_t i = lo; i <= hi; i++)
          s += mag[i];
        return s / (float)count;
      }
    case DET_NOISE_MEDIAN:
      memcpy (scratch, mag + lo, count * sizeof (float));
      qsort (scratch, count, sizeof (float), det_cmp_f32_asc);
      return scratch[count / 2];
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
    }
  return 0.0f; /* unreachable */
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
static size_t
det_peak_list (const float *surf, size_t ny, size_t nx, float gate,
               size_t excl_rows, size_t excl_cols, uint8_t *mask,
               det_peak_t *out, size_t max_peaks)
{
  const size_t n     = ny * nx;
  size_t       count = 0;
  while (count < max_peaks)
    {
      size_t best = n;
      for (size_t k = 0; k < n; k++)
        if (!mask[k] && (best == n || surf[k] > surf[best]))
          best = k;
      if (best == n || !(surf[best] > gate))
        break;
      const size_t r = best / nx, c = best % nx;
      out[count].row   = r;
      out[count].col   = c;
      out[count].value = surf[best];
      count++;
      /* The zone, circular on both axes. */
      const size_t rh = excl_rows < ny / 2 ? excl_rows : ny / 2;
      const size_t ch = excl_cols < nx / 2 ? excl_cols : nx / 2;
      for (size_t dr = 0; dr <= 2 * rh; dr++)
        {
          size_t rr = (r + ny + dr - rh) % ny;
          for (size_t dc = 0; dc <= 2 * ch; dc++)
            mask[rr * nx + (c + nx + dc - ch) % nx] = 1;
        }
    }
  return count;
}

#endif /* DET_PRIVATE_H */
