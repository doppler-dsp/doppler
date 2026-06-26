#include "corr2d/corr2d_core.h"
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

/* 2-D spectral zero-pad (ny,nx) -> (ny_out,nx_out), axis-separable: pad rows
 * (nx -> nx_out) into ztmp, then columns (ny -> ny_out) into out.  The
 * even-axis Nyquist split is handled per axis by _zeropad_1d. */
static void
_zeropad_2d (corr2d_state_t *s, const float complex *p, float complex *out)
{
  for (size_t i = 0; i < s->ny; i++)
    _zeropad_1d (p + i * s->nx, s->nx, s->ztmp + i * s->nx_out, s->nx_out);
  for (size_t j = 0; j < s->nx_out; j++)
    {
      for (size_t i = 0; i < s->ny; i++)
        s->zcol[i] = s->ztmp[i * s->nx_out + j];
      _zeropad_1d (s->zcol, s->ny, s->zcolout, s->ny_out);
      for (size_t i = 0; i < s->ny_out; i++)
        out[i * s->nx_out + j] = s->zcolout[i];
    }
}

corr2d_state_t *
corr2d_create (const float complex *ref, size_t ny, size_t nx, size_t dwell,
               int nthreads, size_t ny_out, size_t nx_out)
{
  corr2d_state_t *state = calloc (1, sizeof (*state)); /* NULL-init pointers */
  if (!state)
    return NULL;

  size_t n   = ny * nx;
  size_t nyo = ny_out ? ny_out : ny;
  size_t nxo = nx_out ? nx_out : nx;
  if (nyo < ny || nxo < nx) /* output may only interpolate (>= native) */
    {
      free (state);
      return NULL;
    }
  int decoupled = (nyo != ny) || (nxo != nx);

  state->fwd = fft2d_create (ny, nx, -1, nthreads);
  state->inv = fft2d_create (nyo, nxo, +1, nthreads);
  if (!state->fwd || !state->inv)
    goto fail;

  state->ref_spec = malloc (n * sizeof (*state->ref_spec));
  state->work_fft = malloc (n * sizeof (*state->work_fft));
  state->accum    = calloc (n, sizeof (*state->accum));
  if (!state->ref_spec || !state->work_fft || !state->accum)
    goto fail;

  if (decoupled)
    {
      state->work_pad = malloc (nyo * nxo * sizeof (float complex));
      state->ztmp     = malloc (ny * nxo * sizeof (float complex));
      state->zcol     = malloc (ny * sizeof (float complex));
      state->zcolout  = malloc (nyo * sizeof (float complex));
      if (!state->work_pad || !state->ztmp || !state->zcol || !state->zcolout)
        goto fail;
    }

  /* Pre-compute conjugate reference spectrum: ref_spec = conj(FFT2(ref)). */
  fft2d_execute_cf32 (state->fwd, ref, n, state->ref_spec);
  for (size_t k = 0; k < n; k++)
    state->ref_spec[k] = conjf (state->ref_spec[k]);

  state->ny     = ny;
  state->nx     = nx;
  state->n      = n;
  state->ny_out = nyo;
  state->nx_out = nxo;
  state->n_out  = nyo * nxo;
  state->dwell  = dwell;
  state->count  = 0;
  return state;

fail:
  corr2d_destroy (state);
  return NULL;
}

void
corr2d_destroy (corr2d_state_t *state)
{
  if (!state)
    return;
  if (state->fwd)
    fft2d_destroy (state->fwd);
  if (state->inv)
    fft2d_destroy (state->inv);
  free (state->ref_spec);
  free (state->work_fft);
  free (state->accum);
  free (state->work_pad);
  free (state->ztmp);
  free (state->zcol);
  free (state->zcolout);
  free (state);
}

void
corr2d_reset (corr2d_state_t *state)
{
  memset (state->accum, 0, state->n * sizeof (*state->accum));
  state->count = 0;
}

void
corr2d_set_ref (corr2d_state_t *state, const float complex *ref)
{
  fft2d_execute_cf32 (state->fwd, ref, state->n, state->ref_spec);
  for (size_t k = 0; k < state->n; k++)
    state->ref_spec[k] = conjf (state->ref_spec[k]);
  corr2d_reset (state);
}

size_t
corr2d_execute_max_out (corr2d_state_t *state)
{
  return state->n_out;
}

size_t
corr2d_execute (corr2d_state_t *state, const float complex *in, size_t n_in,
                float complex *out)
{
  (void)n_in;

  /* Frequency-domain coherent accumulation.  Accumulate the per-frame cross-
   * spectrum  P_k = FFT2(x_k) · conj(FFT2(ref))  and invert once on dump,
   * instead of inverting every frame and summing the correlation surfaces.
   * One IFFT2 per dump instead of dwell of them.
   *
   * VALID UNDER:
   *   1. Coherent integration — the per-dump combination is a COMPLEX (linear)
   *      sum.  This is the load-bearing condition: the deferral relies on the
   *      inverse DFT being linear, Σ_k IFFT2(P_k) = IFFT2(Σ_k P_k), with the
   *      single 1/n applied once either way.  A NON-coherent dump
   *      (Σ_k |IFFT2(P_k)|², a magnitude/energy sum) is nonlinear and must
   *      transform each frame — it cannot defer the inverse.  So this path is
   *      specific to corr2d's coherent `dwell`; a future non-coherent mode
   * must invert per frame.
   *   2. A single inverse transform + normalization for the whole dwell (here
   *      the fixed (ny,nx) plan and 1/n) — trivially met, since the reference
   *      and grid are constant across a dump.  (The reference need not be
   *      constant for linearity to hold, but corr2d's is.)
   *
   * Equivalence is exact in real arithmetic; in cf32 it differs from the
   * per-frame sum only by accumulation-order rounding (~1e-5 relative). */
  fft2d_execute_cf32 (state->fwd, in, state->n, state->work_fft);

  for (size_t k = 0; k < state->n; k++)
    state->accum[k] += state->work_fft[k] * state->ref_spec[k];

  if (++state->count == state->dwell)
    {
      /* On dump, invert once.  When (ny_out,nx_out) > (ny,nx), zero-pad the
       * accumulated spectrum first → band-limited interpolation onto the finer
       * grid; the normalization stays the native 1/n (not 1/n_out) so the
       * interpolated peak equals the native peak.  Native path is unchanged.
       */
      const float complex *src = state->accum;
      if (state->n_out != state->n)
        {
          _zeropad_2d (state, state->accum, state->work_pad);
          src = state->work_pad;
        }
      fft2d_execute_cf32 (state->inv, src, state->n_out, out);
      const float inv_n = 1.0f / (float)state->n;
      for (size_t k = 0; k < state->n_out; k++)
        out[k] *= inv_n;
      memset (state->accum, 0, state->n * sizeof (*state->accum));
      state->count = 0;
      return state->n_out;
    }
  return 0;
}
