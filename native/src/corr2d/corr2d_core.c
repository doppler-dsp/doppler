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

/* True iff ref (ny,nx, row-major) is exactly zero outside row 0 — the
 * single-row-reference fast-path precondition (see corr2d_core.h's file
 * doc comment for the identity this licenses).  ny==1 is trivially true. */
static int
_is_single_row_ref (const float complex *ref, size_t ny, size_t nx)
{
  for (size_t i = 1; i < ny; i++)
    for (size_t j = 0; j < nx; j++)
      if (ref[i * nx + j] != 0.0f)
        return 0;
  return 1;
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
  /* Fast path requires ny_out == ny (the row-axis identity only holds for a
   * matched forward/inverse row-transform length — see the header doc
   * comment) and a reference with no energy outside row 0. */
  int fast = (nyo == ny) && _is_single_row_ref (ref, ny, nx);

  state->work_fft = malloc (n * sizeof (*state->work_fft));
  state->accum    = calloc (n, sizeof (*state->accum));
  if (!state->work_fft || !state->accum)
    goto fail;

  if (fast)
    {
      state->fwd1d = fft_create (nx, -1, nthreads);
      state->inv1d = fft_create (nxo, +1, nthreads);
      if (!state->fwd1d || !state->inv1d)
        goto fail;

      state->row_ref_spec = malloc (nx * sizeof (*state->row_ref_spec));
      if (!state->row_ref_spec)
        goto fail;
      /* row_ref_spec = conj(FFT_nx(ref row 0)) — ref's rows 1..ny-1 are all
       * zero (just checked above), so only row 0 need be transformed. */
      fft_execute_cf32 (state->fwd1d, ref, nx, state->row_ref_spec);
      for (size_t k = 0; k < nx; k++)
        state->row_ref_spec[k] = conjf (state->row_ref_spec[k]);

      if (nxo != nx)
        {
          state->work_pad = malloc (ny * nxo * sizeof (float complex));
          if (!state->work_pad)
            goto fail;
        }
    }
  else
    {
      state->fwd = fft2d_create (ny, nx, -1, nthreads);
      state->inv = fft2d_create (nyo, nxo, +1, nthreads);
      if (!state->fwd || !state->inv)
        goto fail;

      state->ref_spec = malloc (n * sizeof (*state->ref_spec));
      if (!state->ref_spec)
        goto fail;

      if (decoupled)
        {
          state->work_pad = malloc (nyo * nxo * sizeof (float complex));
          state->ztmp     = malloc (ny * nxo * sizeof (float complex));
          state->zcol     = malloc (ny * sizeof (float complex));
          state->zcolout  = malloc (nyo * sizeof (float complex));
          if (!state->work_pad || !state->ztmp || !state->zcol
              || !state->zcolout)
            goto fail;
        }

      /* Pre-compute conjugate reference spectrum: ref_spec = conj(FFT2(ref)).
       */
      fft2d_execute_cf32 (state->fwd, ref, n, state->ref_spec);
      for (size_t k = 0; k < n; k++)
        state->ref_spec[k] = conjf (state->ref_spec[k]);
    }

  state->fast_path = fast;
  state->ny        = ny;
  state->nx        = nx;
  state->n         = n;
  state->ny_out    = nyo;
  state->nx_out    = nxo;
  state->n_out     = nyo * nxo;
  state->dwell     = dwell;
  state->count     = 0;
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
  if (state->fwd1d)
    fft_destroy (state->fwd1d);
  if (state->inv1d)
    fft_destroy (state->inv1d);
  free (state->ref_spec);
  free (state->row_ref_spec);
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

/* Serializable state — running accumulator (ny*nx) + frame count; the 2-D FFT
 * plans and the reference spectrum are config, recomputed by create(). */
size_t
corr2d_state_bytes (const corr2d_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (uint64_t)
         + s->n * sizeof (float _Complex);
}

void
corr2d_get_state (const corr2d_state_t *s, void *blob)
{
  DP_GET_OPEN (CORR2D_STATE_MAGIC, CORR2D_STATE_VERSION,
               corr2d_state_bytes (s));
  dp_w_u64 (&_w, s->count);
  dp_w_cf32 (&_w, s->accum, s->n);
}

int
corr2d_set_state (corr2d_state_t *s, const void *blob)
{
  DP_SET_OPEN (CORR2D_STATE_MAGIC, CORR2D_STATE_VERSION,
               corr2d_state_bytes (s));
  s->count = (size_t)dp_r_u64 (&_r);
  dp_r_cf32 (&_r, s->accum, s->n);
  return DP_OK;
}

int
corr2d_set_ref (corr2d_state_t *state, const float complex *ref)
{
  if (state->fast_path)
    {
      /* Mode is fixed for the object's lifetime (see the header doc
       * comment) — reject rather than silently truncating a ref that no
       * longer fits the single-row assumption row_ref_spec relies on. */
      if (!_is_single_row_ref (ref, state->ny, state->nx))
        return -1;
      fft_execute_cf32 (state->fwd1d, ref, state->nx, state->row_ref_spec);
      for (size_t k = 0; k < state->nx; k++)
        state->row_ref_spec[k] = conjf (state->row_ref_spec[k]);
    }
  else
    {
      fft2d_execute_cf32 (state->fwd, ref, state->n, state->ref_spec);
      for (size_t k = 0; k < state->n; k++)
        state->ref_spec[k] = conjf (state->ref_spec[k]);
    }
  corr2d_reset (state);
  return 0;
}

size_t
corr2d_execute_max_out (corr2d_state_t *state)
{
  return state->n_out;
}

/* Fast path: ref is single-row and ny_out == ny, so (see the header doc
 * comment for the full derivation) the row axis of the 2-D transform pair
 * cancels to an exact identity and corr2d_execute reduces, per row i, to
 *
 *   R(i,j) = IFFT_nx( FFT_nx(row_i) · conj(FFT_nx(ref_row0)) )(j) / nx
 *
 * — ny independent length-nx circular cross-correlations, normalized by
 * 1/nx (NOT 1/n = ny*nx: the row-axis orthogonality sum contributes the
 * extra factor of ny that turns 1/n into 1/nx — see the derivation). */
static size_t
_execute_fast (corr2d_state_t *state, const float complex *in,
               float complex *out)
{
  const size_t ny = state->ny, nx = state->nx, nxo = state->nx_out;

  for (size_t i = 0; i < ny; i++)
    fft_execute_cf32 (state->fwd1d, in + i * nx, nx, state->work_fft + i * nx);

  for (size_t i = 0; i < ny; i++)
    for (size_t v = 0; v < nx; v++)
      state->accum[i * nx + v]
          += state->work_fft[i * nx + v] * state->row_ref_spec[v];

  if (++state->count == state->dwell)
    {
      const float inv_nx = 1.0f / (float)nx;
      if (nxo == nx)
        {
          for (size_t i = 0; i < ny; i++)
            fft_execute_cf32 (state->inv1d, state->accum + i * nx, nx,
                              out + i * nx);
        }
      else
        {
          for (size_t i = 0; i < ny; i++)
            _zeropad_1d (state->accum + i * nx, nx, state->work_pad + i * nxo,
                         nxo);
          for (size_t i = 0; i < ny; i++)
            fft_execute_cf32 (state->inv1d, state->work_pad + i * nxo, nxo,
                              out + i * nxo);
        }
      for (size_t k = 0; k < state->n_out; k++)
        out[k] *= inv_nx;
      memset (state->accum, 0, state->n * sizeof (*state->accum));
      state->count = 0;
      return state->n_out;
    }
  return 0;
}

size_t
corr2d_execute (corr2d_state_t *state, const float complex *in, size_t n_in,
                float complex *out)
{
  (void)n_in;

  if (state->fast_path)
    return _execute_fast (state, in, out);

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
