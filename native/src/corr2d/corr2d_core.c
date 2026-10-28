#include "doppler/corr2d/corr2d_core.h"

#include "doppler/clib_common.h"
#include "doppler/dp_complex.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

/* 1-D spectral zero-pad: q (length m >= n) is the band-limited (Dirichlet)
 * interpolation of p (length n) — low half [0..n/2] at the front, zeros in the
 * middle, high half at the end, with the Nyquist bin split for even n.
 * m == n is a copy.  Matches scipy.signal.resample to machine precision. */
static void
corr2d_zeropad_1d (const float _Complex *p, size_t n, float _Complex *q,
                   size_t m)
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
 * even-axis Nyquist split is handled per axis by corr2d_zeropad_1d. */
static void
corr2d_zeropad_2d (dp_corr2d_state_t *s, const float _Complex *p,
                   float _Complex *out)
{
  for (size_t i = 0; i < s->ny; i++)
    corr2d_zeropad_1d (p + i * s->nx, s->nx, s->ztmp + i * s->nx_out,
                       s->nx_out);
  for (size_t j = 0; j < s->nx_out; j++)
    {
      for (size_t i = 0; i < s->ny; i++)
        s->zcol[i] = s->ztmp[i * s->nx_out + j];
      corr2d_zeropad_1d (s->zcol, s->ny, s->zcolout, s->ny_out);
      for (size_t i = 0; i < s->ny_out; i++)
        out[i * s->nx_out + j] = s->zcolout[i];
    }
}

/* True iff ref (ny,nx, row-major) is exactly zero outside row 0 — the
 * single-row-reference fast-path precondition (see corr2d_core.h's file
 * doc comment for the identity this licenses).  ny==1 is trivially true. */
static int
corr2d_is_single_row_ref (const float _Complex *ref, size_t ny, size_t nx)
{
  for (size_t i = 1; i < ny; i++)
    for (size_t j = 0; j < nx; j++)
      if (ref[i * nx + j] != 0.0f)
        return 0;
  return 1;
}

/* Columns per slow-time tile.  The column stage reads the (ny, nx) surface
 * down its columns, a stride of nx elements; gathering CORR2D_COL_TILE
 * neighbouring columns per row turns each strided read into one contiguous
 * run of a cache line or more, and the scatter back is the same run per
 * output row -- a tiled transpose.  Measured on the column stage alone at
 * (ny, nx) = (256, 4096), one thread (#2666): a tile of 1 (a plain
 * per-column gather, every read a fresh cache line 32 KiB from the last)
 * 81 ms; 4, 8.0 ms; 8, 3.9 ms; 16, 2.2 ms; 32, 3.6 ms; 64, 6.1 ms.  16 is
 * where the tile's ny_out-deep columns still sit in L1/L2. */
#define CORR2D_COL_TILE 16

/* Rows [r0, r1) of chunk t of nworkers over n items: contiguous, fixed by
 * (t, nworkers) alone, so chunk t always covers the same rows whichever
 * thread drains it. */
static void
corr2d_chunk (size_t n, size_t t, int nworkers, size_t *r0, size_t *r1)
{
  *r0 = t * n / (size_t)nworkers;
  *r1 = (t + 1) * n / (size_t)nworkers;
}

/* Build the per-row machinery the single-row fast path runs on: one
 * forward/inverse plan pair per worker, the conjugated code spectrum, and
 * the pool.  nthreads == 1 is serial (no pool); <= 0 means every online
 * core.  Either way the count is capped at max_chunks, the most chunks any
 * stage can split into -- a worker past that would hold plans and a thread
 * and never get a chunk with work in it -- and a cap of 1 builds no pool.
 * Every size here comes from arguments create() already validated, so the
 * allocations abort on OOM (clib_common.h). */
