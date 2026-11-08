

# File correlator\_private.h

[**File List**](files.md) **>** [**correlator**](dir_296d11a08af6c23b555f7cd87cd10cbd.md) **>** [**correlator\_private.h**](correlator__private_8h.md)

[Go to the documentation of this file](correlator__private_8h.md)


```C++

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

typedef int (*det_frame_step_fn) (void *obj, const float _Complex *frame,
                                  void *result, size_t slot);

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
size_t dp__search_push (dp_correlator_state_t *st, const float _Complex *x,
                        size_t n_in, dp_correlator_hit_t *out,
                        size_t max_out);
void dp__search_reset (dp_correlator_state_t *st);
void dp__search_destroy (dp_correlator_state_t *st);
size_t dp__search_consumed (const dp_correlator_state_t *st);
size_t dp__search_state_bytes (const dp_correlator_state_t *st);
void dp__search_get_state (const dp_correlator_state_t *st, void *blob);
int dp__search_state_check (const dp_correlator_state_t *st,
                            const void *blob);
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

typedef struct
{
  uint16_t has_nc; 
  uint16_t keep_surface;     
  uint32_t n_noncoh;         
  uint64_t n;                
  uint64_t samples_consumed; 
  uint32_t nc_count;     
  uint32_t max_peaks;    
  uint32_t n_twins;      
  uint32_t blk_epoch; 
  uint32_t rate_hyps; 
  uint32_t _pad2;     
  double carrier_freq_hz; 
} search_extra_t;

#define DP_CORRELATOR_SEARCH_EXTRA_OFF                                       \
  (sizeof (dp_state_hdr_t) + sizeof (uint64_t))

#endif /* DP_CORRELATOR_PRIVATE_H */
```


