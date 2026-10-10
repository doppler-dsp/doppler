/**
 * @file detector_core.c
 * @brief 1-D signal detector implementation.
 *
 * The core data path is:
 *   push(x[M]) → the ring's framer, one n-sample frame at a time →
 *   dp_corr_execute (FFT correlator + int-dump) →
 *   |·|² + argmax → noise estimate → threshold gate → det_result_t[]
 *
 * Ring buffer sizing:
 *   capacity = next_pow_two(max(n, 512))
 *
 * The factor 512 ensures the double-mapping page-alignment constraint is met:
 *   512 samples × 8 bytes/sample (float _Complex) = 4096 bytes = 1 page.
 * Any power-of-2 multiple of 512 also satisfies the constraint.
 *
 * Noise estimation scratch buffer:
 *   Allocated at create time with capacity (noise_hi - noise_lo + 1) floats.
 *   Used only for DET_NOISE_MEDIAN to avoid a heap allocation on every push.
 */

#include "doppler/detector/detector_core.h"
#include "det_private.h"
#include "doppler/util/util_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

/**
 * @brief Compute peak_lag, peak_mag, noise_est, test_stat from out_buf.
 *
 * Fills the four result fields in @p state from the correlation output
 * already stored in state->out_buf (n complex samples).  Call immediately
 * after a dump (dp_corr_execute returned n).
 */
static void
detector_compute_stat (dp_detector_state_t *state)
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
      = det_noise_estimate (state->mag_buf, state->noise_lo, state->noise_hi,
                            state->noise_scratch, state->noise_mode);

  state->test_stat = (state->noise_est > 0.0f)
                         ? (state->peak_mag / state->noise_est)
                         : 0.0f;
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

dp_detector_state_t *
dp_detector_create (const float _Complex *ref, size_t n, size_t dwell,
                    size_t noise_lo, size_t noise_hi,
                    det_noise_mode_t noise_mode, float threshold, int nthreads)
{
  dp_detector_state_t *state
      = (dp_detector_state_t *)calloc (1, sizeof (dp_detector_state_t));
  if (!state)
    return NULL;

  state->n = n;

  /* Clamp the noise window to the valid index range [0, n-1].  The binding
     passes a SIZE_MAX sentinel (default_raw) for the documented "n-1"
     full-window default; without this clamp it overflows the scratch sizing
     below and reads mag_buf out of bounds in det_noise_estimate. */
  size_t hi         = (noise_hi < state->n) ? noise_hi : state->n - 1;
  size_t lo         = (noise_lo < state->n) ? noise_lo : state->n - 1;
  state->noise_lo   = (lo <= hi) ? lo : hi;
  state->noise_hi   = (lo <= hi) ? hi : lo;
  state->noise_mode = noise_mode;
  state->threshold  = threshold;

  state->ring = det_ring_create (n > 512 ? n : 512);
  if (!state->ring)
    goto fail;
  state->ring_cap = state->ring->capacity;
  /* Frames of n at hop n: the correlator's frames tile the stream. */
  if (dp_f32_framer_init (&state->framer, state->ring, n, n) != DP_OK)
    goto fail;

  state->corr = dp_corr_create (ref, n, dwell, nthreads, 0);
  if (!state->corr)
    goto fail;

  state->out_buf = (float _Complex *)malloc (n * sizeof (float _Complex));
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
  dp_detector_destroy (state);
  return NULL;
}

void
dp_detector_destroy (dp_detector_state_t *state)
{
  if (!state)
    return;
  if (state->ring)
    dp_f32_destroy (state->ring);
  if (state->corr)
    dp_corr_destroy (state->corr);
  free (state->out_buf);
  free (state->mag_buf);
  free (state->noise_scratch);
  free (state);
}

void
dp_detector_reset (dp_detector_state_t *state)
{
  dp_f32_framer_reset (&state->framer);
  dp_corr_reset (state->corr);
  state->_last_corr_valid = 0;
  state->consumed         = 0;
}

/* Serializable state — the corr child (restored, not reset) + the framer's
 * carry as its own child blob (fixed-size for a given n, and self-validating)
 * + the last-dump result fields. consumed is per-call output, not state. */