static void
corr2d_build_fast (dp_corr2d_state_t *state, const float _Complex *code,
                   size_t nx, size_t nxo, int nthreads, size_t max_chunks)
{
  long nt = nthreads;
  if (nt <= 0)
    nt = dp_cpu_count ();
  if (nt < 1)
    nt = 1;
  if ((size_t)nt > max_chunks)
    nt = (long)max_chunks;
  if (nt > 1)
    state->pool = dp_pool_create ((int)nt);
  state->nworkers = dp_pool_threads (state->pool);
  const size_t nw = (size_t)state->nworkers;
  state->row_fwd  = dp_xcalloc (nw, sizeof *state->row_fwd);
  state->row_inv  = dp_xcalloc (nw, sizeof *state->row_inv);
  for (size_t t = 0; t < nw; t++)
    {
      state->row_fwd[t] = dp_xnn (dp_fft_create (nx, -1, 1));
      state->row_inv[t] = dp_xnn (dp_fft_create (nxo, +1, 1));
    }
  /* Worker 0's plans are the ones every serial caller (set_ref, the tests)
     names; they are aliases, freed once through the arrays. */
  state->fwd1d = state->row_fwd[0];
  state->inv1d = state->row_inv[0];

  state->row_ref_spec = dp_xmalloc (nx * sizeof (*state->row_ref_spec));
  /* row_ref_spec = conj(FFT_nx(code)) -- the only row of the reference
   * with energy in it. */
  dp_fft_execute_cf32 (state->fwd1d, code, nx, state->row_ref_spec, nx);
  for (size_t k = 0; k < nx; k++)
    state->row_ref_spec[k] = conjf (state->row_ref_spec[k]);
}

dp_corr2d_state_t *
dp_corr2d_create (const float _Complex *ref, size_t ny, size_t nx,
                  size_t dwell, int nthreads, size_t ny_out, size_t nx_out,
                  int col_out)
{
  dp_corr2d_state_t *state
      = calloc (1, sizeof (*state)); /* NULL-init pointers */
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
  /* The header says dwell "must be >= 1" and nothing enforced it. dwell = 0
     is not a degenerate configuration, it is a black hole: the dump test is
     `++count == dwell`, which a zero dwell never satisfies, so the object
     accumulates every frame it is given and emits nothing until count wraps
     at SIZE_MAX. A caller whose dwell came from a computed value that
     underflowed gets silence and unbounded accumulation rather than an
     error. dp_detector2d_create forwards its own dwell straight here, so
     validating at the primitive covers both objects. */
  if (dwell < 1)
    {
      free (state);
      return NULL;
    }
  int decoupled = (nyo != ny) || (nxo != nx);
  /* Fast path requires ny_out == ny (the row-axis identity only holds for a
   * matched forward/inverse row-transform length — see the header doc
   * comment) and a reference with no energy outside row 0. */
  int fast = (nyo == ny) && corr2d_is_single_row_ref (ref, ny, nx);
  /* A known output column is only defined on the fast path: the general
     2-D inverse mixes rows, so there is no per-row bin to evaluate. A
     caller that knows its lag is correlating against a code replica, which
     is the single-row shape by construction -- so rather than silently
     falling back to the full map (and returning n_out the caller did not
     ask for), refuse the configuration. */
  if (col_out >= 0 && (!fast || nxo != nx || (size_t)col_out >= nx))
    {
      free (state);
      return NULL;
    }

  state->work_fft = malloc (n * sizeof (*state->work_fft));
  state->accum    = calloc (n, sizeof (*state->accum));
  if (!state->work_fft || !state->accum)
    goto fail;

  if (fast)
    {
      /* ref's rows 1..ny-1 are all zero (just checked above), so row 0 is
       * the whole reference. */
      /* The row stage splits into at most ny chunks.  The known-lag path
         (col_out >= 0) never runs the row stage at all, so it gets no
         workers. */
      corr2d_build_fast (state, ref, nx, nxo, nthreads, col_out >= 0 ? 1 : ny);

      if (nxo != nx)
        {
          state->work_pad = malloc (ny * nxo * sizeof (float _Complex));
          if (!state->work_pad)
            goto fail;
        }
    }
  else
    {
      state->fwd = dp_fft2d_create (ny, nx, -1, nthreads);
      state->inv = dp_fft2d_create (nyo, nxo, +1, nthreads);
      if (!state->fwd || !state->inv)
        goto fail;

      state->ref_spec = malloc (n * sizeof (*state->ref_spec));
      if (!state->ref_spec)
        goto fail;

      if (decoupled)
        {
          state->work_pad = malloc (nyo * nxo * sizeof (float _Complex));
          state->ztmp     = malloc (ny * nxo * sizeof (float _Complex));
          state->zcol     = malloc (ny * sizeof (float _Complex));
          state->zcolout  = malloc (nyo * sizeof (float _Complex));
          if (!state->work_pad || !state->ztmp || !state->zcol
              || !state->zcolout)
            goto fail;
        }

      /* Pre-compute conjugate reference spectrum: ref_spec = conj(FFT2(ref)).
       */
      dp_fft2d_execute_cf32 (state->fwd, ref, n, state->ref_spec, n);
      for (size_t k = 0; k < n; k++)
        state->ref_spec[k] = conjf (state->ref_spec[k]);
    }

  if (!fast)
    state->nworkers = 1; /* the general 2-D path runs serially */
  state->fast_path = fast;
  state->ny        = ny;
  state->nx        = nx;
  state->n         = n;
  state->ny_out    = nyo;
  state->nx_out    = nxo;
  state->n_out     = nyo * nxo;
  state->dwell     = dwell;
  state->count     = 0;
  state->col_out   = col_out;

  if (col_out >= 0)
    {
      /* One value per row, not a whole map. */
      state->n_out = ny;
      /* A single known lag needs no transform at ALL, in either direction.
         Expanding the definition,

           R(i,j) = (1/nx) * sum_v X[v]*conj(H[v])*exp(+2i*pi*v*j/nx)
                  = sum_p conj(h[p]) * x[(p + j) mod nx]

         -- the 1/nx cancels against the row-orthogonality sum, leaving a
         plain dot product against the conjugated reference. So this path
         skips the forward FFT too: O(nx) per row against the full map's
         O(nx log nx), which is the whole reason a caller who knows its lag
         would otherwise write the sum out by hand beside this kernel. */
      state->col_ref = dp_xmalloc (nx * sizeof (*state->col_ref));
      for (size_t v = 0; v < nx; v++)
        state->col_ref[v] = conjf (ref[v]);
    }
  return state;

fail:
  dp_corr2d_destroy (state);
  return NULL;
}

