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
_ring_create (size_t cap_min)
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
_cmp_f32_asc (const void *a, const void *b)
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
_noise_estimate (const float *mag, size_t lo, size_t hi, float *scratch,
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
      qsort (scratch, count, sizeof (float), _cmp_f32_asc);
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

#endif /* DET_PRIVATE_H */
