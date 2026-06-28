#include "corr/corr_core.h"
#include <string.h>

/* 1-D spectral zero-pad: q (length m >= n) is the band-limited (Dirichlet)
 * interpolation of p (length n) — low half [0..n/2] at the front, zeros in the
 * middle, high half at the end, with the Nyquist bin split for even n.
 * m == n is a copy.  Matches scipy.signal.resample to machine precision. */
static void
_zeropad_1d (const float complex *p, size_t n, float complex *q, size_t m)
{
  if (m == n)
    {
      memcpy (q, p, n * sizeof (*q));
      return;
    }
  memset (q, 0, m * sizeof (*q));
  size_t h = n / 2;
  for (size_t k = 0; k <= h; k++)
    q[k] = p[k];
  for (size_t k = h + 1; k < n; k++)
    q[m - n + k] = p[k];
  if (n % 2 == 0)
    {
      q[h] *= 0.5f;
      q[m - h] = q[h];
    }
}

corr_state_t *
corr_create (const float complex *ref, size_t n, size_t dwell, int nthreads,
             size_t n_out)
{
  corr_state_t *state = calloc (1, sizeof (*state)); /* NULL-init pointers */
  if (!state)
    return NULL;

  size_t no = n_out ? n_out : n;
  if (no < n) /* output may only interpolate (>= native) */
    {
      free (state);
      return NULL;
    }
  int decoupled = (no != n);

  state->fwd = fft_create (n, -1, nthreads);
  state->inv = fft_create (no, +1, nthreads);
  if (!state->fwd || !state->inv)
    goto fail;

  state->ref_spec = malloc (n * sizeof (*state->ref_spec));
  state->work_fft = malloc (n * sizeof (*state->work_fft));
  state->accum    = calloc (n, sizeof (*state->accum));
  if (!state->ref_spec || !state->work_fft || !state->accum)
    goto fail;

  if (decoupled)
    {
      state->work_pad = malloc (no * sizeof (float complex));
      if (!state->work_pad)
        goto fail;
    }

  /* Pre-compute conjugate reference spectrum: ref_spec = conj(FFT(ref)). */
  fft_execute_cf32 (state->fwd, ref, n, state->ref_spec);
  for (size_t k = 0; k < n; k++)
    state->ref_spec[k] = conjf (state->ref_spec[k]);

  state->n     = n;
  state->n_out = no;
  state->dwell = dwell;
  state->count = 0;
  return state;

fail:
  corr_destroy (state);
  return NULL;
}

void
corr_destroy (corr_state_t *state)
{
  if (!state)
    return;
  if (state->fwd)
    fft_destroy (state->fwd);
  if (state->inv)
    fft_destroy (state->inv);
  free (state->ref_spec);
  free (state->work_fft);
  free (state->accum);
  free (state->work_pad);
  free (state);
}

void
corr_reset (corr_state_t *state)
{
  memset (state->accum, 0, state->n * sizeof (*state->accum));
  state->count = 0;
}

/* Serializable state — running accumulator + frame count; the FFT plans and
 * the reference spectrum are config, recomputed by create() from the same ref.
 */
size_t
corr_state_bytes (const corr_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (uint64_t)
         + s->n * sizeof (float _Complex);
}

void
corr_get_state (const corr_state_t *s, void *blob)
{
  DP_GET_OPEN (CORR_STATE_MAGIC, CORR_STATE_VERSION, corr_state_bytes (s));
  dp_w_u64 (&_w, s->count);
  dp_w_cf32 (&_w, s->accum, s->n);
}

int
corr_set_state (corr_state_t *s, const void *blob)
{
  DP_SET_OPEN (CORR_STATE_MAGIC, CORR_STATE_VERSION, corr_state_bytes (s));
  s->count = (size_t)dp_r_u64 (&_r);
  dp_r_cf32 (&_r, s->accum, s->n);
  return DP_OK;
}

void
corr_set_ref (corr_state_t *state, const float complex *ref)
{
  fft_execute_cf32 (state->fwd, ref, state->n, state->ref_spec);
  for (size_t k = 0; k < state->n; k++)
    state->ref_spec[k] = conjf (state->ref_spec[k]);
  corr_reset (state);
}

size_t
corr_execute_max_out (corr_state_t *state)
{
  return state->n_out;
}

size_t
corr_execute (corr_state_t *state, const float complex *in, size_t n_in,
              float complex *out)
{
  (void)n_in; /* must equal state->n; caller's responsibility */

  /* Frequency-domain coherent accumulation: accumulate the per-frame cross-
   * spectrum  P_k = FFT(x_k) · conj(FFT(ref))  and invert once on dump,
   * instead of inverting every frame and summing the correlation surfaces.
   * Valid by linearity of the IFFT (Σ_k IFFT(P_k) = IFFT(Σ_k P_k), single 1/n
   * applied once) — and only because the dwell is COHERENT (a complex sum); a
   * non- coherent (Σ_k |IFFT(P_k)|²) integration is nonlinear and must invert
   * per frame. */
  fft_execute_cf32 (state->fwd, in, state->n, state->work_fft);

  for (size_t k = 0; k < state->n; k++)
    state->accum[k] += state->work_fft[k] * state->ref_spec[k];

  if (++state->count == state->dwell)
    {
      /* When n_out > n, zero-pad the accumulated spectrum first → band-limited
       * interpolation onto the finer grid; the normalization stays the native
       * 1/n (not 1/n_out) so the interpolated peak equals the native peak. */
      const float complex *src = state->accum;
      if (state->n_out != state->n)
        {
          _zeropad_1d (state->accum, state->n, state->work_pad, state->n_out);
          src = state->work_pad;
        }
      fft_execute_cf32 (state->inv, src, state->n_out, out);
      const float inv_n = 1.0f / (float)state->n;
      for (size_t k = 0; k < state->n_out; k++)
        out[k] *= inv_n;
      memset (state->accum, 0, state->n * sizeof (*state->accum));
      state->count = 0;
      return state->n_out;
    }
  return 0;
}