void
dp_corr2d_destroy (dp_corr2d_state_t *state)
{
  if (!state)
    return;
  if (state->fwd)
    dp_fft2d_destroy (state->fwd);
  if (state->inv)
    dp_fft2d_destroy (state->inv);
  /* fwd1d / inv1d alias row_fwd[0] / row_inv[0]: freed through the arrays.
   * The pool goes first, so no helper is parked on a plan being freed. */
  dp_pool_destroy (state->pool);
  for (int t = 0; t < state->nworkers; t++)
    {
      if (state->row_fwd && state->row_fwd[t])
        dp_fft_destroy (state->row_fwd[t]);
      if (state->row_inv && state->row_inv[t])
        dp_fft_destroy (state->row_inv[t]);
      if (state->slow_fft && state->slow_fft[t])
        dp_fft_destroy (state->slow_fft[t]);
    }
  free (state->row_fwd);
  free (state->row_inv);
  free (state->slow_fft);
  free (state->col_gather);
  free (state->col_spec);
  free (state->ref_spec);
  free (state->row_ref_spec);
  free (state->col_ref);
  free (state->work_fft);
  free (state->accum);
  free (state->work_pad);
  free (state->work_trunc);
  free (state->ztmp);
  free (state->zcol);
  free (state->zcolout);
  free (state);
}

void
dp_corr2d_reset (dp_corr2d_state_t *state)
{
  memset (state->accum, 0, state->n * sizeof (*state->accum));
  state->count = 0;
}

/* Serializable state — running accumulator (ny*nx) + frame count; the 2-D FFT
 * plans and the reference spectrum are config, recomputed by create().
 *
 * The delay-Doppler flavor carries its own magic, so neither flavor's blob
 * restores into the other: the accumulator means the same thing in both,
 * but a restore across flavors would silently turn a correlation surface
 * into a delay-Doppler map, or back.  Its blob also carries ny and ny_out
 * as reject keys (docs/design/state-serialization.md, "What goes in the
 * blob"): ny because the size check sees only ny*nx, and a (4, 8) blob read
 * as (8, 4) would be reinterpreted rather than refused; ny_out because it
 * fixes the map the dwell in progress dumps into. */
