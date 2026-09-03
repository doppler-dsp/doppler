

# File det\_private.h

[**File List**](files.md) **>** [**detector**](dir_f93f7a52d403868792375ffc90a1c1d7.md) **>** [**det\_private.h**](det__private_8h.md)

[Go to the documentation of this file](det__private_8h.md)


```C++

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

static size_t
det_peak_list (const float *surf, size_t ny, size_t nx, float gate,
               size_t excl_rows, size_t excl_cols, uint8_t *mask,
               det_peak_t *out, size_t max_peaks)
{
  const size_t n     = ny * nx;
  size_t       count = 0;
  if (!mask)
    {
      /* One peak, no zone to carry: the argmax, and the same pick the
         masked loop below makes (strict `>`, so the first maximum wins).
         The plain loop on purpose: a four-lane unrolled form runs 4x
         faster in isolation but moves detector2d::push by nothing
         measurable (doppler#1208), so the simple one stays. */
      if (n == 0)
        return 0;
      size_t best = 0;
      for (size_t k = 1; k < n; k++)
        if (surf[k] > surf[best])
          best = k;
      if (!(surf[best] > gate))
        return 0;
      out[0].row   = best / nx;
      out[0].col   = best % nx;
      out[0].value = surf[best];
      return 1;
    }
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
```


