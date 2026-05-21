/**
 * @file detector2d_core.c
 * @brief 2-D signal detector implementation.
 *
 * Structurally identical to detector_core.c with corr replaced by corr2d
 * and peak decomposed into (row, col) via integer divide/modulo.
 */

#include "detector2d/detector2d_core.h"
#include "detector/det_private.h"
#include "util/util_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

static void
_compute_stat_2d (detector2d_state_t *state)
{
  const size_t n = state->n;

  for (size_t k = 0; k < n; k++)
    state->mag_buf[k] = cabsf (state->out_buf[k]);

  size_t peak = 0;
  for (size_t k = 1; k < n; k++)
    if (state->mag_buf[k] > state->mag_buf[peak])
      peak = k;

  /* Decompose flat index into row/col. */
  state->peak_row = peak / state->nx;
  state->peak_col = peak % state->nx;
  state->peak_mag = state->mag_buf[peak];

  state->noise_est
      = _noise_estimate (state->mag_buf, state->noise_lo, state->noise_hi,
                         state->noise_scratch, state->noise_mode);

  state->test_stat = (state->noise_est > 0.0f)
                         ? (state->peak_mag / state->noise_est)
                         : 0.0f;
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

detector2d_state_t *
detector2d_create (const float complex *ref, size_t ny, size_t nx,
                   size_t dwell, size_t noise_lo, size_t noise_hi,
                   det_noise_mode_t noise_mode, float threshold, int nthreads)
{
  detector2d_state_t *state
      = (detector2d_state_t *)calloc (1, sizeof (detector2d_state_t));
  if (!state)
    return NULL;

  state->ny = ny;
  state->nx = nx;
  state->n = ny * nx;
  state->noise_lo = (noise_lo <= noise_hi) ? noise_lo : noise_hi;
  state->noise_hi = (noise_lo <= noise_hi) ? noise_hi : noise_lo;
  state->noise_mode = noise_mode;
  state->threshold = threshold;

  size_t n = state->n;
  state->ring = _ring_create (n > 512 ? n : 512);
  if (!state->ring)
    goto fail;
  state->ring_cap = state->ring->capacity;

  state->corr = corr2d_create (ref, ny, nx, dwell, nthreads);
  if (!state->corr)
    goto fail;

  state->out_buf = (float complex *)malloc (n * sizeof (float complex));
  if (!state->out_buf)
    goto fail;

  state->mag_buf = (float *)malloc (n * sizeof (float));
  if (!state->mag_buf)
    goto fail;

  size_t scratch_count = state->noise_hi - state->noise_lo + 1;
  state->noise_scratch = (float *)malloc (scratch_count * sizeof (float));
  if (!state->noise_scratch)
    goto fail;

  return state;

fail:
  detector2d_destroy (state);
  return NULL;
}

void
detector2d_destroy (detector2d_state_t *state)
{
  if (!state)
    return;
  if (state->ring)
    dp_f32_destroy (state->ring);
  if (state->corr)
    corr2d_destroy (state->corr);
  free (state->out_buf);
  free (state->mag_buf);
  free (state->noise_scratch);
  free (state);
}

void
detector2d_reset (detector2d_state_t *state)
{
  DP_STORE_REL (&state->ring->head, 0);
  DP_STORE_REL (&state->ring->tail, 0);
  corr2d_reset (state->corr);
  state->_last_corr_valid = 0;
}

void
detector2d_set_ref (detector2d_state_t *state, const float complex *ref)
{
  detector2d_reset (state);
  corr2d_set_ref (state->corr, ref);
}

void
detector2d_set_threshold (detector2d_state_t *state, float threshold)
{
  state->threshold = threshold;
}

/* ── Stream push ────────────────────────────────────────────────────────── */

size_t
detector2d_push (detector2d_state_t *state, const float complex *in,
                 size_t n_in, det_result2d_t *result, size_t max_results)
{
  size_t ndet = 0;
  size_t off = 0;

  while (off < n_in && ndet < max_results)
    {
      size_t head = DP_LOAD_RLX (&state->ring->head);
      size_t tail = DP_LOAD_ACQ (&state->ring->tail);
      size_t space = state->ring->capacity - (head - tail);
      size_t to_write = n_in - off;
      if (to_write > space)
        to_write = space;

      if (to_write > 0)
        {
          dp_f32_write (state->ring, (const float *)(in + off), to_write);
          off += to_write;
        }

      while (ndet < max_results)
        {
          size_t h = DP_LOAD_ACQ (&state->ring->head);
          size_t t = DP_LOAD_RLX (&state->ring->tail);
          if (h - t < state->n)
            break;

          float complex *frame = (float complex *)(state->ring->data
                                                   + (t & state->ring->mask)
                                                         * 2);
          size_t n_out = corr2d_execute (state->corr, frame, state->n,
                                         state->out_buf);
          dp_f32_consume (state->ring, state->n);

          if (n_out == 0)
            continue;

          state->_last_corr_valid = 1;
          _compute_stat_2d (state);

          if (state->threshold == 0.0f
              || state->test_stat > state->threshold)
            {
              result[ndet++] = (det_result2d_t){ state->peak_row,
                                                 state->peak_col,
                                                 state->peak_mag,
                                                 state->noise_est,
                                                 state->test_stat };
            }
        }

      if (to_write == 0)
        break;
    }

  return ndet;
}