size_t
dp_corr2d_state_bytes (const dp_corr2d_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (uint64_t)
         + (s->delay_doppler ? 2 * sizeof (uint64_t) : 0)
         + s->n * sizeof (float _Complex);
}

void
dp_corr2d_get_state (const dp_corr2d_state_t *s, void *blob)
{
  if (s->delay_doppler)
    {
      DP_GET_OPEN (DELAY_DOPPLER_STATE_MAGIC, DELAY_DOPPLER_STATE_VERSION,
                   dp_corr2d_state_bytes (s));
      dp_w_u64 (&_w, s->ny);
      dp_w_u64 (&_w, s->ny_out);
      dp_w_u64 (&_w, s->count);
      dp_w_cf32 (&_w, s->accum, s->n);
      return;
    }
  DP_GET_OPEN (CORR2D_STATE_MAGIC, CORR2D_STATE_VERSION,
               dp_corr2d_state_bytes (s));
  dp_w_u64 (&_w, s->count);
  dp_w_cf32 (&_w, s->accum, s->n);
}

/* The delay-Doppler flavor's restore: keys and count are decoded and
 * checked before anything is written, so a refused blob changes nothing. */
static int
corr2d_set_state_dd (dp_corr2d_state_t *s, const void *blob)
{
  DP_SET_OPEN (DELAY_DOPPLER_STATE_MAGIC, DELAY_DOPPLER_STATE_VERSION,
               dp_corr2d_state_bytes (s));
  const uint64_t ny     = dp_r_u64 (&_r);
  const uint64_t ny_out = dp_r_u64 (&_r);
  const uint64_t count  = dp_r_u64 (&_r);
  /* A count at or past the dwell would never dump: `count + 1 == dwell`
     is never true again until it wraps. */
  if (ny != s->ny || ny_out != s->ny_out || count >= s->dwell)
    return DP_ERR_INVALID;
  s->count = (size_t)count;
  dp_r_cf32 (&_r, s->accum, s->n);
  return DP_OK;
}

int
dp_corr2d_set_state (dp_corr2d_state_t *s, const void *blob)
{
  if (s->delay_doppler)
    return corr2d_set_state_dd (s, blob);
  DP_SET_OPEN (CORR2D_STATE_MAGIC, CORR2D_STATE_VERSION,
               dp_corr2d_state_bytes (s));
  s->count = (size_t)dp_r_u64 (&_r);
  dp_r_cf32 (&_r, s->accum, s->n);
  return DP_OK;
}

int
dp_corr2d_set_ref (dp_corr2d_state_t *state, const float _Complex *ref)
{
  if (state->fast_path)
    {
      /* Mode is fixed for the object's lifetime (see the header doc
       * comment) — reject rather than silently truncating a ref that no
       * longer fits the single-row assumption row_ref_spec relies on. */
      if (!corr2d_is_single_row_ref (ref, state->ny, state->nx))
        return -1;
      dp_fft_execute_cf32 (state->fwd1d, ref, state->nx, state->row_ref_spec,
                           state->nx);
      for (size_t k = 0; k < state->nx; k++)
        state->row_ref_spec[k] = conjf (state->row_ref_spec[k]);
      /* The known-lag path reads the TIME-domain replica, so refreshing
         only the spectrum would leave it correlating against the old code
         -- silently, and forever. */
      if (state->col_ref)
        for (size_t k = 0; k < state->nx; k++)
          state->col_ref[k] = conjf (ref[k]);
    }
  else
    {
      dp_fft2d_execute_cf32 (state->fwd, ref, state->n, state->ref_spec,
                             state->n);
      for (size_t k = 0; k < state->n; k++)
        state->ref_spec[k] = conjf (state->ref_spec[k]);
    }
  dp_corr2d_reset (state);
  return 0;
}

size_t
dp_corr2d_execute_max_out (dp_corr2d_state_t *state)
{
  return state->n_out;
}

