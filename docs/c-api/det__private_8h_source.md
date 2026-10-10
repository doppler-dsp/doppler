

# File det\_private.h

[**File List**](files.md) **>** [**detector**](dir_4cdf6fdfdd426ef1a31e056182554d6b.md) **>** [**det\_private.h**](det__private_8h.md)

[Go to the documentation of this file](det__private_8h.md)


```C++

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

static inline size_t
det_peak_list (const float *surf, size_t ny, size_t nx, float gate,
               size_t excl_rows, size_t excl_cols, uint8_t *mask,
               det_peak_t *out, size_t max_peaks);

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

typedef int (*det_frame_step_fn) (void *obj, const float _Complex *frame,
                                  void *result, size_t slot);

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
```


