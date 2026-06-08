/**
 * @file detector_core.c
 * @brief 1-D signal detector implementation.
 *
 * The core data path is:
 *   push(x[M]) → ring buffer → non-blocking drain →
 *   corr_execute (FFT correlator + int-dump) →
 *   |·|² + argmax → noise estimate → threshold gate → det_result_t[]
 *
 * Ring buffer sizing:
 *   capacity = next_pow2(max(n, 512))
 *
 * The factor 512 ensures the double-mapping page-alignment constraint is met:
 *   512 samples × 8 bytes/sample (float complex) = 4096 bytes = 1 page.
 * Any power-of-2 multiple of 512 also satisfies the constraint.
 *
 * Noise estimation scratch buffer:
 *   Allocated at create time with capacity (noise_hi - noise_lo + 1) floats.
 *   Used only for DET_NOISE_MEDIAN to avoid a heap allocation on every push.
 */

#include "detector/detector_core.h"
#include "det_private.h"
#include "util/util_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

/**
 * @brief Compute peak_lag, peak_mag, noise_est, test_stat from out_buf.
 *
 * Fills the four result fields in @p state from the correlation output
 * already stored in state->out_buf (n complex samples).  Call immediately
 * after a dump (corr_execute returned n).
 */
static void
_compute_stat (detector_state_t *state)
{
  const size_t n = state->n;

  /* Compute magnitude vector. */
  for (size_t k = 0; k < n; k++)
    state->mag_buf[k] = cabsf (state->out_buf[k]);

  /* Argmax. */
  size_t peak = 0;
  for (size_t k = 1; k < n; k++)
    if (state->mag_buf[k] > state->mag_buf[peak])
      peak = k;

  state->peak_lag = peak;
  state->peak_mag = state->mag_buf[peak];

  state->noise_est
      = _noise_estimate (state->mag_buf, state->noise_lo, state->noise_hi,
                         state->noise_scratch, state->noise_mode);

  state->test_stat = (state->noise_est > 0.0f)
                         ? (state->peak_mag / state->noise_est)
                         : 0.0f;
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

detector_state_t *
detector_create (const float complex *ref, size_t n, size_t dwell,
                 size_t noise_lo, size_t noise_hi, det_noise_mode_t noise_mode,
                 float threshold, int nthreads)
{
  detector_state_t *state
      = (detector_state_t *)calloc (1, sizeof (detector_state_t));
  if (!state)
    return NULL;

  state->n          = n;
  state->noise_lo   = (noise_lo <= noise_hi) ? noise_lo : noise_hi;
  state->noise_hi   = (noise_lo <= noise_hi) ? noise_hi : noise_lo;
  state->noise_mode = noise_mode;
  state->threshold  = threshold;

  state->ring = _ring_create (n > 512 ? n : 512);
  if (!state->ring)
    goto fail;
  state->ring_cap = state->ring->capacity;

  state->corr = corr_create (ref, n, dwell, nthreads);
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
  detector_destroy (state);
  return NULL;
}

void
detector_destroy (detector_state_t *state)
{
  if (!state)
    return;
  if (state->ring)
    dp_f32_destroy (state->ring);
  if (state->corr)
    corr_destroy (state->corr);
  free (state->out_buf);
  free (state->mag_buf);
  free (state->noise_scratch);
  free (state);
}

void
detector_reset (detector_state_t *state)
{
  /* Drain the ring by resetting head/tail via atomic stores. */
  DP_STORE_REL (&state->ring->head, 0);
  DP_STORE_REL (&state->ring->tail, 0);
  corr_reset (state->corr);
  state->_last_corr_valid = 0;
}

void
detector_set_ref (detector_state_t *state, const float complex *ref)
{
  detector_reset (state);
  corr_set_ref (state->corr, ref);
}

void
detector_set_threshold (detector_state_t *state, float threshold)
{
  state->threshold = threshold;
}

/* ── Stream push ────────────────────────────────────────────────────────── */

size_t
detector_push (detector_state_t *state, const float complex *in, size_t n_in,
               det_result_t *result, size_t max_results)
{
  size_t ndet = 0;
  size_t off  = 0; /* samples consumed from in[] */

  while (off < n_in && ndet < max_results)
    {
      /* ── Write a chunk into the ring ──────────────────────────────── */
      size_t head     = DP_LOAD_RLX (&state->ring->head);
      size_t tail     = DP_LOAD_ACQ (&state->ring->tail);
      size_t space    = state->ring->capacity - (head - tail);
      size_t to_write = n_in - off;
      if (to_write > space)
        to_write = space;

      if (to_write > 0)
        {
          /* dp_f32_t treats each "sample" as one complex (2 floats);
           * casting to float* satisfies the API. */
          dp_f32_write (state->ring, (const float *)(in + off), to_write);
          off += to_write;
        }

      /* ── Drain complete frames ────────────────────────────────────── */
      while (ndet < max_results)
        {
          size_t h = DP_LOAD_ACQ (&state->ring->head);
          size_t t = DP_LOAD_RLX (&state->ring->tail);
          if (h - t < state->n)
            break; /* not enough for a frame */

          /* Zero-copy frame pointer: double-mapping guarantees contiguity
           * even when the frame wraps around the buffer boundary. */
          float complex *frame
              = (float complex *)(state->ring->data
                                  + (t & state->ring->mask) * 2);
          size_t n_out
              = corr_execute (state->corr, frame, state->n, state->out_buf);
          dp_f32_consume (state->ring, state->n);

          if (n_out == 0)
            continue; /* still accumulating — no dump yet */

          state->_last_corr_valid = 1;
          _compute_stat (state);

          if (state->threshold == 0.0f || state->test_stat > state->threshold)
            {
              result[ndet++]
                  = (det_result_t){ state->peak_lag, state->peak_mag,
                                    state->noise_est, state->test_stat };
            }
        }

      /* Safety: if we wrote nothing and drained nothing we'd spin forever.
       * In practice: space==0 only when ring is full, which implies at
       * least one complete frame (ring_cap >= n), so the drain loop always
       * makes progress.  This break handles pathological cases. */
      if (to_write == 0)
        break;
    }

  return ndet;
}