/* One fast-path frame, per-row stage: the work of chunk t (rows r0..r1-1,
 * on worker t's own plans).  Each row is forward-transformed, multiplied by
 * the conjugated code spectrum and added into its accumulator row; on a dump
 * frame the row is then inverted (zero-padded first when nx_out > nx),
 * scaled by 1/nx and its accumulator row cleared.  Rows share nothing, so
 * chunks run concurrently with no synchronization. */
typedef struct
{
  dp_corr2d_state_t    *s;
  const float _Complex *in;   /**< the frame, ny rows of nx              */
  float _Complex       *dst;  /**< dump destination, ny rows of nx_out   */
  int                   dump; /**< 1 on the frame that completes a dwell */
} corr2d_rows_t;

static void
corr2d_rows_body (size_t t, void *ctx)
{
  const corr2d_rows_t *c  = (const corr2d_rows_t *)ctx;
  dp_corr2d_state_t   *s  = c->s;
  const size_t         nx = s->nx, nxo = s->nx_out;
  const float          inv_nx = 1.0f / (float)nx;
  size_t               r0, r1;
  corr2d_chunk (s->ny, t, s->nworkers, &r0, &r1);
  for (size_t i = r0; i < r1; i++)
    {
      float _Complex *w = s->work_fft + i * nx;
      float _Complex *a = s->accum + i * nx;
      dp_fft_execute_cf32 (s->row_fwd[t], c->in + i * nx, nx, w, nx);
      for (size_t v = 0; v < nx; v++)
        a[v] += w[v] * s->row_ref_spec[v];
      if (!c->dump)
        continue;
      float _Complex *d = c->dst + i * nxo;
      if (nxo == nx)
        dp_fft_execute_cf32 (s->row_inv[t], a, nx, d, nx);
      else
        {
          float _Complex *pad = s->work_pad + i * nxo;
          corr2d_zeropad_1d (a, nx, pad, nxo);
          dp_fft_execute_cf32 (s->row_inv[t], pad, nxo, d, nxo);
        }
      for (size_t k = 0; k < nxo; k++)
        d[k] *= inv_nx;
      memset (a, 0, nx * sizeof *a);
    }
}

/* Delay-Doppler dump, per-column stage: the slow-time FFT down every delay
 * column of the (ny, nx) correlation surface, chunk t covering tiles
 * b0..b1-1 of CORR2D_COL_TILE columns on worker t's plan and scratch.
 *
 * A tile is gathered into contiguous columns of length ny_out, whose rows
 * ny..ny_out-1 were zeroed at create() and are never written: that trailing
 * zero-pad of the slow-time samples is what interpolates the Doppler axis
 * (the ny_out-point DFT of ny samples samples their DTFT on a grid ny_out/ny
 * times finer).  Each column is transformed unnormalized, as numpy's fft is,
 * and scattered back FFT-shifted: Doppler bin k lands on output row
 * (k + ny_out/2) mod ny_out, so zero Doppler is row ny_out/2. */
typedef struct
{
  dp_corr2d_state_t    *s;
  const float _Complex *surf; /**< the (ny, nx) correlation surface */
  float _Complex       *out;  /**< the (ny_out, nx) map             */
} corr2d_cols_t;

static void
corr2d_cols_body (size_t t, void *ctx)
{
  const corr2d_cols_t *c  = (const corr2d_cols_t *)ctx;
  dp_corr2d_state_t   *s  = c->s;
  const size_t         ny = s->ny, nx = s->nx, nyo = s->ny_out, half = nyo / 2;
  const size_t         ntiles = (nx + CORR2D_COL_TILE - 1) / CORR2D_COL_TILE;
  float _Complex      *g      = s->col_gather + t * CORR2D_COL_TILE * nyo;
  float _Complex      *o      = s->col_spec + t * CORR2D_COL_TILE * nyo;
  size_t               b0, b1;
  corr2d_chunk (ntiles, t, s->nworkers, &b0, &b1);
  for (size_t b = b0; b < b1; b++)
    {
      const size_t j0 = b * CORR2D_COL_TILE;
      const size_t nb
          = (nx - j0 < CORR2D_COL_TILE) ? nx - j0 : CORR2D_COL_TILE;
      for (size_t i = 0; i < ny; i++)
        {
          const float _Complex *src = c->surf + i * nx + j0;
          for (size_t q = 0; q < nb; q++)
            g[q * nyo + i] = src[q];
        }
      for (size_t q = 0; q < nb; q++)
        dp_fft_execute_cf32 (s->slow_fft[t], g + q * nyo, nyo, o + q * nyo,
                             nyo);
      for (size_t k = 0; k < nyo; k++)
        {
          float _Complex *dst = c->out + ((k + half) % nyo) * nx + j0;
          for (size_t q = 0; q < nb; q++)
            dst[q] = o[q * nyo + k];
        }
    }
}