size_t
dp_detector_state_bytes (const dp_detector_state_t *s)
{
  return sizeof (dp_state_hdr_t) + dp_corr_state_bytes (s->corr)
         + dp_f32_framer_state_bytes (&s->framer) + sizeof (uint64_t)
         + 3 * sizeof (float) + sizeof (uint32_t);
}

void
dp_detector_get_state (const dp_detector_state_t *s, void *blob)
{
  DP_GET_OPEN (DETECTOR_STATE_MAGIC, DETECTOR_STATE_VERSION,
               dp_detector_state_bytes (s));
  DP_W_CHILD (&_w, dp_corr, s->corr);
  DP_W_CHILD (&_w, dp_f32_framer, &s->framer);
  dp_w_u64 (&_w, s->peak_lag);
  dp_w_f32 (&_w, &s->peak_mag, 1);
  dp_w_f32 (&_w, &s->noise_est, 1);
  dp_w_f32 (&_w, &s->test_stat, 1);
  dp_w_u32 (&_w, (uint32_t)s->_last_corr_valid);
}

int
dp_detector_set_state (dp_detector_state_t *s, const void *blob)
{
  DP_SET_OPEN (DETECTOR_STATE_MAGIC, DETECTOR_STATE_VERSION,
               dp_detector_state_bytes (s));
  DP_R_CHILD (&_r, dp_corr, s->corr);
  DP_R_CHILD (&_r, dp_f32_framer, &s->framer);
  s->peak_lag = (size_t)dp_r_u64 (&_r);
  dp_r_f32 (&_r, &s->peak_mag, 1);
  dp_r_f32 (&_r, &s->noise_est, 1);
  dp_r_f32 (&_r, &s->test_stat, 1);
  s->_last_corr_valid = (int)dp_r_u32 (&_r);
  s->consumed         = 0;
  return DP_OK;
}

void
dp_detector_set_ref (dp_detector_state_t *state, const float _Complex *ref)
{
  dp_detector_reset (state);
  dp_corr_set_ref (state->corr, ref);
}

void
dp_detector_set_threshold (dp_detector_state_t *state, float threshold)
{
  state->threshold = threshold;
}

/* ── Stream push ────────────────────────────────────────────────────────── */

size_t
dp_detector_push (dp_detector_state_t *state, const float _Complex *in,
                  size_t n_in, det_result_t *result, size_t max_results)
{
  size_t ndet = 0;
  size_t off  = 0; /* samples taken from in[] */

  /* A frame yields at most one detection, so the framer is fed only what
   * completes as many frames as result still has room for -- and once it
   * is full, only what completes none: the carry. A sample is therefore
   * taken unless it would complete a frame result has no room for; that
   * sample and every one after it are left for the caller, who resumes at
   * in + dp_detector_consumed(). Feeding the whole room at once rather than
   * a frame at a time stops at the same sample (each fed frame can take at
   * most one slot) with one copy per batch instead of one per frame. Every
   * frame fed is drained before the next feed, so the framer is drained
   * whenever this returns: the carry is fewer than n samples and the state
   * blob has a fixed size. */
  for (;;)
    {
      if (off < n_in)
        off += dp_f32_framer_feed_view (&state->framer, in + off, n_in - off,
                                        max_results - ndet);
      size_t                drained = 0;
      const float _Complex *frame; /* into the ring, contiguous across wrap */
      while ((frame = dp_f32_framer_next_view (&state->framer)) != NULL)
        {
          drained++;
          size_t n_out = dp_corr_execute (state->corr, frame, state->n,
                                          state->out_buf, state->n);
          if (n_out == 0)
            continue; /* still accumulating — no dump yet */

          state->_last_corr_valid = 1;
          detector_compute_stat (state);

          if (state->threshold == 0.0f || state->test_stat > state->threshold)
            {
              result[ndet++]
                  = (det_result_t){ state->peak_lag, state->peak_mag,
                                    state->noise_est, state->test_stat };
            }
        }
      if (!drained)
        break; /* the input is used up, or the next frame has no room */
    }

  state->consumed = off;
  return ndet;
}

size_t
dp_detector_consumed (const dp_detector_state_t *state)
{
  return state->consumed;
}
