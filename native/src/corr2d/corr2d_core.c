#include "doppler/corr2d/corr2d_core.h"

#include "doppler/clib_common.h"
#include "doppler/dp_complex.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
corr2d_zeropad_2d (const dp_corr2d_state_t *s, const float _Complex *p,
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

/* Tiles of CORR2D_COL_TILE columns across nx delay columns. */
static size_t
corr2d_ntiles (size_t nx)
{
  return (nx + CORR2D_COL_TILE - 1) / CORR2D_COL_TILE;
}

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
  free (state->bat_acc);
  free (state->bat_work);
  free (state->bat_pad);
  free (state->bat_map);
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

/* One fast-path row, on worker t's plans: forward-transform the period x
 * into w, multiply by the conjugated code spectrum and add into the
 * accumulator row a; on a dump, invert a (zero-padded into pad first when
 * nx_out > nx) into d, scale by 1/nx and clear a.  d may alias w: the
 * forward spectrum is spent once it is accumulated.  Every map the object
 * makes, from execute() or a batch, goes through here, so a batch map is
 * the same arithmetic as a dump. */
/* A row's product spectrum a back to the delay domain, on worker t's
 * inverse plan: zero-padded into pad first when nx_out > nx, inverted into d,
 * scaled by 1/nx.  The one inverse every fast-path map is made with: a dump
 * (corr2d_row) and a bank hypothesis (corr2d_bank_rows_body). */
static void
corr2d_row_inverse (const dp_corr2d_state_t *s, size_t t,
                    const float _Complex *a, float _Complex *pad,
                    float _Complex *d)
{
  const size_t nx = s->nx, nxo = s->nx_out;
  if (nxo == nx)
    dp_fft_execute_cf32 (s->row_inv[t], a, nx, d, nx);
  else
    {
      corr2d_zeropad_1d (a, nx, pad, nxo);
      dp_fft_execute_cf32 (s->row_inv[t], pad, nxo, d, nxo);
    }
  const float inv_nx = 1.0f / (float)nx;
  for (size_t k = 0; k < nxo; k++)
    d[k] *= inv_nx;
}

static void
corr2d_row (const dp_corr2d_state_t *s, size_t t, const float _Complex *x,
            float _Complex *a, float _Complex *w, float _Complex *pad,
            float _Complex *d, int dump)
{
  const size_t nx = s->nx;
  dp_fft_execute_cf32 (s->row_fwd[t], x, nx, w, nx);
  for (size_t v = 0; v < nx; v++)
    a[v] += w[v] * s->row_ref_spec[v];
  if (!dump)
    return;
  corr2d_row_inverse (s, t, a, pad, d);
  memset (a, 0, nx * sizeof *a);
}

/* One fast-path frame, per-row stage: chunk t runs rows r0..r1-1 through
 * corr2d_row on worker t's plans.  The accumulator, spectrum and pad
 * surfaces are named here rather than read from the state, so execute()
 * runs the dwell's own and a batch runs its scratch.  Rows share nothing,
 * so chunks run concurrently with no synchronization. */
typedef struct
{
  dp_corr2d_state_t    *s;
  const float _Complex *in;   /**< the frame, ny rows of nx              */
  float _Complex       *acc;  /**< accumulator, ny rows of nx            */
  float _Complex       *work; /**< forward spectra, ny rows of nx        */
  float _Complex       *pad;  /**< zero-pad rows of nx_out; NULL native  */
  float _Complex       *dst;  /**< dump destination, ny rows of nx_out   */
  int                   dump; /**< 1 on the frame that completes a dwell */
} corr2d_rows_t;

static void
corr2d_rows_body (size_t t, void *ctx)
{
  const corr2d_rows_t *c  = (const corr2d_rows_t *)ctx;
  dp_corr2d_state_t   *s  = c->s;
  const size_t         nx = s->nx, nxo = s->nx_out;
  size_t               r0, r1;
  corr2d_chunk (s->ny, t, s->nworkers, &r0, &r1);
  for (size_t i = r0; i < r1; i++)
    corr2d_row (s, t, c->in + i * nx, c->acc + i * nx, c->work + i * nx,
                c->pad ? c->pad + i * nxo : NULL, c->dst + i * nxo, c->dump);
}

/* Delay-Doppler dump, per-column stage: the slow-time FFT down every delay
 * column of the (ny, nx) correlation surface.  corr2d_col_tile does tile b
 * of CORR2D_COL_TILE columns on worker t's plan and scratch;
 * corr2d_cols_body runs chunk t's tiles b0..b1-1.
 *
 * A tile is gathered into contiguous columns of length ny_out, whose rows
 * ny..ny_out-1 were zeroed at create() and are never written: that trailing
 * zero-pad of the slow-time samples is what interpolates the Doppler axis
 * (the ny_out-point DFT of ny samples samples their DTFT on a grid ny_out/ny
 * times finer).  Each column is transformed unnormalized, as numpy's fft is,
 * and scattered back FFT-shifted: Doppler bin k lands on output row
 * (k + ny_out/2) mod ny_out, so zero Doppler is row ny_out/2. */
static void
corr2d_col_tile (const dp_corr2d_state_t *s, size_t t,
                 const float _Complex *surf, float _Complex *out, size_t b,
                 const float _Complex *w)
{
  const size_t    ny = s->ny, nx = s->nx, nyo = s->ny_out, half = nyo / 2;
  float _Complex *g  = s->col_gather + t * CORR2D_COL_TILE * nyo;
  float _Complex *o  = s->col_spec + t * CORR2D_COL_TILE * nyo;
  const size_t    j0 = b * CORR2D_COL_TILE;
  const size_t    nb = (nx - j0 < CORR2D_COL_TILE) ? nx - j0 : CORR2D_COL_TILE;
  /* w, when given, weights slow-time sample i (a bank's rate hypothesis);
     NULL multiplies nothing, so execute() is unchanged bit for bit. */
  for (size_t i = 0; i < ny; i++)
    {
      const float _Complex *src = surf + i * nx + j0;
      if (w)
        for (size_t q = 0; q < nb; q++)
          g[q * nyo + i] = src[q] * w[i];
      else
        for (size_t q = 0; q < nb; q++)
          g[q * nyo + i] = src[q];
    }
  for (size_t q = 0; q < nb; q++)
    dp_fft_execute_cf32 (s->slow_fft[t], g + q * nyo, nyo, o + q * nyo, nyo);
  for (size_t k = 0; k < nyo; k++)
    {
      float _Complex *dst = out + ((k + half) % nyo) * nx + j0;
      for (size_t q = 0; q < nb; q++)
        dst[q] = o[q * nyo + k];
    }
}

typedef struct
{
  dp_corr2d_state_t    *s;
  const float _Complex *surf; /**< the (ny, nx) correlation surface */
  float _Complex       *out;  /**< the (ny_out, nx) map             */
} corr2d_cols_t;

static void
corr2d_cols_body (size_t t, void *ctx)
{
  const corr2d_cols_t *c      = (const corr2d_cols_t *)ctx;
  const size_t         ntiles = corr2d_ntiles (c->s->nx);
  size_t               b0, b1;
  corr2d_chunk (ntiles, t, c->s->nworkers, &b0, &b1);
  for (size_t b = b0; b < b1; b++)
    corr2d_col_tile (c->s, t, c->surf, c->out, b, NULL);
}

/* Known-lag frame (col_out >= 0): row i's correlation at the one lag, a
 * time-domain dot product against the conjugated code, added into acc[i];
 * on a dump, acc's first ny entries go to dst and acc is cleared (all n of
 * it, as the state blob covers all n). */
static void
corr2d_col_frame (const dp_corr2d_state_t *s, const float _Complex *in,
                  float _Complex *acc, float _Complex *dst, int dump)
{
  const size_t ny = s->ny, nx = s->nx, j0 = (size_t)s->col_out;
  for (size_t i = 0; i < ny; i++)
    {
      const float _Complex *x = in + i * nx;
      float _Complex sum      = 0.0f;
      for (size_t p = 0; p < nx; p++)
        sum += s->col_ref[p] * x[(p + j0) % nx];
      acc[i] += sum;
    }
  if (!dump)
    return;
  for (size_t i = 0; i < ny; i++)
    dst[i] = acc[i];
  memset (acc, 0, s->n * sizeof *acc);
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
  if (state->col_out >= 0)
    {
      /* Known lag: correlate in the time domain, no transform either way.
         The running sum lives in the first ny entries of `accum` so the
         serialized state keeps covering it -- a dwell interrupted mid-way
         still resumes, exactly as the full path's product spectrum does. */
      const int dump = (++state->count == state->dwell);
      corr2d_col_frame (state, in, state->accum, out, dump);
      if (!dump)
        return 0;
      state->count = 0;
      return state->n_out;
    }

  /* Dump decided up front: the per-row stage inverts each row as soon as
     it is accumulated, rather than in a second pass over the surface. */
  const int dump = (state->count + 1 == state->dwell);
  /* The delay-Doppler flavor inverts into work_fft (each row's forward
     spectrum is spent once it is accumulated) and the column stage writes
     `out`; Corr2D inverts straight into `out`. */
  corr2d_rows_t rows = { state,
                         in,
                         state->accum,
                         state->work_fft,
                         state->work_pad,
                         state->delay_doppler ? state->work_fft : out,
                         dump };
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

/* One general-path (2-D) frame: FFT2 into work_fft, multiply by the
 * reference spectrum and add into acc.  On a dump, invert once: when
 * (ny_out, nx_out) > (ny, nx) the accumulated spectrum is zero-padded first
 * (band-limited interpolation onto the finer grid), and the normalization
 * stays the native 1/n (not 1/n_out) so the interpolated peak equals the
 * native peak.  acc is then cleared.  work_fft, work_pad and the zero-pad
 * scratch are scratch, not state, so a batch shares them; it brings its own
 * acc. */
static void
corr2d_general_frame (const dp_corr2d_state_t *s, const float _Complex *in,
                      float _Complex *acc, float _Complex *dst, int dump)
{
  dp_fft2d_execute_cf32 (s->fwd, in, s->n, s->work_fft, s->n);
  for (size_t k = 0; k < s->n; k++)
    acc[k] += s->work_fft[k] * s->ref_spec[k];
  if (!dump)
    return;
  const float _Complex *src = acc;
  if (s->n_out != s->n)
    {
      corr2d_zeropad_2d (s, acc, s->work_pad);
      src = s->work_pad;
    }
  dp_fft2d_execute_cf32 (s->inv, src, s->n_out, dst, s->n_out);
  const float inv_n = 1.0f / (float)s->n;
  for (size_t k = 0; k < s->n_out; k++)
    dst[k] *= inv_n;
  memset (acc, 0, s->n * sizeof *acc);
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
  const int dump = (++state->count == state->dwell);
  corr2d_general_frame (state, in, state->accum, dst, dump);
  if (!dump)
    return 0;
  if (dst != out)
    memcpy (out, dst, cap * sizeof *out);
  state->count = 0;
  return cap;
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
  const size_t ntiles = corr2d_ntiles (nx);
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

/* ── Batches (#2683) ───────────────────────────────────────────────────────
 */

/* Frames in a batch of n_in samples, or 0 when it is empty or ragged. */
static size_t
corr2d_frames (const dp_corr2d_state_t *s, size_t n_in)
{
  return (n_in % s->n) ? 0 : n_in / s->n;
}

/* The batch scratch: one slot per worker, made on the first batch.  The
   sizes are the object's own, already allocated once by create(), so the
   allocations abort on OOM (clib_common.h).  bat_acc is calloc'd: every
   frame helper leaves its accumulator zero again after a dump, which is
   what makes each map start from the zero a fresh object starts from. */
static void
corr2d_batch_alloc (dp_corr2d_state_t *s)
{
  if (s->bat_acc)
    return;
  const size_t nw = (size_t)s->nworkers;
  s->bat_acc      = dp_xcalloc (nw * s->n, sizeof *s->bat_acc);
  s->bat_work     = dp_xmalloc (nw * s->n * sizeof *s->bat_work);
  s->bat_map      = dp_xmalloc (nw * s->n_out * sizeof *s->bat_map);
  if (s->fast_path && s->nx_out != s->nx)
    s->bat_pad = dp_xmalloc (nw * s->ny * s->nx_out * sizeof *s->bat_pad);
}

/* One frame into slot 0 of the batch scratch, split across the workers as
   execute() splits it: the general and known-lag paths run serially, the
   fast path's rows (and, on a dump, the delay-Doppler columns) on the
   pool.  dst receives the map when dump is set. */
static void
corr2d_batch_frame (dp_corr2d_state_t *s, const float _Complex *x,
                    float _Complex *dst, int dump)
{
  if (!s->fast_path)
    {
      corr2d_general_frame (s, x, s->bat_acc, dst, dump);
      return;
    }
  if (s->col_out >= 0)
    {
      corr2d_col_frame (s, x, s->bat_acc, dst, dump);
      return;
    }
  corr2d_rows_t rows
      = { s,           x,          s->bat_acc,
          s->bat_work, s->bat_pad, s->delay_doppler ? s->bat_work : dst,
          dump };
  dp_pool_run (s->pool, (size_t)s->nworkers, corr2d_rows_body, &rows);
  if (dump && s->delay_doppler)
    {
      corr2d_cols_t cols = { s, s->bat_work, dst };
      dp_pool_run (s->pool, (size_t)s->nworkers, corr2d_cols_body, &cols);
    }
}

/* Whole maps, one per worker: chunk t makes the map of frame h0 + t on
   worker t's plans and scratch slot t, every row and then every column
   tile, serially.  A row or tile is the same arithmetic on any worker's
   plans, so the map equals the split one bit for bit. */
typedef struct
{
  dp_corr2d_state_t    *s;
  const float _Complex *in; /**< the batch                 */
  size_t                h0; /**< frame of chunk 0          */
} corr2d_whole_t;

static void
corr2d_whole_body (size_t t, void *ctx)
{
  const corr2d_whole_t *c  = (const corr2d_whole_t *)ctx;
  dp_corr2d_state_t    *s  = c->s;
  const size_t          nx = s->nx, nxo = s->nx_out, n = s->n;
  const float _Complex *x    = c->in + (c->h0 + t) * n;
  float _Complex       *acc  = s->bat_acc + t * n;
  float _Complex       *work = s->bat_work + t * n;
  float _Complex       *pad = s->bat_pad ? s->bat_pad + t * s->ny * nxo : NULL;
  float _Complex       *map = s->bat_map + t * s->n_out;
  float _Complex       *surf = s->delay_doppler ? work : map;
  for (size_t i = 0; i < s->ny; i++)
    corr2d_row (s, t, x + i * nx, acc + i * nx, work + i * nx,
                pad ? pad + i * nxo : NULL, surf + i * nxo, 1);
  if (s->delay_doppler)
    for (size_t b = 0; b < corr2d_ntiles (nx); b++)
      corr2d_col_tile (s, t, work, map, b, NULL);
}

/* The coherent sum on the fast path, one dispatch for the whole batch:
   chunk t owns rows r0..r1-1 and takes each of them through every frame in
   h order, dumping on the last.  A row's accumulator sees the frames in the
   order a dwell would, so the sum is a fresh dwell-H object's bit for bit,
   and a short frame is not split once per frame. */
typedef struct
{
  dp_corr2d_state_t    *s;
  const float _Complex *in;  /**< the batch                       */
  size_t                H;   /**< frames                          */
  float _Complex       *dst; /**< the surface: map, or bat_work   */
} corr2d_coh_t;

static void
corr2d_coh_body (size_t t, void *ctx)
{
  const corr2d_coh_t *c  = (const corr2d_coh_t *)ctx;
  dp_corr2d_state_t  *s  = c->s;
  const size_t        nx = s->nx, nxo = s->nx_out;
  size_t              r0, r1;
  corr2d_chunk (s->ny, t, s->nworkers, &r0, &r1);
  for (size_t h = 0; h < c->H; h++)
    for (size_t i = r0; i < r1; i++)
      corr2d_row (s, t, c->in + h * s->n + i * nx, s->bat_acc + i * nx,
                  s->bat_work + i * nx,
                  s->bat_pad ? s->bat_pad + i * nxo : NULL, c->dst + i * nxo,
                  h + 1 == c->H);
}

/* The one batch loop.  coherent = 0 makes every frame's map and folds each
   in h order; coherent = 1 sums the frames' product spectra and folds the
   one map it inverts, as h = 0.  The dwell is never read or written. */
static size_t
corr2d_batch (dp_corr2d_state_t *s, const float _Complex *in, size_t n_in,
              int coherent, const dp_corr2d_fold_t *fold)
{
  const size_t H = corr2d_frames (s, n_in);
  if (H == 0)
    return SIZE_MAX;
  corr2d_batch_alloc (s);
  const size_t n = s->n, nw = (size_t)s->nworkers;
  const int    split = s->fast_path && s->col_out < 0;
  if (coherent && split)
    {
      corr2d_coh_t c
          = { s, in, H, s->delay_doppler ? s->bat_work : s->bat_map };
      dp_pool_run (s->pool, nw, corr2d_coh_body, &c);
      if (s->delay_doppler)
        {
          corr2d_cols_t cols = { s, s->bat_work, s->bat_map };
          dp_pool_run (s->pool, nw, corr2d_cols_body, &cols);
        }
      fold->fold (fold->ctx, 0, s->bat_map, s->n_out);
      return H;
    }
  if (coherent)
    {
      for (size_t h = 0; h < H; h++)
        corr2d_batch_frame (s, in + h * n, s->bat_map, h + 1 == H);
      fold->fold (fold->ctx, 0, s->bat_map, s->n_out);
      return H;
    }
  /* Two frames or more: whole maps, one per worker.  Splitting one map
     across a pool costs more than it saves at short frame lengths
     (corr2d-hypotheses.md section 5), and maps side by side need no split.
     One frame runs as execute() runs it. */
  if (split && nw > 1 && H >= 2)
    {
      for (size_t h0 = 0; h0 < H; h0 += nw)
        {
          const size_t   m = (H - h0 < nw) ? H - h0 : nw;
          corr2d_whole_t w = { s, in, h0 };
          dp_pool_run (s->pool, m, corr2d_whole_body, &w);
          for (size_t t = 0; t < m; t++)
            fold->fold (fold->ctx, h0 + t, s->bat_map + t * s->n_out,
                        s->n_out);
        }
      return H;
    }
  for (size_t h = 0; h < H; h++)
    {
      corr2d_batch_frame (s, in + h * n, s->bat_map, 1);
      fold->fold (fold->ctx, h, s->bat_map, s->n_out);
    }
  return H;
}

size_t
dp_corr2d_execute_fold (dp_corr2d_state_t *state, const float _Complex *in,
                        size_t n_in, const dp_corr2d_fold_t *fold)
{
  return corr2d_batch (state, in, n_in, 0, fold);
}

/* |z|^2 as every power reducer computes it: in double, rounded once to
   float.  A float squared is exact in double (24-bit mantissas, 48-bit
   product), so the sum rounds the same whether or not the compiler fuses it
   into an FMA.  In float it did not: under -ffast-math, clang -O3 compiled
   the h == 0 store and the h > 0 compare as two loop versions that rounded
   re*re + im*im differently, so an identical map compared one ulp greater
   and took the index (the tie test failed on macOS; reproduced locally with
   clang -O3).  The power of a cell must be one value, whichever code path
   computes it. */
static inline float
corr2d_pow (float _Complex z)
{
  const double re = crealf (z), im = cimagf (z);
  return (float)(re * re + im * im);
}

/* The built-in reducers.  Each copies or folds the map into its outputs;
   the typed calls below have already checked the outputs are big enough. */
static void
corr2d_fold_copy (void *ctx, size_t h, const float _Complex *map, size_t n)
{
  memcpy ((float _Complex *)ctx + h * n, map, n * sizeof *map);
}

static void
corr2d_fold_power_sum (void *ctx, size_t h, const float _Complex *map,
                       size_t n)
{
  float *out = (float *)ctx;
  for (size_t k = 0; k < n; k++)
    out[k] = (h == 0) ? corr2d_pow (map[k]) : out[k] + corr2d_pow (map[k]);
}

typedef struct
{
  float    *pow;
  uint32_t *idx;
} corr2d_max_t;

static void
corr2d_fold_power_max (void *ctx, size_t h, const float _Complex *map,
                       size_t n)
{
  const corr2d_max_t *c = (const corr2d_max_t *)ctx;
  float *restrict pw    = c->pow;
  uint32_t *restrict ix = c->idx;
  const uint32_t hh     = (uint32_t)h;
  if (h == 0)
    {
      for (size_t k = 0; k < n; k++)
        {
          pw[k] = corr2d_pow (map[k]);
          ix[k] = 0;
        }
      return;
    }
  /* A select with unconditional stores, so the loop vectorizes. Strictly
     greater: a tie keeps the earlier, lower h. */
  for (size_t k = 0; k < n; k++)
    {
      const float    p = corr2d_pow (map[k]);
      const uint32_t m = 0u - (uint32_t)(p > pw[k]);
      ix[k]            = (ix[k] & ~m) | (hh & m);
      pw[k]            = fmaxf (p, pw[k]);
    }
}

typedef struct
{
  const dp_corr2d_state_t *s;
  float                   *peak;
  uint32_t                *row;
  uint32_t                *col;
  float                   *mean;
} corr2d_peaks_t;

static void
corr2d_fold_peaks (void *ctx, size_t h, const float _Complex *map, size_t n)
{
  const corr2d_peaks_t *c = (const corr2d_peaks_t *)ctx;
  /* Two passes so both vectorize: the peak value and the sum, then the
     first cell holding that value (row-major order wins a tie). */
  float  pk  = corr2d_pow (map[0]);
  double sum = 0.0;
  for (size_t k = 0; k < n; k++)
    {
      const float p = corr2d_pow (map[k]);
      sum += p;
      pk = fmaxf (pk, p);
    }
  size_t best = 0;
  while (corr2d_pow (map[best]) != pk)
    best++;
  const int    col_out = c->s->col_out >= 0;
  const size_t width   = col_out ? 1 : c->s->nx_out;
  c->peak[h]           = pk;
  c->row[h]            = (uint32_t)(best / width);
  c->col[h]  = col_out ? (uint32_t)c->s->col_out : (uint32_t)(best % width);
  c->mean[h] = (float)(sum / (double)n);
}

size_t
dp_corr2d_execute_all_max_out (const dp_corr2d_state_t *state, size_t n_in)
{
  const size_t H = n_in / state->n;
  if (H && state->n_out > SIZE_MAX / H)
    return SIZE_MAX;
  return H * state->n_out;
}

size_t
dp_corr2d_execute_all (dp_corr2d_state_t *state, const float _Complex *in,
                       size_t n_in, float _Complex *out, size_t max_out)
{
  const size_t need = dp_corr2d_execute_all_max_out (state, n_in);
  if (corr2d_frames (state, n_in) == 0 || need == SIZE_MAX || max_out < need)
    return SIZE_MAX;
  dp_corr2d_fold_t f = { corr2d_fold_copy, out };
  corr2d_batch (state, in, n_in, 0, &f);
  return need;
}

size_t
dp_corr2d_execute_coherent (dp_corr2d_state_t *state, const float _Complex *in,
                            size_t n_in, float _Complex *out, size_t max_out)
{
  if (corr2d_frames (state, n_in) == 0 || max_out < state->n_out)
    return SIZE_MAX;
  dp_corr2d_fold_t f = { corr2d_fold_copy, out };
  corr2d_batch (state, in, n_in, 1, &f);
  return state->n_out;
}

size_t
dp_corr2d_execute_power_sum (dp_corr2d_state_t    *state,
                             const float _Complex *in, size_t n_in, float *out,
                             size_t max_out)
{
  if (corr2d_frames (state, n_in) == 0 || max_out < state->n_out)
    return SIZE_MAX;
  dp_corr2d_fold_t f = { corr2d_fold_power_sum, out };
  corr2d_batch (state, in, n_in, 0, &f);
  return state->n_out;
}

size_t
dp_corr2d_execute_power_max (dp_corr2d_state_t    *state,
                             const float _Complex *in, size_t n_in, float *out,
                             uint32_t *out1, size_t max_out)
{
  const size_t H = corr2d_frames (state, n_in);
  /* The index is a uint32: map H - 1 must fit in one. */
  if (H == 0 || H - 1 > UINT32_MAX || max_out < state->n_out)
    return SIZE_MAX;
  corr2d_max_t     c = { out, out1 };
  dp_corr2d_fold_t f = { corr2d_fold_power_max, &c };
  corr2d_batch (state, in, n_in, 0, &f);
  return state->n_out;
}

size_t
dp_corr2d_execute_peaks_max_out (const dp_corr2d_state_t *state, size_t n_in)
{
  return n_in / state->n;
}

size_t
dp_corr2d_execute_peaks (dp_corr2d_state_t *state, const float _Complex *in,
                         size_t n_in, float *out, uint32_t *out1,
                         uint32_t *out2, float *out3, size_t max_out)
{
  const size_t H = corr2d_frames (state, n_in);
  if (H == 0 || max_out < H)
    return SIZE_MAX;
  corr2d_peaks_t   c = { state, out, out1, out2, out3 };
  dp_corr2d_fold_t f = { corr2d_fold_peaks, &c };
  corr2d_batch (state, in, n_in, 0, &f);
  return H;
}

/* The three reductions to one map each write n_out values. */
size_t
dp_corr2d_execute_coherent_max_out (const dp_corr2d_state_t *state)
{
  return state->n_out;
}

size_t
dp_corr2d_execute_power_sum_max_out (const dp_corr2d_state_t *state)
{
  return state->n_out;
}

size_t
dp_corr2d_execute_power_max_max_out (const dp_corr2d_state_t *state)
{
  return state->n_out;
}

/* ── The hypothesis bank (#2683 phase 3) ──────────────────────────────────
 *
 * One frame through every carrier g in g_min..g_max on a 1/(L nx) grid,
 * times every rate (a slow-time weight vector, DelayDoppler only).  The
 * order is corr2d-hypotheses.md's: each fraction j = g mod L mixes and
 * transforms the rows once; each whole bin k = floor(g / L) is a roll of
 * that spectrum before the reference multiply; each carrier pays its row
 * inverses once; each rate pays only the slow-time FFT.  Hypothesis h is
 * (g - g_min) * R + r, and maps are folded in h order. */

/* The validated geometry of one bank call. */
typedef struct
{
  size_t L;     /**< fractions per whole bin                       */
  int    g_min; /**< lowest carrier, in 1/(L nx) cycles/sample     */
  size_t C;     /**< carriers, g_max - g_min + 1                   */
  size_t R;     /**< rates, at least 1 (no rates = one unit vector) */
  const float _Complex *rates; /**< R x ny weights, or NULL        */
} corr2d_bank_t;

/* Refuse what a bank cannot mean, before anything is allocated or written:
   a frame of another length, no grid, an empty or inverted range, rates
   that are not whole ny-long vectors, and a hypothesis count the uint32
   index cannot name.  Every path runs a bank; only the fast path and
   DelayDoppler share work across hypotheses. */
static int
corr2d_bank_check (const dp_corr2d_state_t *s, size_t x_len, size_t L,
                   int g_min, int g_max, size_t rates_len, corr2d_bank_t *b)
{
  if (x_len != s->n || L == 0 || g_min > g_max || rates_len % s->ny != 0)
    return 0;
  const size_t C = (size_t)((long long)g_max - (long long)g_min + 1);
  const size_t R = rates_len ? rates_len / s->ny : 1;
  if (C > UINT32_MAX / R)
    return 0;
  b->L     = L;
  b->g_min = g_min;
  b->C     = C;
  b->R     = R;
  b->rates = NULL;
  return 1;
}

/* floor(a / b) and the matching non-negative remainder, for any sign. */
static long long
corr2d_floordiv (long long a, long long b)
{
  return (a >= 0) ? a / b : -((-a + b - 1) / b);
}

/* The bank's scratch for one call, released by corr2d_bank_free. */
typedef struct
{
  float _Complex *spec;  /**< L x n: each fraction's row spectra          */
  unsigned char  *have;  /**< L flags: fraction j's spectra are made       */
  float _Complex *phase; /**< L x nx: e^{-j 2 pi j n / (L nx)}             */
  float _Complex *prod;  /**< nworkers x nx: one row's product spectrum   */
  float _Complex *pad;   /**< nworkers x nx_out, or NULL when native       */
  float _Complex *surf;  /**< ny x nx_out: one carrier's correlated rows   */
  float _Complex *maps;  /**< R x n_out: one carrier's maps, one per rate  */
  float _Complex *acc;   /**< n: a fresh accumulator; unshared paths only */
  float _Complex *copy;  /**< n: one corrected frame; unshared paths only */
} corr2d_bank_mem_t;

static void
corr2d_bank_free (corr2d_bank_mem_t *m)
{
  free (m->spec);
  free (m->have);
  free (m->phase);
  free (m->prod);
  free (m->pad);
  free (m->surf);
  free (m->maps);
  free (m->acc);
  free (m->copy);
}

/* The fraction spectra, phases and flags follow the caller's L, and the
   maps its rate count, so a failure there is a refusal (0), never an abort.
   The rest is the object's own size, already allocated once by create(), so
   it goes through the abort-on-OOM helpers (clib_common.h). */
static int
corr2d_bank_alloc (const dp_corr2d_state_t *s, const corr2d_bank_t *b,
                   corr2d_bank_mem_t *m)
{
  const size_t nw = (size_t)s->nworkers, L = b->L;
  memset (m, 0, sizeof *m);
  if (L > SIZE_MAX / s->n / sizeof (float _Complex)
      || b->R > SIZE_MAX / s->n_out / sizeof (float _Complex))
    return 0;
  m->spec  = malloc (L * s->n * sizeof *m->spec);
  m->have  = calloc (L, 1);
  m->phase = malloc (L * s->nx * sizeof *m->phase);
  m->maps  = malloc (b->R * s->n_out * sizeof *m->maps);
  if (!m->spec || !m->have || !m->phase || !m->maps)
    {
      corr2d_bank_free (m);
      return 0;
    }
  m->prod = dp_xmalloc (nw * s->nx * sizeof *m->prod);
  if (s->nx_out != s->nx)
    m->pad = dp_xmalloc (nw * s->nx_out * sizeof *m->pad);
  m->surf = dp_xmalloc (s->ny * s->nx_out * sizeof *m->surf);
  if (!(s->fast_path && s->col_out < 0))
    {
      m->acc  = dp_xcalloc (s->n, sizeof *m->acc);
      m->copy = dp_xmalloc (s->n * sizeof *m->copy);
    }
  /* The within-period mix of each fraction, formed in double and rounded
     once: the phase restarts at every period (rule 4 of the design page),
     so the phase between periods stays Doppler. */
  for (size_t j = 0; j < L; j++)
    for (size_t p = 0; p < s->nx; p++)
      {
        const double a          = -2.0 * M_PI * (double)j * (double)p
                                  / ((double)L * (double)s->nx);
        m->phase[j * s->nx + p] = (float)cos (a) + (float)sin (a) * I;
      }
  return 1;
}

/* Fraction j's row spectra: chunk t mixes rows r0..r1-1 and transforms them
   on worker t's forward plan.  j = 0 is not mixed at all, so the whole-bin
   carriers see exactly the frame execute() sees. */
typedef struct
{
  const dp_corr2d_state_t *s;
  const float _Complex    *x;
  const float _Complex    *phase; /**< nx values, or NULL for j = 0 */
  float _Complex          *spec;  /**< ny x nx                      */
  float _Complex          *prod;  /**< per-worker mix scratch       */
} corr2d_bank_fwd_t;

static void
corr2d_bank_fwd_body (size_t t, void *ctx)
{
  const corr2d_bank_fwd_t *c  = (const corr2d_bank_fwd_t *)ctx;
  const size_t             nx = c->s->nx;
  float _Complex          *y  = c->prod + t * nx;
  size_t                   r0, r1;
  corr2d_chunk (c->s->ny, t, c->s->nworkers, &r0, &r1);
  for (size_t i = r0; i < r1; i++)
    {
      const float _Complex *src = c->x + i * nx;
      if (c->phase)
        {
          for (size_t p = 0; p < nx; p++)
            y[p] = src[p] * c->phase[p];
          src = y;
        }
      dp_fft_execute_cf32 (c->s->row_fwd[t], src, nx, c->spec + i * nx, nx);
    }
}

/* One carrier's rows: chunk t rolls each row spectrum by k whole bins,
   multiplies by the conjugated code spectrum and inverts it (the dump's own
   inverse, corr2d_row_inverse) into the surface. */
typedef struct
{
  const dp_corr2d_state_t *s;
  const float _Complex    *spec; /**< fraction j's ny x nx spectra */
  size_t                   k;    /**< the roll, 0 .. nx-1          */
  corr2d_bank_mem_t       *m;
  float _Complex          *dst; /**< ny x nx_out                  */
} corr2d_bank_rows_t;

static void
corr2d_bank_rows_body (size_t t, void *ctx)
{
  const corr2d_bank_rows_t *c  = (const corr2d_bank_rows_t *)ctx;
  const dp_corr2d_state_t  *s  = c->s;
  const size_t              nx = s->nx, nxo = s->nx_out, k = c->k;
  float _Complex           *a   = c->m->prod + t * nx;
  float _Complex           *pad = c->m->pad ? c->m->pad + t * nxo : NULL;
  size_t                    r0, r1;
  corr2d_chunk (s->ny, t, s->nworkers, &r0, &r1);
  for (size_t i = r0; i < r1; i++)
    {
      /* DFT{x e^{-j 2 pi k n / nx}}[q] = X[(q + k) mod nx]: a correction of
         k bins reads the spectrum k bins up. */
      const float _Complex *w = c->spec + i * nx;
      for (size_t q = 0; q < nx - k; q++)
        a[q] = w[q + k] * s->row_ref_spec[q];
      for (size_t q = nx - k; q < nx; q++)
        a[q] = w[q + k - nx] * s->row_ref_spec[q];
      corr2d_row_inverse (s, t, a, pad, c->dst + i * nxo);
    }
}

/* One carrier's slow-time stage for every rate: the R x ntiles items split
   into nworkers chunks, rate innermost so each tile of the surface is read
   while it is warm. */
typedef struct
{
  const dp_corr2d_state_t *s;
  const float _Complex    *surf;
  const corr2d_bank_t     *b;
  float _Complex          *maps; /**< R x n_out */
} corr2d_bank_cols_t;

static void
corr2d_bank_cols_body (size_t t, void *ctx)
{
  const corr2d_bank_cols_t *c      = (const corr2d_bank_cols_t *)ctx;
  const dp_corr2d_state_t  *s      = c->s;
  const size_t              ntiles = corr2d_ntiles (s->nx), R = c->b->R;
  size_t                    i0, i1;
  corr2d_chunk (ntiles * R, t, s->nworkers, &i0, &i1);
  for (size_t it = i0; it < i1; it++)
    {
      const size_t          b = it / R, r = it % R;
      const float _Complex *w = c->b->rates ? c->b->rates + r * s->ny : NULL;
      corr2d_col_tile (s, t, c->surf, c->maps + r * s->n_out, b, w);
    }
}

/* Run a stage's chunks in order on the caller.  Same chunks, same per-worker
   plans, so a bank is bit-identical for any worker count; but one frame is
   never split across the pool, which costs more than it saves at the frame
   lengths a bank serves (corr2d-hypotheses.md section 5: short frames run on
   one thread).  Parallel banks would split hypotheses, not a frame. */
static void
corr2d_bank_stage (const dp_corr2d_state_t *s, void (*body) (size_t, void *),
                   void                    *ctx)
{
  for (size_t t = 0; t < (size_t)s->nworkers; t++)
    body (t, ctx);
}

/* The one bank loop: carriers in ascending g, rates inside, each map folded
   as h = (g - g_min) * R + r.  The dwell is never read or written. */
/* A path with no shared stage (the general 2-D path, a known column): each
   hypothesis is its corrected frame, made by hand and run through the
   path's own frame code into a fresh accumulator, which the frame code
   leaves zero again.  The bank's definition, with nothing shared. */
static void
corr2d_bank_unshared (const dp_corr2d_state_t *s, const float _Complex *x,
                      const corr2d_bank_t *b, const dp_corr2d_fold_t *fold,
                      corr2d_bank_mem_t *m)
{
  const size_t    ny = s->ny, nx = s->nx;
  float _Complex *ph = m->prod; /* nx: one carrier's within-period mix */
  for (size_t c = 0; c < b->C; c++)
    {
      const double g = (double)b->g_min + (double)c;
      for (size_t p = 0; p < nx; p++)
        {
          const double a
              = -2.0 * M_PI * g * (double)p / ((double)b->L * (double)nx);
          ph[p] = (float)cos (a) + (float)sin (a) * I;
        }
      for (size_t r = 0; r < b->R; r++)
        {
          const float _Complex *w = b->rates ? b->rates + r * ny : NULL;
          for (size_t i = 0; i < ny; i++)
            for (size_t p = 0; p < nx; p++)
              m->copy[i * nx + p] = x[i * nx + p] * ph[p] * (w ? w[i] : 1.0f);
          if (s->col_out >= 0)
            corr2d_col_frame (s, m->copy, m->acc, m->maps, 1);
          else
            corr2d_general_frame (s, m->copy, m->acc, m->maps, 1);
          fold->fold (fold->ctx, c * b->R + r, m->maps, s->n_out);
        }
    }
}

static size_t
corr2d_bank_run (const dp_corr2d_state_t *s, const float _Complex *x,
                 const corr2d_bank_t *b, const dp_corr2d_fold_t *fold)
{
  corr2d_bank_mem_t m;
  if (!corr2d_bank_alloc (s, b, &m))
    return SIZE_MAX;
  if (!(s->fast_path && s->col_out < 0))
    {
      corr2d_bank_unshared (s, x, b, fold, &m);
      corr2d_bank_free (&m);
      return b->C * b->R;
    }
  const size_t n = s->n, L = b->L;
  for (size_t c = 0; c < b->C; c++)
    {
      const long long g = (long long)b->g_min + (long long)c;
      const long long k = corr2d_floordiv (g, (long long)L);
      const size_t    j = (size_t)(g - k * (long long)L);
      if (!m.have[j])
        {
          corr2d_bank_fwd_t f = { s, x, j ? m.phase + j * s->nx : NULL,
                                  m.spec + j * n, m.prod };
          corr2d_bank_stage (s, corr2d_bank_fwd_body, &f);
          m.have[j] = 1;
        }
      const long long kn   = (long long)s->nx;
      const size_t    roll = (size_t)(((k % kn) + kn) % kn);
      /* Corr2D with rates, like DelayDoppler, keeps the carrier's rows in
         the surface and makes one map per rate from them. */
      const int          keep = s->delay_doppler || b->rates;
      float _Complex    *dst  = keep ? m.surf : m.maps;
      corr2d_bank_rows_t rows = { s, m.spec + j * n, roll, &m, dst };
      corr2d_bank_stage (s, corr2d_bank_rows_body, &rows);
      if (s->delay_doppler)
        {
          corr2d_bank_cols_t cols = { s, m.surf, b, m.maps };
          corr2d_bank_stage (s, corr2d_bank_cols_body, &cols);
        }
      else if (b->rates)
        /* Corr2D's rows are its periods: weighting period i weights map
           row i, the correlation being linear in the row. */
        for (size_t r = 0; r < b->R; r++)
          for (size_t i = 0; i < s->ny; i++)
            for (size_t q = 0; q < s->nx_out; q++)
              m.maps[r * s->n_out + i * s->nx_out + q]
                  = m.surf[i * s->nx_out + q] * b->rates[r * s->ny + i];
      for (size_t r = 0; r < b->R; r++)
        fold->fold (fold->ctx, c * b->R + r, m.maps + r * s->n_out, s->n_out);
    }
  corr2d_bank_free (&m);
  return b->C * b->R;
}

size_t
dp_corr2d_bank_fold (const dp_corr2d_state_t *state, const float _Complex *x,
                     size_t x_len, size_t L, int g_min, int g_max,
                     const float _Complex *rates, size_t rates_len,
                     const dp_corr2d_fold_t *fold)
{
  corr2d_bank_t b;
  if (!corr2d_bank_check (state, x_len, L, g_min, g_max, rates_len, &b))
    return SIZE_MAX;
  b.rates = rates_len ? rates : NULL;
  return corr2d_bank_run (state, x, &b, fold);
}

size_t
dp_corr2d_bank_power_max_max_out (dp_corr2d_state_t *state, size_t x_len)
{
  (void)x_len;
  return state->n_out;
}

size_t
dp_corr2d_bank_power_max (const dp_corr2d_state_t *state,
                          const float _Complex *x, size_t x_len, size_t L,
                          int g_min, int g_max, const float _Complex *rates,
                          size_t rates_len, float *out, uint32_t *out1,
                          size_t max_out)
{
  corr2d_bank_t b;
  if (!corr2d_bank_check (state, x_len, L, g_min, g_max, rates_len, &b)
      || max_out < state->n_out)
    return SIZE_MAX;
  b.rates            = rates_len ? rates : NULL;
  corr2d_max_t     c = { out, out1 };
  dp_corr2d_fold_t f = { corr2d_fold_power_max, &c };
  return corr2d_bank_run (state, x, &b, &f) == SIZE_MAX ? SIZE_MAX
                                                        : state->n_out;
}

/* ── Detect coarse, refine the candidate (corr2d-hypotheses.md section 6) */

/* The map value of carrier g (on a 1/(L nx) grid) and rate w at one cell,
   with no transform: row i's correlation at lag tau is the time-domain sum
   the known-column path computes, over the row mixed within its period;
   DelayDoppler then takes the slow-time DFT at the cell's Doppler bin.  It
   is the bank's map at that cell, up to float rounding. */
static double
corr2d_cell_power (const dp_corr2d_state_t *s, const float _Complex *x,
                   const float _Complex *code_conj, long long g, size_t L,
                   const float _Complex *w, size_t row, size_t tau,
                   double _Complex *ph)
{
  const size_t nx = s->nx, ny = s->ny, nyo = s->ny_out;
  /* The within-period mix depends only on n, so one table serves every row:
     nx sines rather than ny * nx. */
  for (size_t n = 0; n < nx; n++)
    {
      const double a
          = -2.0 * M_PI * (double)g * (double)n / ((double)L * (double)nx);
      ph[n] = cos (a) + I * sin (a);
    }
  const double bin
      = s->delay_doppler ? (double)((row + nyo - nyo / 2) % nyo) : 0.0;
  double _Complex v = 0.0;
  for (size_t i = 0; i < ny; i++)
    {
      if (!s->delay_doppler && i != row)
        continue;
      const float _Complex *xi = x + i * nx;
      double _Complex sum      = 0.0;
      for (size_t p = 0; p < nx; p++)
        {
          const size_t n = (p + tau) % nx;
          sum += (double _Complex)code_conj[p] * (double _Complex)xi[n]
                 * ph[n];
        }
      if (w)
        sum *= (double _Complex)w[i];
      if (s->delay_doppler)
        {
          const double a = -2.0 * M_PI * bin * (double)i / (double)nyo;
          sum *= cos (a) + I * sin (a);
        }
      v += sum;
    }
  return creal (v) * creal (v) + cimag (v) * cimag (v);
}

size_t
dp_corr2d_bank_detect_max_out (dp_corr2d_state_t *state, size_t x_len)
{
  (void)x_len;
  return state->n_out;
}

/* The cell's power under one fine carrier and rate, by the bank itself: the
   map of that one hypothesis, read at the cell.  The refine on a path with
   no time-domain cell evaluator (an interpolated delay axis, the general
   path, a known column), where it costs a map rather than a dot product. */
typedef struct
{
  size_t cell;
  double power;
} corr2d_cell_t;

static void
corr2d_fold_cell (void *ctx, size_t h, const float _Complex *map, size_t n)
{
  corr2d_cell_t *c = (corr2d_cell_t *)ctx;
  (void)h, (void)n;
  const double re = crealf (map[c->cell]), im = cimagf (map[c->cell]);
  c->power = re * re + im * im;
}

size_t
dp_corr2d_bank_detect (const dp_corr2d_state_t *state, const float _Complex *x,
                       size_t x_len, size_t L_d, size_t L_r, int g_min,
                       int g_max, float t_candidate, float t_confirm,
                       const float _Complex *rates, size_t rates_len,
                       float *out, int32_t *out1, uint32_t *out2,
                       uint32_t *out3, uint32_t *out4, size_t max_out)
{
  corr2d_bank_t b;
  if (L_d == 0 || L_r < L_d || L_r % L_d != 0
      || !corr2d_bank_check (state, x_len, L_d, g_min, g_max, rates_len, &b)
      || max_out < state->n_out)
    return SIZE_MAX;
  const long long m = (long long)(L_r / L_d);
  /* every refined carrier must fit the int32 the record carries */
  if ((long long)g_min * m - (m - 1) < INT32_MIN
      || (long long)g_max * m + (m - 1) > INT32_MAX)
    return SIZE_MAX;
  b.rates            = rates_len ? rates : NULL;
  const size_t n_out = state->n_out, nx = state->nx;
  /* The map's own geometry: a known column is ny rows of one. */
  const size_t width = state->col_out >= 0 ? 1 : state->nx_out;
  const size_t rows  = n_out / width;
  /* The time-domain refine needs whole lags on a shared row stage. */
  const int direct
      = state->fast_path && state->col_out < 0 && state->nx_out == nx;
  float           *pw   = dp_xmalloc (n_out * sizeof *pw);
  uint32_t        *ix   = dp_xmalloc (n_out * sizeof *ix);
  float _Complex  *code = direct ? dp_xmalloc (nx * sizeof *code) : NULL;
  double _Complex *ph   = direct ? dp_xmalloc (nx * sizeof *ph) : NULL;
  /* 1. Detect: the strongest cell over the coarse bank, and which carrier
     and rate it came from. */
  corr2d_max_t     c = { pw, ix };
  dp_corr2d_fold_t f = { corr2d_fold_power_max, &c };
  if (corr2d_bank_run (state, x, &b, &f) == SIZE_MAX)
    {
      free (pw), free (ix), free (code), free (ph);
      return SIZE_MAX;
    }
  if (direct)
    {
      /* The refine wants conj(code[p]) in the time domain, and the object
         keeps only row_ref_spec = conj(FFT(code)).  Its unnormalized
         inverse over nx is nx * conj(code[(nx - p) mod nx]): scale it, then
         undo the time reversal (index 0 is its own mirror). */
      dp_fft_execute_cf32 (state->row_inv[0], state->row_ref_spec, nx, code,
                           nx);
      for (size_t p = 0; p < nx; p++)
        code[p] = code[p] / (float)nx;
      for (size_t p = 1; p < nx - p; p++)
        {
          const float _Complex tmp = code[p];
          code[p]                  = code[nx - p];
          code[nx - p]             = tmp;
        }
    }
  /* 2. Candidates: local maxima above t_candidate, over the 8 neighbors
     with both axes circular (a delay lag and a Doppler bin both wrap).
     3. Refine each at its own cell over the fine carriers between its coarse
     neighbors, on the rate that found it; 4. confirm. */
  size_t nrec = 0, ok = 1;
  for (size_t cell = 0; cell < n_out && ok; cell++)
    {
      const float p0 = pw[cell];
      if (!(p0 > t_candidate))
        continue;
      const size_t row = cell / width, col = cell % width;
      int          peak = 1;
      for (int dr = -1; dr <= 1 && peak; dr++)
        for (int dc = -1; dc <= 1 && peak; dc++)
          {
            if (!dr && !dc)
              continue;
            const size_t rr = (row + rows + (size_t)(dr + 1) - 1) % rows;
            const size_t cc = (col + width + (size_t)(dc + 1) - 1) % width;
            peak            = pw[rr * width + cc] <= p0;
          }
      if (!peak)
        continue;
      const size_t          h    = ix[cell];
      const size_t          r    = h % b.R;
      const long long       gd   = (long long)g_min + (long long)(h / b.R);
      const float _Complex *w    = b.rates ? b.rates + r * state->ny : NULL;
      double                best = -1.0;
      long long             gbst = gd * m;
      for (long long gr = gd * m - (m - 1); gr <= gd * m + (m - 1); gr++)
        {
          double pr;
          if (direct)
            pr = corr2d_cell_power (state, x, code, gr, L_r, w, row, col, ph);
          else
            {
              corr2d_bank_t    one = { L_r, (int)gr, 1, 1, w };
              corr2d_cell_t    cc  = { cell, 0.0 };
              dp_corr2d_fold_t cf  = { corr2d_fold_cell, &cc };
              if (corr2d_bank_run (state, x, &one, &cf) == SIZE_MAX)
                {
                  ok = 0;
                  break;
                }
              pr = cc.power;
            }
          if (pr > best) /* the lowest g wins a tie */
            {
              best = pr;
              gbst = gr;
            }
        }
      if (!ok || !(best > (double)t_confirm))
        continue;
      out[nrec]  = (float)best;
      out1[nrec] = (int32_t)gbst;
      out2[nrec] = (uint32_t)r;
      out3[nrec] = (uint32_t)row;
      out4[nrec]
          = state->col_out >= 0 ? (uint32_t)state->col_out : (uint32_t)col;
      nrec++;
    }
  free (pw), free (ix), free (code), free (ph);
  return ok ? nrec : SIZE_MAX;
}