/* Fast path: ref is single-row and ny_out == ny, so (see the header doc
 * comment for the full derivation) the row axis of the 2-D transform pair
 * cancels to an exact identity and dp_corr2d_execute reduces, per row i, to
 *
 *   R(i,j) = IFFT_nx( FFT_nx(row_i) · conj(FFT_nx(ref_row0)) )(j) / nx
 *
 * — ny independent length-nx circular cross-correlations, normalized by
 * 1/nx (NOT 1/n = ny*nx: the row-axis orthogonality sum contributes the
 * extra factor of ny that turns 1/n into 1/nx — see the derivation).
 *
 * The rows are independent, so they run as nworkers chunks on the pool
 * (corr2d_rows_body); the delay-Doppler flavor then runs its slow-time
 * column stage the same way (corr2d_cols_body). */
static size_t
corr2d_execute_fast (dp_corr2d_state_t *state, const float _Complex *in,
                     float _Complex *out)
{
  const size_t ny = state->ny, nx = state->nx;

  if (state->col_out >= 0)
    {
      /* Known lag: correlate in the time domain, no transform either way.
         The running sum lives in the first ny entries of `accum` so the
         serialized state keeps covering it -- a dwell interrupted mid-way
         still resumes, exactly as the full path's product spectrum does. */
      const size_t j0 = (size_t)state->col_out;
      for (size_t i = 0; i < ny; i++)
        {
          const float _Complex *x = in + i * nx;
          float _Complex acc      = 0.0f;
          for (size_t p = 0; p < nx; p++)
            acc += state->col_ref[p] * x[(p + j0) % nx];
          state->accum[i] += acc;
        }
      if (++state->count == state->dwell)
        {
          for (size_t i = 0; i < ny; i++)
            out[i] = state->accum[i];
          memset (state->accum, 0, state->n * sizeof (*state->accum));
          state->count = 0;
          return state->n_out;
        }
      return 0;
    }

  /* Dump decided up front: the per-row stage inverts each row as soon as
     it is accumulated, rather than in a second pass over the surface. */
  const int dump = (state->count + 1 == state->dwell);
  /* The delay-Doppler flavor inverts into work_fft (each row's forward
     spectrum is spent once it is accumulated) and the column stage writes
     `out`; Corr2D inverts straight into `out`. */
  corr2d_rows_t rows
      = { state, in, state->delay_doppler ? state->work_fft : out, dump };
  dp_pool_run (state->pool, (size_t)state->nworkers, corr2d_rows_body, &rows);
  if (!dump)
    {
      state->count++;
      return 0;
    }
  if (state->delay_doppler)
    {
      corr2d_cols_t cols = { state, state->work_fft, out };
      dp_pool_run (state->pool, (size_t)state->nworkers, corr2d_cols_body,
                   &cols);
    }
  state->count = 0;
  return state->n_out;
}

size_t
dp_corr2d_execute (dp_corr2d_state_t *state, const float _Complex *in,
                   size_t n_in, float _Complex *out, size_t max_out)
{
  /* n_in must equal ny*nx; refuse before touching the accumulator, so a
   * refused call is not counted toward the dwell (#1925, as dp_corr_execute).
   */
  if (n_in != state->n)
    return SIZE_MAX;

  /* Emission stops at the caller's capacity (jm gh-138). Neither path can
   * serve a short buffer by writing less -- the 2-D inverse plan is fixed at
   * n_out, and the fast path writes ny whole rows -- so a short `out` is
   * served by writing the full surface into a bounce buffer and copying the
   * prefix. Decided here, once, because BOTH paths write through `dst`; the
   * fast path is a separate write site and a clamp applied only to the slow
   * one would leave it unguarded. */
  float _Complex *dst = out;
  size_t          cap = state->n_out;
  if (max_out < state->n_out)
    {
      if (!state->work_trunc)
        state->work_trunc = (float _Complex *)dp_xcalloc (
            state->n_out, sizeof *state->work_trunc);
      dst = state->work_trunc;
      cap = max_out;
    }

  if (state->fast_path)
    {
      size_t n = corr2d_execute_fast (state, in, dst);
      if (n && dst != out)
        {
          memcpy (out, dst, cap * sizeof *out);
          n = cap;
        }
      return n;
    }

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
  dp_fft2d_execute_cf32 (state->fwd, in, state->n, state->work_fft, state->n);

  for (size_t k = 0; k < state->n; k++)
    state->accum[k] += state->work_fft[k] * state->ref_spec[k];

  if (++state->count == state->dwell)
    {
      /* On dump, invert once.  When (ny_out,nx_out) > (ny,nx), zero-pad the
       * accumulated spectrum first → band-limited interpolation onto the finer
       * grid; the normalization stays the native 1/n (not 1/n_out) so the
       * interpolated peak equals the native peak.  Native path is unchanged.
       */
      const float _Complex *src = state->accum;
      if (state->n_out != state->n)
        {
          corr2d_zeropad_2d (state, state->accum, state->work_pad);
          src = state->work_pad;
        }
      dp_fft2d_execute_cf32 (state->inv, src, state->n_out, dst, state->n_out);
      const float inv_n = 1.0f / (float)state->n;
      for (size_t k = 0; k < state->n_out; k++)
        dst[k] *= inv_n;
      if (dst != out)
        memcpy (out, dst, cap * sizeof *out);
      memset (state->accum, 0, state->n * sizeof (*state->accum));
      state->count = 0;
      return cap;
    }
  return 0;
}

dp_corr2d_state_t *
dp_corr2d_create_delay_doppler (const float _Complex *code, size_t code_len,
                                size_t ny, size_t dwell, int nthreads,
                                size_t ny_out)
{
  const size_t nx  = code_len;
  const size_t nyo = ny_out ? ny_out : ny;
  if (!code || nx == 0 || ny == 0 || dwell < 1 || nyo < ny)
    return NULL;

  /* Every size below follows from the arguments just validated, so the
     allocations abort on OOM rather than unwind (clib_common.h). */
  dp_corr2d_state_t *state = dp_xcalloc (1, sizeof (*state));
  const size_t       n     = ny * nx;
  state->work_fft          = dp_xmalloc (n * sizeof (*state->work_fft));
  state->accum             = dp_xcalloc (n, sizeof (*state->accum));
  /* The delay axis is native (nx_out == nx), so the row stage is exactly
     Corr2D's fast path over a reference whose row 0 is the code. */
  /* Rows split into at most ny chunks, columns into at most one per tile:
     the larger is the most workers either stage can keep busy. */
  const size_t ntiles = (nx + CORR2D_COL_TILE - 1) / CORR2D_COL_TILE;
  corr2d_build_fast (state, code, nx, nx, nthreads, ny > ntiles ? ny : ntiles);

  const size_t nw = (size_t)state->nworkers;
  state->slow_fft = dp_xcalloc (nw, sizeof *state->slow_fft);
  for (size_t t = 0; t < nw; t++)
    state->slow_fft[t] = dp_xnn (dp_fft_create (nyo, -1, 1));
  /* calloc, not malloc: rows ny..ny_out-1 of every gathered column are the
     slow-time zero-pad, written here once and never again. */
  state->col_gather
      = dp_xcalloc (nw * CORR2D_COL_TILE * nyo, sizeof (float _Complex));
  state->col_spec
      = dp_xmalloc (nw * CORR2D_COL_TILE * nyo * sizeof (float _Complex));

  state->delay_doppler = 1;
  state->fast_path     = 1;
  state->ny            = ny;
  state->nx            = nx;
  state->n             = n;
  state->ny_out        = nyo;
  state->nx_out        = nx;
  state->n_out         = nyo * nx;
  state->dwell         = dwell;
  state->count         = 0;
  state->col_out       = -1;
  state->doppler_step  = 1.0 / ((double)nyo * (double)nx);
  return state;
}
