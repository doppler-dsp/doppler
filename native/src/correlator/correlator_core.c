#include "doppler/correlator/correlator_core.h"
#include "correlator_map.h"
#include "doppler/correlator/correlator_private.h"

#include "doppler/clib_common.h"
#include "doppler/dp_complex.h"
#include "doppler/util/util_core.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The detector flavors: the stream and detection layers on top of the map
   (docs/design/correlator.md section 4). They take frames through push
   alone, so every execute, batch and bank call refuses them. */
static inline int
corr2d_is_detector (const dp_correlator_state_t *s)
{
  return s->flavor == DP_CORRELATOR_DETECTOR_1D
         || s->flavor == DP_CORRELATOR_DETECTOR_2D;
}

/* The search flavors (#2752 3.4b): the detection layers with the search
   policy on top, in the search flavors' section below. The functions every
   flavor shares dispatch to it; the map's refuse them, as they refuse a
   detector. */
static inline int
corr2d_is_search (const dp_correlator_state_t *s)
{
  return s->flavor == DP_CORRELATOR_SEARCH_CONTINUOUS
         || s->flavor == DP_CORRELATOR_SEARCH_BURST;
}

/* The continuous search: the one flavor the carrier coupling, telemetry,
   surface and block read-outs are honored on (docs/design/
   correlator-python.md section 18). The burst search shares the record but
   nothing shows it honors them -- the carrier feeds a chip arithmetic a
   sample template does not have -- so they refuse it. */
static inline int
corr2d_is_continuous (const dp_correlator_state_t *s)
{
  return s->flavor == DP_CORRELATOR_SEARCH_CONTINUOUS;
}

/* The delay-Doppler flavor: `flavor` is its one record (#2901), and every
   path that runs the slow-time map asks here. */
static inline int
corr2d_is_dd (const dp_correlator_state_t *s)
{
  return s->flavor == DP_CORRELATOR_DELAY_DOPPLER;
}

/* A map flavor: the ones execute, its batch family and the bank serve. */
static inline int
corr2d_is_map (const dp_correlator_state_t *s)
{
  return !corr2d_is_detector (s) && !corr2d_is_search (s);
}

static size_t corr2d_execute (dp_correlator_state_t *state,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 2-D spectral zero-pad (ny,nx) -> (ny_out,nx_out), axis-separable: pad rows
 * (nx -> nx_out) into ztmp, then columns (ny -> ny_out) into out.  The
 * even-axis Nyquist split is handled per axis by dp_fft_pad_spectrum(). */
static void
corr2d_zeropad_2d (const dp_correlator_state_t *s, const float _Complex *p,
                   float _Complex *out)
{
  for (size_t i = 0; i < s->ny; i++)
    dp_fft_pad_spectrum (p + i * s->nx, s->nx, s->ztmp + i * s->nx_out,
                         s->nx_out);
  for (size_t j = 0; j < s->nx_out; j++)
    {
      for (size_t i = 0; i < s->ny; i++)
        s->zcol[i] = s->ztmp[i * s->nx_out + j];
      dp_fft_pad_spectrum (s->zcol, s->ny, s->zcolout, s->ny_out);
      for (size_t i = 0; i < s->ny_out; i++)
        out[i * s->nx_out + j] = s->zcolout[i];
    }
}

/* True iff ref (ny,nx, row-major) is exactly zero outside row 0 — the
 * single-row-reference fast-path precondition (see correlator_core.h's file
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

/* Columns per slow-time tile.  A slow-time transform reads its input down
 * columns, a stride of a row; gathering CORR2D_COL_TILE
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
static int
corr2d_build_fast (dp_correlator_state_t *state, const float _Complex *code,
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
      /* A transform too long to plan is a refusal the caller names, not
         an abort: the lengths come from the caller's ref and nx_out. */
      state->row_fwd[t] = dp_fft_create (nx, -1, 1);
      state->row_inv[t] = dp_fft_create (nxo, +1, 1);
      if (!state->row_fwd[t] || !state->row_inv[t])
        return -1;
    }
  /* Worker 0's plans are the ones every serial caller (set_ref, the tests)
     names; they are aliases, freed once through the arrays. */
  state->fwd1d = state->row_fwd[0];
  state->inv1d = state->row_inv[0];

  state->row_ref_spec = dp_xmallocn (nx, sizeof (*state->row_ref_spec));
  /* row_ref_spec = conj(FFT_nx(code)) -- the only row of the reference
   * with energy in it. */
  dp_fft_execute_cf32 (state->fwd1d, code, nx, state->row_ref_spec, nx);
  for (size_t k = 0; k < nx; k++)
    state->row_ref_spec[k] = conjf (state->row_ref_spec[k]);
  return 0;
}

/* A flavor constructor's refusal: point @p why (when given) at the
   sentence naming the knob, a static string, and return NULL. The public
   constructors pass NULL; dp_correlator_create() passes its caller's, so
   the binding raises the knob's own sentence. */
#define CORR_WHY(msg)                                                         \
  do                                                                          \
    {                                                                         \
      if (why)                                                                \
        *why = (msg);                                                         \
      return NULL;                                                            \
    }                                                                         \
  while (0)

/* dp_correlator_create_2d(), saying why it refuses (the 1-D flavor and both
   detectors forward here, so their map knobs are refused in one place). */
static dp_correlator_state_t *
corr2d_create (const float _Complex *ref, size_t ny, size_t nx, size_t dwell,
               int nthreads, size_t ny_out, size_t nx_out, int col_out,
               const char **why)
{
  /* An empty axis is refused here, before anything is built: the row plans
     below go through dp_xnn, and an FFT plan of length 0 is NULL, so an
     empty reference would abort the process instead of returning NULL.
     The 1-D flavor (ref_len 0) arrives here as nx 0. */
  if (nx == 0 || ny == 0)
    return NULL; /* the dispatcher refuses an empty ref first */

  size_t n   = ny * nx;
  size_t nyo = ny_out ? ny_out : ny;
  size_t nxo = nx_out ? nx_out : nx;
  /* The output may only interpolate (>= native). */
  if (nyo < ny)
    CORR_WHY ("Correlator: ny_out pads the rows; it must be 0 (native) or "
              "at least the ref's rows");
  if (nxo < nx)
    CORR_WHY ("Correlator: nx_out pads the lags; it must be 0 (native) or "
              "at least len(ref)");
  /* A padded length whose map (or transform buffer) no size_t can count
     is refused before anything is sized from it (#2958). */
  size_t bytes = 0;
  if (!dp_size_mul (nxo, 2 * sizeof (double), &bytes))
    CORR_WHY ("Correlator: nx_out asks for a map no size_t can hold");
  /* The native ny x nx is the caller's ref, so a product that does not
     fit is a pad's doing: ny_out's when it pads, else nx_out's. */
  if (!dp_size_mul (nyo, nxo, &bytes)
      || !dp_size_mul (bytes, sizeof (float _Complex), &bytes))
    CORR_WHY (nyo != ny
                  ? "Correlator: ny_out asks for a map no size_t can hold"
                  : "Correlator: nx_out asks for a map no size_t can hold");
  /* The header says dwell "must be >= 1" and nothing enforced it. dwell = 0
     is not a degenerate configuration, it is a black hole: the dump test is
     `++count == dwell`, which a zero dwell never satisfies, so the object
     accumulates every frame it is given and emits nothing until count wraps
     at SIZE_MAX. A caller whose dwell came from a computed value that
     underflowed gets silence and unbounded accumulation rather than an
     error. dp_correlator_create_detector_2d forwards its own dwell straight
     here, so validating at the primitive covers both objects. */
  if (dwell < 1)
    CORR_WHY ("Correlator: dwell must be at least 1 (frames a dump)");
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
  if (col_out >= 0 && (size_t)col_out >= nx)
    CORR_WHY ("Correlator: col_out must be -1 or a lag below len(ref[0])");
  if (col_out >= 0 && nyo != ny)
    CORR_WHY ("Correlator: col_out reads a native map; ny_out pads it, so it "
              "must be 0 with col_out");
  if (col_out >= 0 && nxo != nx)
    CORR_WHY ("Correlator: col_out reads a native map; nx_out pads it, so it "
              "must be 0 with col_out");
  if (col_out >= 0 && !fast)
    CORR_WHY ("Correlator: col_out needs a ref with energy in its first row "
              "only");

  dp_correlator_state_t *state
      = calloc (1, sizeof (*state)); /* NULL-init pointers */
  if (!state)
    return NULL;

  state->work_fft = dp_mallocn (n, sizeof (*state->work_fft));
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
      if (corr2d_build_fast (state, ref, nx, nxo, nthreads,
                             col_out >= 0 ? 1 : ny))
        {
          dp_correlator_destroy (state);
          /* Only a padded length is the caller's knob; at native nx_out
             the ref's own transform failed, which is memory. */
          if (nxo != nx)
            CORR_WHY ("Correlator: nx_out asks for a transform that cannot "
                      "be allocated");
          return NULL;
        }

      if (nxo != nx)
        {
          state->work_pad = dp_mallocn (dp_size_mul_sat (ny, nxo),
                                        sizeof (float _Complex));
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

      state->ref_spec = dp_mallocn (n, sizeof (*state->ref_spec));
      if (!state->ref_spec)
        goto fail;

      if (decoupled)
        {
          state->work_pad = dp_mallocn (dp_size_mul_sat (nyo, nxo),
                                        sizeof (float _Complex));
          state->ztmp     = dp_mallocn (dp_size_mul_sat (ny, nxo),
                                        sizeof (float _Complex));
          state->zcol     = dp_mallocn (ny, sizeof (float _Complex));
          state->zcolout  = dp_mallocn (nyo, sizeof (float _Complex));
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
  state->flavor    = DP_CORRELATOR_2D;
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
      state->col_ref = dp_xmallocn (nx, sizeof (*state->col_ref));
      for (size_t v = 0; v < nx; v++)
        state->col_ref[v] = conjf (ref[v]);
    }
  return state;

fail:
  dp_correlator_destroy (state);
  return NULL;
}

dp_correlator_state_t *
dp_correlator_create_2d (const float _Complex *ref, size_t ny, size_t nx,
                         size_t dwell, int nthreads, size_t ny_out,
                         size_t nx_out, int col_out)
{
  return corr2d_create (ref, ny, nx, dwell, nthreads, ny_out, nx_out, col_out,
                        NULL);
}

/* The 1-D flavor is the 2-D one at one row (docs/design/correlator.md
 * section 2.1): the single-row path, its count and its dwell, so nothing of
 * Corr's own survives (D1, D4). One row is one chunk, so the pool is never
 * built and nthreads changes nothing, as Corr documented. */
static dp_correlator_state_t *
corr1d_create (const float _Complex *ref, size_t ref_len, size_t dwell,
               int nthreads, size_t n_out, const char **why)
{
  dp_correlator_state_t *state
      = corr2d_create (ref, 1, ref_len, dwell, nthreads, 0, n_out, -1, why);
  if (state)
    state->flavor = DP_CORRELATOR_1D;
  return state;
}

dp_correlator_state_t *
dp_correlator_create_1d (const float _Complex *ref, size_t ref_len,
                         size_t dwell, int nthreads, size_t n_out)
{
  return corr1d_create (ref, ref_len, dwell, nthreads, n_out, NULL);
}

/* ── Detector flavors (docs/design/correlator.md section 4) ─────────────── */

/* The stream and detection layers, on a correlator create_2d built: the
   ring and its framer (frames of n at hop n, so they tile the stream), the
   dump buffer, its magnitudes and the median's scratch.  Returns 0, or -1
   when the ring cannot be made (the caller destroys). */
static int
corr2d_detection_build (dp_correlator_state_t *s, size_t noise_lo,
                        size_t noise_hi, det_noise_mode_t noise_mode,
                        float threshold)
{
  const size_t n = s->n;
  /* Clamp the noise window to [0, n-1]. The binding passes a SIZE_MAX
     sentinel (default_raw) for the documented "n-1" full-window default;
     without this clamp it would overflow the scratch sizing below and read
     mag_buf out of bounds in det_noise_estimate. */
  const size_t hi = (noise_hi < n) ? noise_hi : n - 1;
  const size_t lo = (noise_lo < n) ? noise_lo : n - 1;
  s->noise_lo     = (lo <= hi) ? lo : hi;
  s->noise_hi     = (lo <= hi) ? hi : lo;
  s->noise_mode   = noise_mode;
  s->threshold    = threshold;

  s->ring = det_ring_create (n > 512 ? n : 512);
  if (!s->ring)
    return -1;
  s->ring_cap = s->ring->capacity;
  if (dp_f32_framer_init (&s->framer, s->ring, n, n) != DP_OK)
    return -1;
  /* Sized from the map's own n, already built: only genuine OOM fails
     here, so these abort through the helpers (clib_common.h). */
  s->det_out       = dp_xmallocn (n, sizeof *s->det_out);
  s->mag_buf       = dp_xmallocn (n, sizeof *s->mag_buf);
  s->noise_scratch = dp_xmallocn ((s->noise_hi - s->noise_lo + 1),
                                  sizeof *s->noise_scratch);
  return 0;
}

static dp_correlator_state_t *
corr_detector_create (const float _Complex *ref, size_t ny, size_t nx,
                      size_t dwell, size_t noise_lo, size_t noise_hi,
                      det_noise_mode_t noise_mode, float threshold,
                      int nthreads, const char **why)
{
  dp_correlator_state_t *s
      = corr2d_create (ref, ny, nx, dwell, nthreads, 0, 0, -1, why);
  if (!s)
    return NULL;
  s->flavor = DP_CORRELATOR_DETECTOR_2D;
  if (corr2d_detection_build (s, noise_lo, noise_hi, noise_mode, threshold))
    {
      dp_correlator_destroy (s);
      return NULL;
    }
  return s;
}

dp_correlator_state_t *
dp_correlator_create_detector_2d (const float _Complex *ref, size_t ny,
                                  size_t nx, size_t dwell, size_t noise_lo,
                                  size_t noise_hi, det_noise_mode_t noise_mode,
                                  float threshold, int nthreads)
{
  return corr_detector_create (ref, ny, nx, dwell, noise_lo, noise_hi,
                               noise_mode, threshold, nthreads, NULL);
}

/* The 1-D detector is the 2-D one at one row (test_detector_equiv), on the
   1-D flavor's direct single-row call. */
dp_correlator_state_t *
dp_correlator_create_detector_1d (const float _Complex *ref, size_t ref_len,
                                  size_t dwell, size_t noise_lo,
                                  size_t noise_hi, det_noise_mode_t noise_mode,
                                  float threshold, int nthreads)
{
  dp_correlator_state_t *s
      = corr_detector_create (ref, 1, ref_len, dwell, noise_lo, noise_hi,
                              noise_mode, threshold, nthreads, NULL);
  if (s)
    s->flavor = DP_CORRELATOR_DETECTOR_1D;
  return s;
}

/* The last dump's statistic, from det_out: the magnitudes, the one peak
   picker at one peak and no gate (the first maximum; a NaN is never the
   peak, so an all-NaN map lists nothing and reads (0, 0) at 0), the noise
   floor and their ratio. */
static void
corr2d_detection_stat (dp_correlator_state_t *s)
{
  const size_t n = s->n;
  for (size_t k = 0; k < n; k++)
    s->mag_buf[k] = cabsf (s->det_out[k]);

  det_peak_t pk = { 0 };
  (void)det_peak_list (s->mag_buf, s->ny, s->nx, -1.0f, 0, 0, NULL, &pk, 1);
  s->peak_row = pk.row;
  s->peak_col = pk.col;
  s->peak_mag = pk.value;

  s->noise_est = det_noise_estimate (s->mag_buf, s->noise_lo, s->noise_hi,
                                     s->noise_scratch, s->noise_mode);
  s->test_stat = (s->noise_est > 0.0f) ? (s->peak_mag / s->noise_est) : 0.0f;
}

/* One frame of dp_correlator_push(), the step det_framed_push() drives:
   correlate, and on a dump that passes the gate write ONE hit. */
static int
corr2d_detection_step (void *obj, const float _Complex *frame, void *result,
                       size_t slot)
{
  dp_correlator_state_t *s = (dp_correlator_state_t *)obj;
  if (corr2d_execute (s, frame, s->n, s->det_out, s->n) == 0)
    return 0; /* still accumulating: no dump yet */

  s->_last_corr_valid = 1;
  corr2d_detection_stat (s);
  if (s->threshold == 0.0f || s->test_stat > s->threshold)
    {
      /* The search flavors' fields (chip_phase on) are 0 from a detector
         (docs/design/correlator.md section 4: the one hit record). */
      ((dp_correlator_hit_t *)result)[slot]
          = (dp_correlator_hit_t){ .row       = s->peak_row,
                                   .col       = s->peak_col,
                                   .peak_mag  = s->peak_mag,
                                   .noise_est = s->noise_est,
                                   .test_stat = s->test_stat };
      return 1;
    }
  return 0;
}

size_t
dp_correlator_push (dp_correlator_state_t *state, const float _Complex *in,
                    size_t n_in, dp_correlator_hit_t *result,
                    size_t max_results)
{
  if (corr2d_is_search (state))
    return dp__search_push (state, in, n_in, result, max_results);
  if (!corr2d_is_detector (state))
    return SIZE_MAX;
  /* The drain, its contract and why a batch is exact: correlator_private.h. */
  return det_framed_push (&state->framer, in, n_in, result, max_results,
                          corr2d_detection_step, state, &state->consumed);
}

size_t
dp_correlator_push_max_out (const dp_correlator_state_t *state, size_t n_in)
{
  if (corr2d_is_map (state))
    return 0;
  /* The frames the input completes, counting the carry: the framer's own
     count (its frames tile the stream, hop = frame), times what one frame
     may make -- one hit a detector's dump, max_peaks a search's dwell. */
  const size_t frames = dp_f32_framer_frames_in (&state->framer, n_in);
  const size_t per    = corr2d_is_search (state) ? state->max_peaks : 1;
  size_t       bound;
  return dp_size_mul (frames, per, &bound) ? bound : SIZE_MAX;
}

size_t
dp_correlator_consumed (const dp_correlator_state_t *state)
{
  return state->consumed;
}

int
dp_correlator_set_threshold (dp_correlator_state_t *state, float threshold)
{
  if (!corr2d_is_detector (state))
    return DP_ERR_UNSUPPORTED;
  state->threshold = threshold;
  return DP_OK;
}

void
dp_correlator_destroy (dp_correlator_state_t *state)
{
  if (!state)
    return;
  if (corr2d_is_search (state))
    {
      dp__search_destroy (state);
      return;
    }
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
    }
  free (state->row_fwd);
  free (state->row_inv);
  dp__dd_map_destroy (state->dd_map);
  free (state->dd_fine);
  free (state->dd_rows);
  free (state->bat_fine);
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
  if (state->ring)
    dp_f32_destroy (state->ring);
  free (state->det_out);
  free (state->mag_buf);
  free (state->noise_scratch);
  free (state);
}

void
dp_correlator_reset (dp_correlator_state_t *state)
{
  if (corr2d_is_search (state))
    {
      dp__search_reset (state);
      return;
    }
  memset (state->accum, 0, state->n * sizeof (*state->accum));
  state->count = 0;
  if (!corr2d_is_detector (state))
    return;
  dp_f32_framer_reset (&state->framer);
  state->_last_corr_valid = 0;
  state->consumed         = 0;
  /* The last dump's result is in the blob, so a reset that kept it would
     serialize a dump this fresh cycle never made (doppler#2642). */
  state->peak_row  = 0;
  state->peak_col  = 0;
  state->peak_mag  = 0.0f;
  state->noise_est = 0.0f;
  state->test_stat = 0.0f;
}

/* Serializable state — one blob for every flavor (docs/design/correlator.md
 * section 4): the flavor, ny, ny_out, the frame count and the ny*nx
 * accumulator. The FFT plans and the reference spectrum are config,
 * recomputed by create().
 *
 * The flavor, ny and ny_out are reject keys (docs/design/state-
 * serialization.md, "What goes in the blob"). The flavor because the
 * accumulator means the same thing in each, so a restore across flavors
 * would silently turn a correlation surface into a delay-Doppler map, or
 * back; ny because the size check sees only ny*nx, and a (4, 8) blob read as
 * (8, 4) would be reinterpreted rather than refused; ny_out because it fixes
 * the map the dwell in progress dumps into.
 *
 * Within one flavor and shape, three more config values change what the
 * accumulator holds, so they are reject keys too: fast_path (row spectra
 * against one 2-D spectrum), col_out (per-row dot products against
 * spectra) and nx_out (the width the dwell in progress dumps into). A blob
 * from a single-row-reference object would otherwise load into a
 * full-reference object of the same shape and dump garbage. */
size_t
dp_correlator_state_bytes (const dp_correlator_state_t *s)
{
  if (corr2d_is_search (s))
    return dp__search_state_bytes (s);
  size_t b = sizeof (dp_state_hdr_t) + 7 * sizeof (uint64_t)
             + s->n * sizeof (float _Complex);
  /* A detector flavor: noise_lo, noise_hi, noise_mode, the threshold, the
     framer child, peak_row, peak_col, peak_mag, noise_est, test_stat. */
  if (corr2d_is_detector (s))
    b += 5 * sizeof (uint64_t) + 4 * sizeof (float)
         + dp_f32_framer_state_bytes (&s->framer);
  return b;
}

void
dp_correlator_get_state (const dp_correlator_state_t *s, void *blob)
{
  if (corr2d_is_search (s))
    {
      dp__search_get_state (s, blob);
      return;
    }
  DP_GET_OPEN (CORRELATOR_STATE_MAGIC, CORRELATOR_STATE_VERSION,
               dp_correlator_state_bytes (s));
  dp_w_u64 (&_w, (uint64_t)s->flavor);
  dp_w_u64 (&_w, s->ny);
  dp_w_u64 (&_w, s->ny_out);
  dp_w_u64 (&_w, (uint64_t)s->fast_path);
  dp_w_u64 (&_w, (uint64_t)(int64_t)s->col_out);
  dp_w_u64 (&_w, s->nx_out);
  dp_w_u64 (&_w, s->count);
  dp_w_cf32 (&_w, s->accum, s->n);
  if (!corr2d_is_detector (s))
    return;
  dp_w_u64 (&_w, s->noise_lo);
  dp_w_u64 (&_w, s->noise_hi);
  dp_w_u64 (&_w, (uint64_t)s->noise_mode);
  dp_w_f32 (&_w, &s->threshold, 1);
  DP_W_CHILD (&_w, dp_f32_framer, &s->framer);
  dp_w_u64 (&_w, s->peak_row);
  dp_w_u64 (&_w, s->peak_col);
  dp_w_f32 (&_w, &s->peak_mag, 1);
  dp_w_f32 (&_w, &s->noise_est, 1);
  dp_w_f32 (&_w, &s->test_stat, 1);
}

/* A blob, read and checked whole, before anything is written: what
   dp_correlator_state_check returns and what set_state commits. */
typedef struct
{
  uint64_t    count;
  const void *accum; /* in place in the blob, n complex samples */
  float       thr;   /* detector flavors from here on */
  const void *fr;    /* the framer's child blob */
  uint64_t    row, col;
  float       last[3];
} corr2d_blob_t;

static int
corr2d_state_read (const dp_correlator_state_t *s, const void *blob,
                   corr2d_blob_t *b)
{
  DP_SET_OPEN (CORRELATOR_STATE_MAGIC, CORRELATOR_STATE_VERSION,
               dp_correlator_state_bytes (s));
  /* A count at or past the dwell would never dump: `count + 1 == dwell` is
     never true again until it wraps (#2142, doppler#2641). */
  const uint64_t flavor = dp_r_u64 (&_r);
  const uint64_t ny     = dp_r_u64 (&_r);
  const uint64_t ny_out = dp_r_u64 (&_r);
  const uint64_t fast   = dp_r_u64 (&_r);
  const uint64_t col    = dp_r_u64 (&_r);
  const uint64_t nx_out = dp_r_u64 (&_r);
  b->count              = dp_r_u64 (&_r);
  if (flavor != (uint64_t)s->flavor || ny != s->ny || ny_out != s->ny_out
      || fast != (uint64_t)s->fast_path || col != (uint64_t)(int64_t)s->col_out
      || nx_out != s->nx_out || b->count >= s->dwell)
    return DP_ERR_INVALID;
  b->accum = dp_r_reserve (&_r, s->n * sizeof (float _Complex));
  if (!b->accum)
    return DP_ERR_INVALID;
  if (!corr2d_is_detector (s))
    return DP_OK;

  /* A detector. Its noise window and mode are reject keys: they decide
     what the stored statistic means. The framer's child blob is checked
     without restoring it, and its frame count must name the dwell's
     position: push, reset and set_ref keep count == frames % dwell, so a
     blob that breaks it would put every later dump off by the difference,
     without a word (#2015's class, which acq refuses the same way). The
     last peak must be a cell of the map. */
  const uint64_t lo   = dp_r_u64 (&_r);
  const uint64_t hi   = dp_r_u64 (&_r);
  const uint64_t mode = dp_r_u64 (&_r);
  dp_r_f32 (&_r, &b->thr, 1);
  b->fr = dp_r_reserve (&_r, dp_f32_framer_state_bytes (&s->framer));
  uint64_t frames;
  if (lo != s->noise_lo || hi != s->noise_hi || mode != (uint64_t)s->noise_mode
      || !b->fr
      || dp_f32_framer_state_frames (&s->framer, b->fr, &frames) != DP_OK
      || frames % s->dwell != b->count)
    return DP_ERR_INVALID;
  b->row = dp_r_u64 (&_r);
  b->col = dp_r_u64 (&_r);
  dp_r_f32 (&_r, b->last, 3);
  if (b->row >= s->ny || b->col >= s->nx)
    return DP_ERR_INVALID;
  return DP_OK;
}

int
dp_correlator_state_check (const dp_correlator_state_t *s, const void *blob)
{
  if (corr2d_is_search (s))
    return dp__search_state_check (s, blob);
  corr2d_blob_t b;
  return corr2d_state_read (s, blob, &b);
}

int
dp_correlator_set_state (dp_correlator_state_t *s, const void *blob)
{
  if (corr2d_is_search (s))
    return dp__search_set_state (s, blob);
  /* Read and checked whole first, so a refused blob changes nothing. */
  corr2d_blob_t b;
  if (corr2d_state_read (s, blob, &b) != DP_OK)
    return DP_ERR_INVALID;
  s->count = (size_t)b.count;
  memcpy (s->accum, b.accum, s->n * sizeof (float _Complex));
  if (!corr2d_is_detector (s))
    return DP_OK;
  /* Nothing below can refuse: the framer's blob passed the check its own
     set_state runs. */
  s->threshold = b.thr;
  (void)dp_f32_framer_set_state (&s->framer, b.fr);
  s->peak_row  = (size_t)b.row;
  s->peak_col  = (size_t)b.col;
  s->peak_mag  = b.last[0];
  s->noise_est = b.last[1];
  s->test_stat = b.last[2];
  /* det_out is not carried, so there is no last map to view, and
     consumed is per-call output, not state. */
  s->_last_corr_valid = 0;
  s->consumed         = 0;
  return DP_OK;
}

int
dp_correlator_set_ref (dp_correlator_state_t *state, const float _Complex *ref)
{
  /* §18: the 1-D and 2-D maps and both detectors (each has a set_ref
     test); a search has no forward plan for a reference, and the
     delay-Doppler map's tables were sized from its code at create. */
  const dp_correlator_flavor_t f = dp_correlator_flavor (state);
  if (f != DP_CORRELATOR_1D && f != DP_CORRELATOR_2D
      && f != DP_CORRELATOR_DETECTOR_1D && f != DP_CORRELATOR_DETECTOR_2D)
    return DP_ERR_UNSUPPORTED;
  /* A detector resets even when the reference is then refused, as
     CorrDetector2D always did: its carry and last dump belong to the old
     reference either way. */
  if (!corr2d_is_map (state))
    dp_correlator_reset (state);
  if (state->fast_path)
    {
      /* Mode is fixed for the object's lifetime (see the header doc
       * comment) — reject rather than silently truncating a ref that no
       * longer fits the single-row assumption row_ref_spec relies on. */
      if (!corr2d_is_single_row_ref (ref, state->ny, state->nx))
        return DP_ERR_INVALID;
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
  dp_correlator_reset (state);
  return 0;
}

size_t
dp_correlator_execute_max_out (dp_correlator_state_t *state)
{
  /* 0 off a map: execute() refuses there, and a detector's n_out is its
     own frame, not a map this would size (#2752 step 4, §18). */
  return corr2d_is_map (state) ? state->n_out : 0;
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
corr2d_row_inverse (const dp_correlator_state_t *s, size_t t,
                    const float _Complex *a, float _Complex *pad,
                    float _Complex *d)
{
  const size_t nx = s->nx, nxo = s->nx_out;
  if (nxo == nx)
    dp_fft_execute_cf32 (s->row_inv[t], a, nx, d, nx);
  else
    {
      dp_fft_pad_spectrum (a, nx, pad, nxo);
      dp_fft_execute_cf32 (s->row_inv[t], pad, nxo, d, nxo);
    }
  const float inv_nx = 1.0f / (float)nx;
  for (size_t k = 0; k < nxo; k++)
    d[k] *= inv_nx;
}

static void
corr2d_row (const dp_correlator_state_t *s, size_t t, const float _Complex *x,
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

/* Does the delay-Doppler flavor run its map rows first?  Then the dump
   frame's row stage inverts each row as it accumulates it (the map's own
   epoch stage, dp__dd_map_epoch1, while the row is hot) and the dump runs
   the map's column stage; slow first accumulates and runs the map whole. */
static int
corr2d_dd_fused (const dp_correlator_state_t *s)
{
  return s->dd_fused;
}

/* One compiled body for the delay-Doppler row, out of line. The tree
   builds with -ffast-math, so a compiler with baseline FMA fuses the row's
   multiply-adds differently in each caller it is inlined into: GCC 13 on
   AArch64 made execute()'s row and a batch's whole-map row differ in the
   last bit, and a batch map stopped equalling a fresh object's (#2831).
   Forbidding the inline and the clone keeps every caller on the one body,
   as loop_filter_core.c does for its gains (doppler#2103). */
#if defined(_MSC_VER)
#define CORR2D_ONE_BODY __declspec (noinline)
#elif defined(__GNUC__) && !defined(__clang__)
#define CORR2D_ONE_BODY __attribute__ ((noinline, noclone))
#else
#define CORR2D_ONE_BODY __attribute__ ((noinline))
#endif

/* One delay-Doppler row: accumulated; on the dump frame of a fused map,
   inverted through the map's epoch stage into blk and cleared. */
CORR2D_ONE_BODY static void
corr2d_dd_row (const dp_correlator_state_t *s, size_t t, size_t i,
               const float _Complex *x, float _Complex *a, float _Complex *w,
               float _Complex *blk, int dump)
{
  corr2d_row (s, t, x, a, w, NULL, NULL, 0);
  if (!dump || !corr2d_dd_fused (s))
    return;
  const dp__dd_tile_t tile = { 0, NULL, NULL, s->dd_rows };
  dp__dd_map_epoch1 (s->dd_map, (int)t, a, i, &tile, blk);
  memset (a, 0, s->nx * sizeof *a);
}

/* One fast-path frame, per-row stage: chunk t runs rows r0..r1-1 through
 * corr2d_row on worker t's plans.  The accumulator, spectrum and pad
 * surfaces are named here rather than read from the state, so execute()
 * runs the dwell's own and a batch runs its scratch.  Rows share nothing,
 * so chunks run concurrently with no synchronization. */
typedef struct
{
  dp_correlator_state_t *s;
  const float _Complex  *in;   /**< the frame, ny rows of nx              */
  float _Complex        *acc;  /**< accumulator, ny rows of nx            */
  float _Complex        *work; /**< forward spectra, ny rows of nx        */
  float _Complex        *pad;  /**< zero-pad rows of nx_out; NULL native  */
  float _Complex        *dst;  /**< dump destination, ny rows of nx_out   */
  int                    dump; /**< 1 on the frame that completes a dwell */
} corr2d_rows_t;

static void
corr2d_rows_body (size_t t, void *ctx)
{
  const corr2d_rows_t   *c  = (const corr2d_rows_t *)ctx;
  dp_correlator_state_t *s  = c->s;
  const size_t           nx = s->nx, nxo = s->nx_out;
  size_t                 r0, r1;
  corr2d_chunk (s->ny, t, s->nworkers, &r0, &r1);
  for (size_t i = r0; i < r1; i++)
    if (corr2d_is_dd (s))
      corr2d_dd_row (s, t, i, c->in + i * nx, c->acc + i * nx,
                     c->work + i * nx, c->work + i * nx, c->dump);
    else
      corr2d_row (s, t, c->in + i * nx, c->acc + i * nx, c->work + i * nx,
                  c->pad ? c->pad + i * nxo : NULL, c->dst + i * nxo, c->dump);
}

/* The delay-Doppler dump: the one block map (correlator_map.h) on the dwell's
 * product spectra `acc` (ny rows of nx, each already times the conjugated
 * code spectrum), every row at 1/nx onto its centered output row (zero
 * Doppler at row ny_out/2), in the schedule the map's rule picks (one tile,
 * one rate: rows first, today's order, at every ny_out), and acc cleared.
 * The map runs on the pool when slot < 0, or whole on worker `slot`'s plans
 * (a batch's map per worker); either way a row is the same arithmetic, so
 * the map is the same bit for bit.  `work` holds corr2d_dd_work() values. */
static void
corr2d_dd_none (void *ctx, size_t r, const float _Complex *out)
{
  (void)ctx, (void)r, (void)out; /* the map is already in out */
}

static size_t
corr2d_dd_work (const dp_correlator_state_t *s)
{
  const size_t w = dp__dd_map_work_len (s->dd_map, 1, 1);
  return w ? w : 1;
}

/* `blk` holds the dump frame's rows as the fused row stage inverted them
   (rows first); slow first reads `acc` instead and clears it. */
static void
corr2d_dd_dump (dp_correlator_state_t *s, int slot, float _Complex *acc,
                const float _Complex *blk, float _Complex *work,
                float _Complex *out)
{
  const dp__dd_tile_t tile = { 0, NULL, NULL, s->dd_rows };
  if (corr2d_dd_fused (s))
    {
      dp__dd_map_columns (s->dd_map, s->pool, slot, blk, NULL, &tile, 1, out);
      return;
    }
  (void)dp__dd_map_run (s->dd_map, s->pool, slot, acc, NULL, 1, &tile, 1, work,
                        out, corr2d_dd_none, NULL);
  memset (acc, 0, s->n * sizeof *acc);
}

/* Known-lag frame (col_out >= 0): row i's correlation at the one lag, a
 * time-domain dot product against the conjugated code, added into acc[i];
 * on a dump, acc's first ny entries go to dst and acc is cleared (all n of
 * it, as the state blob covers all n). */
static void
corr2d_col_frame (const dp_correlator_state_t *s, const float _Complex *in,
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
 * cancels to an exact identity and dp_correlator_execute reduces, per row i,
 * to
 *
 *   R(i,j) = IFFT_nx( FFT_nx(row_i) · conj(FFT_nx(ref_row0)) )(j) / nx
 *
 * — ny independent length-nx circular cross-correlations, normalized by
 * 1/nx (NOT 1/n = ny*nx: the row-axis orthogonality sum contributes the
 * extra factor of ny that turns 1/n into 1/nx — see the derivation).
 *
 * The rows are independent, so they run as nworkers chunks on the pool
 * (corr2d_rows_body); the delay-Doppler flavor then makes its map from the
 * accumulated product spectra (corr2d_dd_dump). */
static size_t
corr2d_execute_fast (dp_correlator_state_t *state, const float _Complex *in,
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

  /* Dump decided up front: Corr2D's per-row stage inverts each row as soon
     as it is accumulated, rather than in a second pass over the surface.
     The delay-Doppler flavor only accumulates, and the map makes the dump
     from the product spectra (slow time first, corr2d_dd_dump). */
  const int     dump = (state->count + 1 == state->dwell);
  corr2d_rows_t rows = { state,           in,  state->accum, state->work_fft,
                         state->work_pad, out, dump };
  dp_pool_run (state->pool, (size_t)state->nworkers, corr2d_rows_body, &rows);
  if (!dump)
    {
      state->count++;
      return 0;
    }
  if (corr2d_is_dd (state))
    corr2d_dd_dump (state, -1, state->accum, state->work_fft, state->dd_fine,
                    out);
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
corr2d_general_frame (const dp_correlator_state_t *s, const float _Complex *in,
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

/* One frame through the map flavors' and the detectors' shared path: the
   public execute minus its flavor refusal, which push must not meet. */
static size_t
corr2d_execute (dp_correlator_state_t *state, const float _Complex *in,
                size_t n_in, float _Complex *out, size_t max_out)
{
  /* n_in must equal ny*nx; refuse before touching the accumulator, so a
   * refused call is not counted toward the dwell (#1925, as
   * dp_correlator_execute).
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

  /* The 1-D flavor is one row on worker 0's plans, called directly: no
     pool dispatch and none of the other flavors' branches. Through
     corr2d_execute_fast the short frames ran 1-2% slower than Corr's own
     path did on main (n = 31 and 63, docs/design/correlator.md U1); this
     call is the same corr2d_row, so test_corr_equiv still holds it to the
     2-D flavor at one row byte for byte.  The 1-D detector is the same
     row, so it takes the same call (test_detector_equiv). */
  if (state->flavor == DP_CORRELATOR_1D
      || state->flavor == DP_CORRELATOR_DETECTOR_1D)
    {
      const int dump = (++state->count == state->dwell);
      corr2d_row (state, 0, in, state->accum, state->work_fft, state->work_pad,
                  dst, dump);
      if (!dump)
        return 0;
      state->count = 0;
      if (dst != out)
        memcpy (out, dst, cap * sizeof *out);
      return cap;
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

size_t
dp_correlator_execute (dp_correlator_state_t *state, const float _Complex *in,
                       size_t n_in, float _Complex *out, size_t max_out,
                       const char **why)
{
  if (!corr2d_is_map (state))
    return SIZE_MAX; /* off-flavor: the binding's declared sentence */
  const size_t got = corr2d_execute (state, in, n_in, out, max_out);
  if (got == SIZE_MAX && why) /* corr2d_execute refuses one thing */
    *why = "execute: takes exactly one frame: the input length must be the "
           "frame length n (ny*nx)";
  return got;
}

static dp_correlator_state_t *
corr_dd_create (const float _Complex *code, size_t code_len, size_t ny,
                size_t dwell, int nthreads, size_t ny_out, const char **why)
{
  const size_t nx  = code_len;
  const size_t nyo = ny_out ? ny_out : ny;
  if (!code || nx == 0)
    return NULL; /* the dispatcher refuses an empty ref first */
  if (ny == 0)
    CORR_WHY ("Correlator: doppler_rows must be at least 1 for a "
              "delay-Doppler map");
  if (dwell < 1)
    CORR_WHY ("Correlator: dwell must be at least 1 (frames a dump)");
  if (nyo < ny)
    CORR_WHY ("Correlator: ny_out pads the Doppler rows; it must be 0 "
              "(native) or at least doppler_rows");
  /* Every buffer below is at most ny_out x len complex samples (the
     frame, the accumulator, the map's rows and its scratch): a product
     that does not fit a size_t is refused by name before anything is
     sized from it, where it once allocated small and was written past
     (#2958). */
  size_t bytes = 0;
  if (!dp_size_mul (ny, nx, &bytes)
      || !dp_size_mul (bytes, sizeof (float _Complex), &bytes))
    CORR_WHY ("Correlator: doppler_rows asks for a map no size_t can hold");
  if (!dp_size_mul (nyo, nx, &bytes)
      || !dp_size_mul (bytes, sizeof (float _Complex), &bytes))
    CORR_WHY ("Correlator: ny_out asks for a map no size_t can hold");

  /* Every size below follows from the arguments just validated, so the
     allocations abort on OOM rather than unwind (clib_common.h). */
  dp_correlator_state_t *state = dp_xcalloc (1, sizeof (*state));
  const size_t           n     = ny * nx;
  state->work_fft              = dp_xmallocn (n, sizeof (*state->work_fft));
  state->accum                 = dp_xcalloc (n, sizeof (*state->accum));
  /* The delay axis is native (nx_out == nx), so the row stage is exactly
     Corr2D's fast path over a reference whose row 0 is the code. */
  /* Rows split into at most ny chunks, columns into at most one per tile:
     the larger is the most workers either stage can keep busy. */
  const size_t ntiles = corr2d_ntiles (nx);
  if (corr2d_build_fast (state, code, nx, nx, nthreads,
                         ny > ntiles ? ny : ntiles))
    {
      dp_correlator_destroy (state);
      return NULL; /* the native length could not be planned: memory */
    }

  /* The one block map (correlator_map.h): ny epochs of nx, read on ny_out
     slow rows, no code-rate ramp (F = 1), a slot per worker. */
  const dp__dd_geom_t geom = { nx, nx, ny, nyo, 1 };
  state->dd_map            = dp__dd_map_create (&geom, state->nworkers);
  if (!state->dd_map)
    {
      dp_correlator_destroy (state);
      return NULL; /* the map's tables could not be allocated */
    }
  state->dd_rows = dp_xmallocn (nyo, sizeof *state->dd_rows);
  /* The schedule, decided once: the rule's, for one tile and one rate.
     Rows first (fused) inverts into work_fft and needs no scratch of the
     map's own; only the run (slow first, or one slow row) does. */
  state->dd_fused
      = nyo > 1
        && dp__dd_map_pick (state->dd_map, 1, 1, 1) == DP__DD_ROWS_FIRST;
  if (!state->dd_fused)
    state->dd_fine
        = dp_xmallocn (corr2d_dd_work (state), sizeof (float _Complex));
  for (size_t i = 0; i < nyo; i++)
    state->dd_rows[i] = (i + nyo / 2) % nyo;

  state->flavor       = DP_CORRELATOR_DELAY_DOPPLER;
  state->fast_path    = 1;
  state->ny           = ny;
  state->nx           = nx;
  state->n            = n;
  state->ny_out       = nyo;
  state->nx_out       = nx;
  state->n_out        = nyo * nx;
  state->dwell        = dwell;
  state->count        = 0;
  state->col_out      = -1;
  state->doppler_step = 1.0 / ((double)nyo * (double)nx);
  return state;
}

dp_correlator_state_t *
dp_correlator_create_delay_doppler (const float _Complex *code,
                                    size_t code_len, size_t ny, size_t dwell,
                                    int nthreads, size_t ny_out)
{
  return corr_dd_create (code, code_len, ny, dwell, nthreads, ny_out, NULL);
}

/* ── Batches (#2683) ───────────────────────────────────────────────────────
 */

/* Frames in a batch of n_in samples, or 0 when it is empty or ragged, or
   when s is a detector flavor: every batch call tests this first, so 0 is
   how each of them refuses a detector (SIZE_MAX, nothing written). */
static size_t
corr2d_frames (const dp_correlator_state_t *s, size_t n_in)
{
  if (!corr2d_is_map (s))
    return 0;
  return (n_in % s->n) ? 0 : n_in / s->n;
}

/* The batch scratch: one slot per worker, made on the first batch.  The
   sizes are the object's own, already allocated once by create(), so the
   allocations abort on OOM (clib_common.h).  bat_acc is calloc'd: every
   frame helper leaves its accumulator zero again after a dump, which is
   what makes each map start from the zero a fresh object starts from. */
static void
corr2d_batch_alloc (dp_correlator_state_t *s)
{
  if (s->bat_acc)
    return;
  const size_t nw = (size_t)s->nworkers;
  /* Trusted (nw workers x a frame already allocated): an overflow aborts
     as the allocation itself would. */
  s->bat_acc  = dp_xcallocn (nw, s->n, sizeof *s->bat_acc);
  s->bat_work = dp_xmallocn (dp_size_mul_sat (nw, s->n), sizeof *s->bat_work);
  s->bat_map
      = dp_xmallocn (dp_size_mul_sat (nw, s->n_out), sizeof *s->bat_map);
  if (s->fast_path && s->nx_out != s->nx)
    s->bat_pad = dp_xmallocn (
        dp_size_mul_sat (dp_size_mul_sat (nw, s->ny), s->nx_out),
        sizeof *s->bat_pad);
  if (corr2d_is_dd (s) && !s->dd_fused)
    s->bat_fine = dp_xmallocn (dp_size_mul_sat (nw, corr2d_dd_work (s)),
                               sizeof *s->bat_fine);
}

/* One frame into slot 0 of the batch scratch, split across the workers as
   execute() splits it: the general and known-lag paths run serially, the
   fast path's rows (and, on a dump, the delay-Doppler columns) on the
   pool.  dst receives the map when dump is set. */
static void
corr2d_batch_frame (dp_correlator_state_t *s, const float _Complex *x,
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
      = { s, x, s->bat_acc, s->bat_work, s->bat_pad, dst, dump };
  dp_pool_run (s->pool, (size_t)s->nworkers, corr2d_rows_body, &rows);
  if (dump && corr2d_is_dd (s))
    corr2d_dd_dump (s, -1, s->bat_acc, s->bat_work, s->bat_fine, dst);
}

/* Whole maps, one per worker: chunk t makes the map of frame h0 + t on
   worker t's plans and scratch slot t, every row and then, for the
   delay-Doppler flavor, the map on worker t's map slot, serially.  A row
   is the same arithmetic on any worker's plans, so the map equals the
   split one bit for bit. */
typedef struct
{
  dp_correlator_state_t *s;
  const float _Complex  *in; /**< the batch                 */
  size_t                 h0; /**< frame of chunk 0          */
} corr2d_whole_t;

static void
corr2d_whole_body (size_t t, void *ctx)
{
  const corr2d_whole_t  *c  = (const corr2d_whole_t *)ctx;
  dp_correlator_state_t *s  = c->s;
  const size_t           nx = s->nx, nxo = s->nx_out, n = s->n;
  const float _Complex  *x    = c->in + (c->h0 + t) * n;
  float _Complex        *acc  = s->bat_acc + t * n;
  float _Complex        *work = s->bat_work + t * n;
  float _Complex *pad = s->bat_pad ? s->bat_pad + t * s->ny * nxo : NULL;
  float _Complex *map = s->bat_map + t * s->n_out;
  for (size_t i = 0; i < s->ny; i++)
    if (corr2d_is_dd (s))
      corr2d_dd_row (s, t, i, x + i * nx, acc + i * nx, work + i * nx,
                     work + i * nx, 1);
    else
      corr2d_row (s, t, x + i * nx, acc + i * nx, work + i * nx,
                  pad ? pad + i * nxo : NULL, map + i * nxo, 1);
  if (corr2d_is_dd (s))
    corr2d_dd_dump (s, (int)t, acc, work,
                    s->bat_fine ? s->bat_fine + t * corr2d_dd_work (s) : NULL,
                    map);
}

/* The coherent sum on the fast path, one dispatch for the whole batch:
   chunk t owns rows r0..r1-1 and takes each of them through every frame in
   h order, dumping on the last.  A row's accumulator sees the frames in the
   order a dwell would, so the sum is a fresh dwell-H object's bit for bit,
   and a short frame is not split once per frame. */
typedef struct
{
  dp_correlator_state_t *s;
  const float _Complex  *in;  /**< the batch                       */
  size_t                 H;   /**< frames                          */
  float _Complex        *dst; /**< the surface: map, or bat_work   */
} corr2d_coh_t;

static void
corr2d_coh_body (size_t t, void *ctx)
{
  const corr2d_coh_t    *c  = (const corr2d_coh_t *)ctx;
  dp_correlator_state_t *s  = c->s;
  const size_t           nx = s->nx, nxo = s->nx_out;
  size_t                 r0, r1;
  corr2d_chunk (s->ny, t, s->nworkers, &r0, &r1);
  for (size_t h = 0; h < c->H; h++)
    for (size_t i = r0; i < r1; i++)
      if (corr2d_is_dd (s))
        corr2d_dd_row (s, t, i, c->in + h * s->n + i * nx, s->bat_acc + i * nx,
                       s->bat_work + i * nx, s->bat_work + i * nx,
                       h + 1 == c->H);
      else
        corr2d_row (s, t, c->in + h * s->n + i * nx, s->bat_acc + i * nx,
                    s->bat_work + i * nx,
                    s->bat_pad ? s->bat_pad + i * nxo : NULL, c->dst + i * nxo,
                    h + 1 == c->H);
}

/* The one batch loop.  coherent = 0 makes every frame's map and folds each
   in h order; coherent = 1 sums the frames' product spectra and folds the
   one map it inverts, as h = 0.  The dwell is never read or written. */
static size_t
corr2d_batch (dp_correlator_state_t *s, const float _Complex *in, size_t n_in,
              int coherent, const dp_correlator_fold_t *fold)
{
  const size_t H = corr2d_frames (s, n_in);
  if (H == 0)
    return SIZE_MAX;
  corr2d_batch_alloc (s);
  const size_t n = s->n, nw = (size_t)s->nworkers;
  const int    split = s->fast_path && s->col_out < 0;
  if (coherent && split)
    {
      corr2d_coh_t c = { s, in, H, s->bat_map };
      dp_pool_run (s->pool, nw, corr2d_coh_body, &c);
      if (corr2d_is_dd (s))
        corr2d_dd_dump (s, -1, s->bat_acc, s->bat_work, s->bat_fine,
                        s->bat_map);
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
dp_correlator_execute_fold (dp_correlator_state_t *state,
                            const float _Complex *in, size_t n_in,
                            const dp_correlator_fold_t *fold)
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
     greater: a tie keeps the earlier, lower h.  The value is a ternary, not
     fmaxf: under -fno-finite-math-only fmaxf keeps its NaN rule, and GCC 16
     on x86 compiled it to a library call per cell (1.8x slower than the
     branch it replaced).  The ternary is a packed max on every compiler
     measured, and matches the old branch for a NaN exactly. */
  for (size_t k = 0; k < n; k++)
    {
      const float    p = corr2d_pow (map[k]);
      const float    q = pw[k];
      const uint32_t m = 0u - (uint32_t)(p > q);
      ix[k]            = (ix[k] & ~m) | (hh & m);
      pw[k]            = p > q ? p : q;
    }
}

typedef struct
{
  const dp_correlator_state_t *s;
  float                       *peak;
  uint32_t                    *row;
  uint32_t                    *col;
  float                       *mean;
} corr2d_peaks_t;

static void
corr2d_fold_peaks (void *ctx, size_t h, const float _Complex *map, size_t n)
{
  const corr2d_peaks_t *c = (const corr2d_peaks_t *)ctx;
  /* Two passes, both vectorized on every target: the peak and the sum, then
     the first cell holding the peak (row-major order wins a tie).  A power
     is a sum of squares, never negative, so its IEEE bits order as an
     unsigned integer, and an integer max is a packed reduction everywhere;
     a float max is not under -fno-finite-math-only (fmaxf keeps its NaN
     rule, and GCC 16 on x86 made it a library call per cell).  The second
     pass matches bits, so it stops at the peak's cell whatever the peak is;
     only a map holding a NaN reads differently from the old branch. */
  uint32_t pkb = 0;
  double   sum = 0.0;
  for (size_t k = 0; k < n; k++)
    {
      const float p = corr2d_pow (map[k]);
      uint32_t    b;
      memcpy (&b, &p, sizeof b);
      sum += p;
      pkb = b > pkb ? b : pkb;
    }
  size_t best = 0;
  for (;; best++)
    {
      const float p = corr2d_pow (map[best]);
      uint32_t    b;
      memcpy (&b, &p, sizeof b);
      if (b == pkb)
        break;
    }
  float pk;
  memcpy (&pk, &pkb, sizeof pk);
  const int    col_out = c->s->col_out >= 0;
  const size_t width   = col_out ? 1 : c->s->nx_out;
  c->peak[h]           = pk;
  c->row[h]            = (uint32_t)(best / width);
  c->col[h]  = col_out ? (uint32_t)c->s->col_out : (uint32_t)(best % width);
  c->mean[h] = (float)(sum / (double)n);
}

size_t
dp_correlator_execute_all_max_out (const dp_correlator_state_t *state,
                                   size_t                       n_in)
{
  /* Off a map, 0: a search's n is 0 (its constructor never sets it), and
     execute_all() refuses there whatever this sizes (§18). */
  if (!corr2d_is_map (state))
    return 0;
  const size_t H = n_in / state->n;
  size_t       need;
  return dp_size_mul (H, state->n_out, &need) ? need : SIZE_MAX;
}

/* Why a batch refused, for its `why` (just-makeit#2574): `frames` and
   `out` are the method's own sentences. Off a map the binding raises the
   method's declared "only" sentence, so nothing is written there. */
static void
corr2d_batch_why (const dp_correlator_state_t *s, size_t n_in, size_t need,
                  size_t max_out, const char *frames, const char *out,
                  const char **why)
{
  if (!why || !corr2d_is_map (s))
    return;
  *why = (corr2d_frames (s, n_in) == 0 || need == SIZE_MAX) ? frames
         : max_out < need                                   ? out
                                                            : NULL;
}

size_t
dp_correlator_execute_all (dp_correlator_state_t *state,
                           const float _Complex *in, size_t n_in,
                           float _Complex *out, size_t max_out,
                           const char **why)
{
  const size_t need = dp_correlator_execute_all_max_out (state, n_in);
  if (corr2d_frames (state, n_in) == 0 || need == SIZE_MAX || max_out < need)
    {
      corr2d_batch_why (state, n_in, need, max_out,
                        "execute_all: the input must be a whole, nonzero "
                        "number of frames (multiples of ny*nx)",
                        "execute_all: out= must hold every map (frames x "
                        "n_out values)",
                        why);
      return SIZE_MAX;
    }
  dp_correlator_fold_t f = { corr2d_fold_copy, out };
  corr2d_batch (state, in, n_in, 0, &f);
  return need;
}

size_t
dp_correlator_execute_coherent (dp_correlator_state_t *state,
                                const float _Complex *in, size_t n_in,
                                float _Complex *out, size_t max_out,
                                const char **why)
{
  if (corr2d_frames (state, n_in) == 0 || max_out < state->n_out)
    {
      corr2d_batch_why (state, n_in, state->n_out, max_out,
                        "execute_coherent: the input must be a whole, "
                        "nonzero number of frames (multiples of ny*nx)",
                        "execute_coherent: out= must hold one map (n_out "
                        "values)",
                        why);
      return SIZE_MAX;
    }
  dp_correlator_fold_t f = { corr2d_fold_copy, out };
  corr2d_batch (state, in, n_in, 1, &f);
  return state->n_out;
}

size_t
dp_correlator_execute_power_sum (dp_correlator_state_t *state,
                                 const float _Complex *in, size_t n_in,
                                 float *out, size_t max_out, const char **why)
{
  if (corr2d_frames (state, n_in) == 0 || max_out < state->n_out)
    {
      corr2d_batch_why (state, n_in, state->n_out, max_out,
                        "execute_power_sum: the input must be a whole, "
                        "nonzero number of frames (multiples of ny*nx)",
                        "execute_power_sum: out= must hold one map (n_out "
                        "values)",
                        why);
      return SIZE_MAX;
    }
  dp_correlator_fold_t f = { corr2d_fold_power_sum, out };
  corr2d_batch (state, in, n_in, 0, &f);
  return state->n_out;
}

size_t
dp_correlator_execute_power_max (dp_correlator_state_t *state,
                                 const float _Complex *in, size_t n_in,
                                 float *out, uint32_t *out1, size_t max_out,
                                 const char **why)
{
  const size_t H = corr2d_frames (state, n_in);
  /* The index is a uint32: map H - 1 must fit in one. */
  if (H == 0 || H - 1 > UINT32_MAX || max_out < state->n_out)
    {
      if (H != 0 && H - 1 > UINT32_MAX && why)
        *why = "execute_power_max: at most 2**32 frames (the index of the "
               "winning map is a uint32)";
      else
        corr2d_batch_why (state, n_in, state->n_out, max_out,
                          "execute_power_max: the input must be a whole, "
                          "nonzero number of frames (multiples of ny*nx)",
                          "execute_power_max: out= must hold one map (n_out "
                          "values)",
                          why);
      return SIZE_MAX;
    }
  corr2d_max_t         c = { out, out1 };
  dp_correlator_fold_t f = { corr2d_fold_power_max, &c };
  corr2d_batch (state, in, n_in, 0, &f);
  return state->n_out;
}

size_t
dp_correlator_execute_peaks_max_out (const dp_correlator_state_t *state,
                                     size_t                       n_in)
{
  /* As execute_all_max_out: a search's n is 0 (§18). */
  return corr2d_is_map (state) ? n_in / state->n : 0;
}

size_t
dp_correlator_execute_peaks (dp_correlator_state_t *state,
                             const float _Complex *in, size_t n_in, float *out,
                             uint32_t *out1, uint32_t *out2, float *out3,
                             size_t max_out, const char **why)
{
  const size_t H = corr2d_frames (state, n_in);
  if (H == 0 || max_out < H)
    {
      corr2d_batch_why (state, n_in, H, max_out,
                        "execute_peaks: the input must be a whole, nonzero "
                        "number of frames (multiples of ny*nx)",
                        "execute_peaks: out= must hold one record per frame",
                        why);
      return SIZE_MAX;
    }
  corr2d_peaks_t       c = { state, out, out1, out2, out3 };
  dp_correlator_fold_t f = { corr2d_fold_peaks, &c };
  corr2d_batch (state, in, n_in, 0, &f);
  return H;
}

/* The three reductions to one map each write n_out values. */
size_t
dp_correlator_execute_coherent_max_out (const dp_correlator_state_t *state)
{
  return state->n_out;
}

size_t
dp_correlator_execute_power_sum_max_out (const dp_correlator_state_t *state)
{
  return state->n_out;
}

size_t
dp_correlator_execute_power_max_max_out (const dp_correlator_state_t *state)
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
/* The bank's argument rules, and the cause a refused call names through its
   `why` (just-makeit#2574): corr2d_bank_check returns BANK_OK or the first
   rule broken, so each rule is stated once. BANK_OUT and the scratch's
   causes come from the entry points and corr2d_bank_alloc. */
enum
{
  BANK_OK,
  BANK_FRAME,
  BANK_L,
  BANK_G,
  BANK_RATES,
  BANK_INDEX,
  BANK_OUT,
  BANK_SCRATCH_L,     /* the fractions' scratch (L x n) cannot be counted */
  BANK_SCRATCH_RATES, /* the maps' scratch (rates x n_out) cannot be counted */
  BANK_SCRATCH_MEMORY /* the scratch could not be allocated */
};

static int
corr2d_bank_check (const dp_correlator_state_t *s, size_t x_len, size_t L,
                   int g_min, int g_max, size_t rates_len, corr2d_bank_t *b)
{
  if (x_len != s->n)
    return BANK_FRAME;
  if (L == 0)
    return BANK_L;
  if (g_min > g_max)
    return BANK_G;
  if (rates_len % s->ny != 0)
    return BANK_RATES;
  const size_t C = (size_t)((long long)g_max - (long long)g_min + 1);
  const size_t R = rates_len ? rates_len / s->ny : 1;
  if (C > UINT32_MAX / R)
    return BANK_INDEX;
  b->L     = L;
  b->g_min = g_min;
  b->C     = C;
  b->R     = R;
  b->rates = NULL;
  return BANK_OK;
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
  float _Complex *fine;  /**< L x R x ny_out x nx: fraction j's slow time
                              under rate r, made once (delay-Doppler)     */
  unsigned char  *have_fine; /**< L x R flags for @ref fine              */
  float _Complex *wt;  /**< nx: the code's weights at 1/nx              */
  float _Complex *blk; /**< n: one carrier's inverted epochs (rows
                            first, delay-Doppler)                       */
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
  free (m->fine);
  free (m->have_fine);
  free (m->wt);
  free (m->blk);
}

/* The fraction spectra, phases and flags follow the caller's L, and the
   maps its rate count, so a failure there is a refusal (0), never an abort.
   The rest is the object's own size, already allocated once by create(), so
   it goes through the abort-on-OOM helpers (clib_common.h). */
static int
corr2d_bank_alloc (const dp_correlator_state_t *s, const corr2d_bank_t *b,
                   int slow_first, corr2d_bank_mem_t *m)
{
  const size_t nw = (size_t)s->nworkers, L = b->L;
  memset (m, 0, sizeof *m);
  size_t spec_bytes, maps_bytes;
  if (!dp_size_mul (L, s->n, &spec_bytes)
      || !dp_size_mul (spec_bytes, sizeof (float _Complex), &spec_bytes))
    return BANK_SCRATCH_L;
  if (!dp_size_mul (b->R, s->n_out, &maps_bytes)
      || !dp_size_mul (maps_bytes, sizeof (float _Complex), &maps_bytes))
    return BANK_SCRATCH_RATES;
  m->spec  = dp_mallocn (dp_size_mul_sat (L, s->n), sizeof *m->spec);
  m->have  = calloc (L, 1);
  m->phase = dp_mallocn (dp_size_mul_sat (L, s->nx), sizeof *m->phase);
  m->maps  = dp_mallocn (dp_size_mul_sat (b->R, s->n_out), sizeof *m->maps);
  int ok   = m->spec && m->have && m->phase && m->maps;
  if (ok && slow_first)
    {
      /* Every fraction's slow time under every rate, made once and shared
         by the whole-bin carriers that roll it: slow first only. */
      const size_t fl = dp__dd_map_fine_len (s->dd_map);
      m->fine = dp_mallocn (dp_size_mul_sat (dp_size_mul_sat (L, b->R), fl),
                            sizeof *m->fine);
      m->have_fine = dp_callocn (L, b->R, 1);
      ok           = m->fine && m->have_fine;
    }
  if (!ok)
    {
      corr2d_bank_free (m);
      return BANK_SCRATCH_MEMORY;
    }
  if (corr2d_is_dd (s))
    {
      /* The object's own sizes, allocated once by create(): abort on OOM
         (clib_common.h), as m->prod is. */
      m->wt  = dp_xmallocn (s->nx, sizeof *m->wt);
      m->blk = dp_xmallocn (s->n, sizeof *m->blk);
      for (size_t q = 0; q < s->nx; q++)
        m->wt[q] = s->row_ref_spec[q] / (float)s->nx;
    }
  m->prod = dp_xmallocn (dp_size_mul_sat (nw, s->nx), sizeof *m->prod);
  if (s->nx_out != s->nx)
    m->pad = dp_xmallocn (dp_size_mul_sat (nw, s->nx_out), sizeof *m->pad);
  m->surf = dp_xmallocn (dp_size_mul_sat (s->ny, s->nx_out), sizeof *m->surf);
  if (!(s->fast_path && s->col_out < 0))
    {
      m->acc  = dp_xcalloc (s->n, sizeof *m->acc);
      m->copy = dp_xmallocn (s->n, sizeof *m->copy);
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
  return BANK_OK;
}

/* Fraction j's row spectra: chunk t mixes rows r0..r1-1 and transforms them
   on worker t's forward plan.  j = 0 is not mixed at all, so the whole-bin
   carriers see exactly the frame execute() sees. */
typedef struct
{
  const dp_correlator_state_t *s;
  const float _Complex        *x;
  const float _Complex        *phase; /**< nx values, or NULL for j = 0 */
  float _Complex              *spec;  /**< ny x nx                      */
  float _Complex              *prod;  /**< per-worker mix scratch       */
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
  const dp_correlator_state_t *s;
  const float _Complex        *spec; /**< fraction j's ny x nx spectra */
  size_t                       k;    /**< the roll, 0 .. nx-1          */
  corr2d_bank_mem_t           *m;
  float _Complex              *dst; /**< ny x nx_out                  */
} corr2d_bank_rows_t;

static void
corr2d_bank_rows_body (size_t t, void *ctx)
{
  const corr2d_bank_rows_t    *c  = (const corr2d_bank_rows_t *)ctx;
  const dp_correlator_state_t *s  = c->s;
  const size_t                 nx = s->nx, nxo = s->nx_out, k = c->k;
  float _Complex              *a   = c->m->prod + t * nx;
  float _Complex              *pad = c->m->pad ? c->m->pad + t * nxo : NULL;
  size_t                       r0, r1;
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

/* Run a stage's chunks in order on the caller.  Same chunks, same per-worker
   plans, so a bank is bit-identical for any worker count; but one frame is
   never split across the pool, which costs more than it saves at the frame
   lengths a bank serves (corr2d-hypotheses.md section 5: short frames run on
   one thread).  Parallel banks would split hypotheses, not a frame. */
static void
corr2d_bank_stage (const dp_correlator_state_t *s,
                   void (*body) (size_t, void *), void *ctx)
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
corr2d_bank_unshared (const dp_correlator_state_t *s, const float _Complex *x,
                      const corr2d_bank_t *b, const dp_correlator_fold_t *fold,
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

/* dp__dd_map_run's fold for one bank carrier: rate r's map is hypothesis
   h0 + r. */
typedef struct
{
  const dp_correlator_fold_t *fold;
  size_t                      h0;
  size_t                      n_out;
} corr2d_bank_fold_t;

static void
corr2d_bank_fold_one (void *ctx, size_t r, const float _Complex *out)
{
  const corr2d_bank_fold_t *f = (const corr2d_bank_fold_t *)ctx;
  f->fold->fold (f->fold->ctx, f->h0 + r, out, f->n_out);
}

/* SIZE_MAX when the scratch cannot be made, its cause (a BANK_SCRATCH_*)
   in *cause; the hypothesis count otherwise. */
static size_t
corr2d_bank_run (const dp_correlator_state_t *s, const float _Complex *x,
                 const corr2d_bank_t *b, const dp_correlator_fold_t *fold,
                 int *cause)
{
  /* The delay-Doppler map's schedule, decided once by its rule over the
     carriers one fraction's slow time serves and the rates (correlator_map.h);
     one slow row is the run's own case. */
  const int slow_first
      = corr2d_is_dd (s) && s->ny_out > 1
        && dp__dd_map_pick (s->dd_map, (b->C + b->L - 1) / b->L, b->R, 0)
               == DP__DD_SLOW_FIRST;
  corr2d_bank_mem_t m;
  const int         made = corr2d_bank_alloc (s, b, slow_first, &m);
  if (made != BANK_OK)
    {
      *cause = made;
      return SIZE_MAX;
    }
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
      if (corr2d_is_dd (s) && !slow_first)
        {
          /* The one block map's own driver, on one carrier: rows first,
             each carrier's inverses shared by its rates (at one tile the
             rule always picks it), or one slow row.  One frame is never
             split across the pool, so it runs whole on slot 0; each rate's
             map is folded as it is made, as h = c R + r. */
          const dp__dd_tile_t tile = { roll, NULL, m.wt, s->dd_rows };
          corr2d_bank_fold_t  f    = { fold, c * b->R, s->n_out };
          (void)dp__dd_map_run (s->dd_map, NULL, 0, m.spec + j * n, b->rates,
                                b->R, &tile, 1, m.blk, m.maps,
                                corr2d_bank_fold_one, &f);
          continue;
        }
      if (corr2d_is_dd (s))
        {
          /* Slow first: fraction j's slow time under rate r, made once and
             shared by every whole-bin carrier of the fraction, then this
             carrier's rows.  The stages, not the run: the carriers that
             share a fraction are not adjacent in the fold's h order
             (carrier outer), so the run's per-call slow time could not be
             shared across them (docs/design/acq-surface.md §18.1). */
          const size_t        fl   = dp__dd_map_fine_len (s->dd_map);
          const dp__dd_tile_t tile = { roll, NULL, m.wt, s->dd_rows };
          for (size_t r = 0; r < b->R; r++)
            {
              float _Complex       *S = m.fine + (j * b->R + r) * fl;
              const float _Complex *w = b->rates ? b->rates + r * s->ny : NULL;
              if (!m.have_fine[j * b->R + r])
                {
                  dp__dd_map_slow (s->dd_map, NULL, 0, m.spec + j * n, w, S);
                  m.have_fine[j * b->R + r] = 1;
                }
              dp__dd_map_rows (s->dd_map, NULL, 0, S, &tile, 1,
                               m.maps + r * s->n_out);
            }
        }
      else
        {
          /* Corr2D with rates keeps the carrier's rows in the surface and
             makes one map per rate from them. */
          float _Complex    *dst  = b->rates ? m.surf : m.maps;
          corr2d_bank_rows_t rows = { s, m.spec + j * n, roll, &m, dst };
          corr2d_bank_stage (s, corr2d_bank_rows_body, &rows);
        }
      if (!corr2d_is_dd (s) && b->rates)
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
dp_correlator_bank_fold (const dp_correlator_state_t *state,
                         const float _Complex *x, size_t x_len, size_t L,
                         int g_min, int g_max, const float _Complex *rates,
                         size_t rates_len, const dp_correlator_fold_t *fold)
{
  /* A detector has no bank. Refused here, at each entry, and not in
     corr2d_bank_check: that helper is inlined into all three, and the one
     extra flavor test there changed how the whole bank compiles, 12%
     slower on bank_64x512_detect_l2_l4_1t (#2911, bisected on the bench
     box). */
  if (!corr2d_is_map (state))
    return SIZE_MAX;
  corr2d_bank_t b;
  int           cause;
  if (corr2d_bank_check (state, x_len, L, g_min, g_max, rates_len, &b)
      != BANK_OK)
    return SIZE_MAX;
  b.rates = rates_len ? rates : NULL;
  return corr2d_bank_run (state, x, &b, fold, &cause);
}

size_t
dp_correlator_bank_power_max_max_out (dp_correlator_state_t *state,
                                      size_t                 x_len)
{
  (void)x_len;
  return state->n_out;
}

/* A bank refusal, said once: its status (just-makeit#2584), memory being
   DP_ERR_MEMORY and every other cause the caller's argument, and its
   sentence. Both outputs may be NULL; SIZE_MAX always. */
static size_t
bank_refuse (int status_code, const char *sentence, int *status,
             const char **why)
{
  if (status)
    *status = status_code;
  if (why)
    *why = sentence;
  return SIZE_MAX;
}

/* The status of a bank cause: only the scratch's allocation is memory. */
static int
bank_status (int cause)
{
  return cause == BANK_SCRATCH_MEMORY ? DP_ERR_MEMORY : DP_ERR_INVALID;
}

size_t
dp_correlator_bank_power_max (const dp_correlator_state_t *state,
                              const float _Complex *x, size_t x_len, size_t L,
                              int g_min, int g_max,
                              const float _Complex *rates, size_t rates_len,
                              float *out, uint32_t *out1, size_t max_out,
                              int *status, const char **why)
{
  /* A detector has no bank. Refused here, at each entry, and not in
     corr2d_bank_check: that helper is inlined into all three, and the one
     extra flavor test there changed how the whole bank compiles, 12%
     slower on bank_64x512_detect_l2_l4_1t (#2911, bisected on the bench
     box). */
  if (!corr2d_is_map (state))
    return bank_refuse (DP_ERR_UNSUPPORTED, NULL, status, why);
  static const char *const said[] = {
    [BANK_FRAME] = "bank_power_max: takes exactly one frame (ny*nx samples)",
    [BANK_L]     = "bank_power_max: L must be at least 1",
    [BANK_G]     = "bank_power_max: g_min must not exceed g_max",
    [BANK_RATES] = "bank_power_max: rates must be a whole number of ny-long "
                   "vectors",
    [BANK_INDEX] = "bank_power_max: (g_max - g_min + 1) x the rate count must "
                   "fit the uint32 hypothesis index",
    [BANK_OUT]   = "bank_power_max: out= must hold one map (n_out values)",
    [BANK_SCRATCH_L]      = "bank_power_max: L asks for more fractions than "
                            "the bank's scratch can hold",
    [BANK_SCRATCH_RATES]  = "bank_power_max: rates ask for more maps than the "
                            "bank's scratch can hold",
    [BANK_SCRATCH_MEMORY] = "bank_power_max: the bank's scratch could not be "
                            "allocated (out of memory)",
  };
  corr2d_bank_t b;
  int cause = corr2d_bank_check (state, x_len, L, g_min, g_max, rates_len, &b);
  if (cause == BANK_OK && max_out < state->n_out)
    cause = BANK_OUT;
  if (cause != BANK_OK)
    return bank_refuse (bank_status (cause), said[cause], status, why);
  b.rates                = rates_len ? rates : NULL;
  corr2d_max_t         c = { out, out1 };
  dp_correlator_fold_t f = { corr2d_fold_power_max, &c };
  if (corr2d_bank_run (state, x, &b, &f, &cause) == SIZE_MAX)
    return bank_refuse (bank_status (cause), said[cause], status, why);
  return state->n_out;
}

/* ── Detect coarse, refine the candidate (corr2d-hypotheses.md section 6) */

/* The map value of carrier g (on a 1/(L nx) grid) and rate w at one cell,
   with no transform: row i's correlation at lag tau is the time-domain sum
   the known-column path computes, over the row mixed within its period;
   DelayDoppler then takes the slow-time DFT at the cell's Doppler bin.  It
   is the bank's map at that cell, up to float rounding.

   GCC's SLP vectorizer is off for this one function. GCC 16.2.1 on x86
   packs its complex multiply-accumulate into a 45-instruction loop a term
   where the scalar form is 38: +3.2% instructions and +4.8% cycles on
   bank_64x512_detect_l2_l4_1t (perf_event_open counts on the bench box),
   with the outputs bit-identical either way. Whether GCC packs it depends
   on the code around it, so the choice is pinned here; #2982 is the
   instructions-per-call ratchet that guards it. */
#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-tree-slp-vectorize")))
#endif
static double
corr2d_cell_power (const dp_correlator_state_t *s, const float _Complex *x,
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
      = corr2d_is_dd (s) ? (double)((row + nyo - nyo / 2) % nyo) : 0.0;
  double _Complex v = 0.0;
  for (size_t i = 0; i < ny; i++)
    {
      if (!corr2d_is_dd (s) && i != row)
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
      if (corr2d_is_dd (s))
        {
          const double a = -2.0 * M_PI * bin * (double)i / (double)nyo;
          sum *= cos (a) + I * sin (a);
        }
      v += sum;
    }
  return creal (v) * creal (v) + cimag (v) * cimag (v);
}

size_t
dp_correlator_bank_detect_max_out (dp_correlator_state_t *state, size_t x_len)
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
dp_correlator_bank_detect (const dp_correlator_state_t *state,
                           const float _Complex *x, size_t x_len, size_t L_d,
                           size_t L_r, int g_min, int g_max, float t_candidate,
                           float t_confirm, const float _Complex *rates,
                           size_t rates_len, float *out, int32_t *out1,
                           uint32_t *out2, uint32_t *out3, uint32_t *out4,
                           size_t max_out, int *status, const char **why)
{
  /* A detector has no bank. Refused here, at each entry, and not in
     corr2d_bank_check: that helper is inlined into all three, and the one
     extra flavor test there changed how the whole bank compiles, 12%
     slower on bank_64x512_detect_l2_l4_1t (#2911, bisected on the bench
     box). */
  if (!corr2d_is_map (state))
    return bank_refuse (DP_ERR_UNSUPPORTED, NULL, status, why);
  static const char *const said[] = {
    [BANK_FRAME] = "bank_detect: takes exactly one frame (ny*nx samples)",
    [BANK_L]     = "bank_detect: L_d must be at least 1",
    [BANK_G]     = "bank_detect: g_min must not exceed g_max",
    [BANK_RATES] = "bank_detect: rates must be a whole number of ny-long "
                   "vectors",
    [BANK_INDEX] = "bank_detect: (g_max - g_min + 1) x the rate count must "
                   "fit the uint32 hypothesis index",
    [BANK_OUT]   = "bank_detect: out= must hold one record per map cell",
    [BANK_SCRATCH_L]      = "bank_detect: L_d asks for more fractions than "
                            "the bank's scratch can hold",
    [BANK_SCRATCH_RATES]  = "bank_detect: rates ask for more maps than the "
                            "bank's scratch can hold",
    [BANK_SCRATCH_MEMORY] = "bank_detect: the bank's scratch could not be "
                            "allocated (out of memory)",
  };
  corr2d_bank_t b;
  int           cause
      = corr2d_bank_check (state, x_len, L_d, g_min, g_max, rates_len, &b);
  if (cause == BANK_OK && max_out < state->n_out)
    cause = BANK_OUT;
  /* L_d == 0 is the check's own BANK_L; a refine factor is the caller's. */
  if (L_d != 0 && (L_r < L_d || L_r % L_d != 0))
    return bank_refuse (DP_ERR_INVALID,
                        "bank_detect: L_r must be a positive multiple of L_d",
                        status, why);
  if (cause != BANK_OK)
    return bank_refuse (bank_status (cause), said[cause], status, why);
  const long long m = (long long)(L_r / L_d);
  /* every refined carrier must fit the int32 the record carries */
  if ((long long)g_min * m - (m - 1) < INT32_MIN
      || (long long)g_max * m + (m - 1) > INT32_MAX)
    return bank_refuse (DP_ERR_INVALID,
                        "bank_detect: every refined carrier (g x L_r / L_d) "
                        "must fit an int32",
                        status, why);
  b.rates            = rates_len ? rates : NULL;
  const size_t n_out = state->n_out, nx = state->nx;
  /* The map's own geometry: a known column is ny rows of one. */
  const size_t width = state->col_out >= 0 ? 1 : state->nx_out;
  const size_t rows  = n_out / width;
  /* The time-domain refine needs whole lags on a shared row stage. */
  const int direct
      = state->fast_path && state->col_out < 0 && state->nx_out == nx;
  /* The detector's own work: the coarse map's power and index, and the
     direct refine's code and phase. Out of memory is a refusal. */
  float           *pw   = dp_mallocn (n_out, sizeof *pw);
  uint32_t        *ix   = dp_mallocn (n_out, sizeof *ix);
  float _Complex  *code = direct ? dp_mallocn (nx, sizeof *code) : NULL;
  double _Complex *ph   = direct ? dp_mallocn (nx, sizeof *ph) : NULL;
  if (!pw || !ix || (direct && (!code || !ph)))
    {
      free (pw), free (ix), free (code), free (ph);
      return bank_refuse (DP_ERR_MEMORY,
                          "bank_detect: the detector's work buffers could "
                          "not be allocated (out of memory)",
                          status, why);
    }
  /* 1. Detect: the strongest cell over the coarse bank, and which carrier
     and rate it came from. */
  corr2d_max_t         c = { pw, ix };
  dp_correlator_fold_t f = { corr2d_fold_power_max, &c };
  if (corr2d_bank_run (state, x, &b, &f, &cause) == SIZE_MAX)
    {
      free (pw), free (ix), free (code), free (ph);
      return bank_refuse (bank_status (cause), said[cause], status, why);
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
              corr2d_bank_t        one = { L_r, (int)gr, 1, 1, w };
              corr2d_cell_t        cc  = { cell, 0.0 };
              dp_correlator_fold_t cf  = { corr2d_fold_cell, &cc };
              if (corr2d_bank_run (state, x, &one, &cf, &cause) == SIZE_MAX)
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
  if (!ok)
    /* The refine's scratch is the time-domain bank at L_r, one carrier:
       L_r x n the caller chose, or memory. */
    return bank_refuse (bank_status (cause),
                        cause == BANK_SCRATCH_L
                            ? "bank_detect: L_r asks for more fractions than "
                              "the refine's scratch can hold"
                            : "bank_detect: the refine's scratch could not "
                              "be allocated (out of memory)",
                        status, why);
  return nrec;
}

/* ── The one delay-Doppler block map (#2752 step 2, correlator_map.h)
 * ─────────
 *
 * Every stage fans over chunks fixed by the chunk index alone, and chunk t
 * runs on slot t: its own plans (forward N, slow F DI, inverse L) and its
 * own scratch. A pocketfft plan keeps scratch of its own, so plans are
 * never shared between chunks; the arithmetic of a row does not depend on
 * which chunk computes it, so the map is bit-identical at every count. */

/* Bins a slow-time chunk gathers at once (the column stage's transpose
   tile, CORR2D_COL_TILE, measured for exactly this access). */
#define DD_SLOW_TILE CORR2D_COL_TILE
/* Bins the row stage gathers at once: their fine rows stay in cache while
   every slow row of the group reads them (proto-2732's CHUNK). */
#define DD_GATHER 8
/* Rows the row stage gathers before inverting them. */
#define DD_ROW_GROUP 16

typedef struct
{
  dp_fft_state_t *fwd;  /* N, forward                         */
  dp_fft_state_t *slow; /* F DI, forward; NULL when F DI = 1  */
  dp_fft_state_t *inv;  /* L, inverse                         */
  float _Complex *col;  /* DD_SLOW_TILE x F DI, the zero tail
                           written by calloc and never again */
  float _Complex *prod; /* DD_ROW_GROUP x N gathered products */
  float _Complex *pad;  /* L, the padded product; NULL L = N  */
  /* Rows-first's column stage (DI > 1): */
  dp_fft_state_t *slowr; /* DI, forward                       */
  float _Complex *colr;  /* DD_SLOW_TILE x DI, zero tail      */
  float _Complex *colo;  /* DD_SLOW_TILE x DI, transformed    */
} dd_slot_t;

struct dp__dd_map
{
  dp__dd_geom_t   g;
  size_t          fdi;   /* F DI                              */
  int             slots; /* slots built                       */
  dd_slot_t      *slot;  /* [slots]                           */
  float           unit;  /* 1/N, the weight of a NULL wt      */
  float _Complex *tw;    /* F DI: e^{-j 2 pi m / (F DI)}, the
                            rows-first ramp's phases; NULL at
                            F DI = 1                          */
  int force;             /* -1 the rule; else a schedule      */
};

static void
dd_slot_free (dd_slot_t *s)
{
  dp_fft_destroy (s->fwd);
  dp_fft_destroy (s->slow);
  dp_fft_destroy (s->inv);
  dp_fft_destroy (s->slowr);
  free (s->colr);
  free (s->colo);
  free (s->col);
  free (s->prod);
  free (s->pad);
  memset (s, 0, sizeof *s);
}

/* One slot's plans and scratch; 0, or -1 with nothing left allocated. */
static int
dd_slot_build (const dp__dd_map_t *m, dd_slot_t *s)
{
  const dp__dd_geom_t *g = &m->g;
  memset (s, 0, sizeof *s);
  s->fwd  = dp_fft_create (g->n, -1, 1);
  s->inv  = dp_fft_create (g->l, +1, 1);
  s->prod = dp_mallocn (dp_size_mul_sat (DD_ROW_GROUP, g->n), sizeof *s->prod);
  int ok  = s->fwd && s->inv && s->prod;
  if (ok && m->fdi > 1)
    {
      s->slow = dp_fft_create (m->fdi, -1, 1);
      s->col  = dp_callocn (DD_SLOW_TILE, m->fdi, sizeof *s->col);
      ok      = s->slow && s->col;
    }
  if (ok && g->l != g->n)
    {
      s->pad = dp_mallocn (g->l, sizeof *s->pad);
      ok     = s->pad != NULL;
    }
  if (ok && g->di > 1)
    {
      s->slowr = dp_fft_create (g->di, -1, 1);
      s->colr  = dp_callocn (DD_SLOW_TILE, g->di, sizeof *s->colr);
      s->colo  = dp_mallocn (dp_size_mul_sat (DD_SLOW_TILE, g->di),
                             sizeof *s->colo);
      ok       = s->slowr && s->colr && s->colo;
    }
  if (!ok)
    dd_slot_free (s);
  return ok ? 0 : -1;
}

dp__dd_map_t *
dp__dd_map_create (const dp__dd_geom_t *g, int slots)
{
  if (!g || g->n == 0 || g->l < g->n || g->d == 0 || g->di < g->d
      || (g->f != 1 && g->f != DP__DD_MAP_FINE)
      || (g->di == 1 && g->f > 1) /* one slow row has no fine grid */
      || slots < 1)
    return NULL;
  size_t fdi;
  if (!dp_size_mul (g->f, g->di, &fdi))
    return NULL;
  dp__dd_map_t *m = calloc (1, sizeof *m);
  if (!m)
    return NULL;
  m->g     = *g;
  m->fdi   = g->f * g->di;
  m->unit  = 1.0f / (float)g->n;
  m->force = -1;
  if (m->fdi > 1)
    {
      m->tw = dp_mallocn (m->fdi, sizeof *m->tw);
      if (!m->tw)
        {
          free (m);
          return NULL;
        }
      for (size_t k = 0; k < m->fdi; k++)
        m->tw[k] = (float _Complex)cexp (-2.0 * M_PI * I * (double)k
                                         / (double)m->fdi);
    }
  if (dp__dd_map_reserve (m, slots) != 0)
    {
      dp__dd_map_destroy (m);
      return NULL;
    }
  return m;
}

int
dp__dd_map_reserve (dp__dd_map_t *m, int slots)
{
  if (slots <= m->slots)
    return 0;
  dd_slot_t *grown = dp_reallocn (m->slot, (size_t)slots, sizeof *grown);
  if (!grown)
    return -1;
  m->slot = grown;
  for (int t = m->slots; t < slots; t++)
    {
      if (dd_slot_build (m, &m->slot[t]) != 0)
        return -1; /* the slots built so far stand */
      m->slots = t + 1;
    }
  return 0;
}

void
dp__dd_map_destroy (dp__dd_map_t *m)
{
  if (!m)
    return;
  for (int t = 0; t < m->slots; t++)
    dd_slot_free (&m->slot[t]);
  free (m->slot);
  free (m->tw);
  free (m);
}

const dp__dd_geom_t *
dp__dd_map_geom (const dp__dd_map_t *m)
{
  return &m->g;
}

size_t
dp__dd_map_fine_len (const dp__dd_map_t *m)
{
  return m->fdi * m->g.n;
}

/* Chunks a stage fans into: one per slot the pool can run at once. */
static size_t
dd_chunks (const dp__dd_map_t *m, dp_pool_t *pool, size_t items)
{
  size_t k = (size_t)dp_pool_threads (pool);
  if (k > (size_t)m->slots)
    k = (size_t)m->slots;
  if (k > items)
    k = items;
  return k ? k : 1;
}

/* Run body over k chunks on the pool, or, given a slot, as one chunk of
   one on that slot. The body reads its slot and range from the context. */
typedef struct
{
  dp__dd_map_t         *m;
  size_t                k;    /* chunks                         */
  int                   slot; /* >= 0: one chunk, on this slot  */
  size_t                items;
  const float _Complex *in;
  const float _Complex *w;
  float _Complex       *out;
  const dp__dd_tile_t  *tiles;
} dd_run_t;

static void
dd_range (const dd_run_t *c, size_t t, size_t *a, size_t *b, dd_slot_t **s)
{
  if (c->slot >= 0)
    {
      *a = 0;
      *b = c->items;
      *s = &c->m->slot[c->slot];
      return;
    }
  *a = t * c->items / c->k;
  *b = (t + 1) * c->items / c->k;
  *s = &c->m->slot[t];
}

static void
dd_run (dd_run_t *c, dp_pool_t *pool, void (*body) (size_t, void *))
{
  if (c->slot >= 0)
    {
      body (0, c);
      return;
    }
  c->k = dd_chunks (c->m, pool, c->items);
  dp_pool_run (pool, c->k, body, c);
}

static void
dd_forward_body (size_t t, void *ctx)
{
  const dd_run_t *c = (const dd_run_t *)ctx;
  const size_t    N = c->m->g.n;
  size_t          a, b;
  dd_slot_t      *s;
  dd_range (c, t, &a, &b, &s);
  for (size_t k = a; k < b; k++)
    dp_fft_execute_cf32 (s->fwd, c->in + k * N, N, c->out + k * N, N);
}

void
dp__dd_map_forward (dp__dd_map_t *m, dp_pool_t *pool, int slot,
                    const float _Complex *x, float _Complex *X)
{
  dd_run_t c = { .m = m, .slot = slot, .items = m->g.d, .in = x, .out = X };
  dd_run (&c, pool, dd_forward_body);
}

/* The one slow-time transform of both schedules: nc columns, each the D
   values src[k stride + q] weighted per epoch by w (or not), gathered into
   the slot tile `col` of `len`-point columns whose rows D .. len - 1 stay
   zero (the pad, written by calloc and never again), and transformed by
   `plan` into dst + q len. Slow first runs it on capture bins at F DI
   points straight into S; rows first on lag columns at DI points, then
   scatters the result onto the tile's rows. */
static void
dd_col_ffts (dp_fft_state_t *plan, float _Complex *col, size_t len,
             const float _Complex *src, size_t stride, size_t nc, size_t D,
             const float _Complex *w, float _Complex *dst)
{
  for (size_t k = 0; k < D; k++)
    {
      const float _Complex *row = src + k * stride;
      if (w)
        for (size_t q = 0; q < nc; q++)
          col[q * len + k] = row[q] * w[k];
      else
        for (size_t q = 0; q < nc; q++)
          col[q * len + k] = row[q];
    }
  for (size_t q = 0; q < nc; q++)
    dp_fft_execute_cf32 (plan, col + q * len, len, dst + q * len, len);
}

/* Slow time for bins [p0, p0 + nb), straight into S's bin-major rows. */
static void
dd_slow_body (size_t t, void *ctx)
{
  const dd_run_t *c = (const dd_run_t *)ctx;
  const size_t    N = c->m->g.n, D = c->m->g.d, fdi = c->m->fdi;
  size_t          a, b;
  dd_slot_t      *s;
  dd_range (c, t, &a, &b, &s);
  for (size_t tile = a; tile < b; tile++)
    {
      const size_t p0 = tile * DD_SLOW_TILE;
      const size_t nb = N - p0 < DD_SLOW_TILE ? N - p0 : DD_SLOW_TILE;
      dd_col_ffts (s->slow, s->col, fdi, c->in + p0, N, nb, D, c->w,
                   c->out + p0 * fdi);
    }
}

void
dp__dd_map_slow (dp__dd_map_t *m, dp_pool_t *pool, int slot,
                 const float _Complex *X, const float _Complex *w,
                 float _Complex *S)
{
  dd_run_t c = { .m     = m,
                 .slot  = slot,
                 .items = (m->g.n + DD_SLOW_TILE - 1) / DD_SLOW_TILE,
                 .in    = X,
                 .w     = w,
                 .out   = S };
  dd_run (&c, pool, dd_slow_body);
}

/* A gathered product of N bins, padded to L the way dp_fft_pad_spectrum
   pads, and inverted (unnormalized) into dst: the one inverse both
   schedules end a row or an epoch with. */
static void
dd_pad_inverse (const dp__dd_map_t *m, dd_slot_t *s, float _Complex *pr,
                float _Complex *dst)
{
  const size_t N = m->g.n, L = m->g.l;
  if (s->pad)
    {
      dp_fft_pad_spectrum (pr, N, s->pad, L);
      pr = s->pad;
    }
  dp_fft_execute_cf32 (s->inv, pr, L, dst, L);
}

/* Rows [i0, i1) of one tile: gathered DD_GATHER bins at a time, each bin's
   fine row advancing by F a slow row with one compare to wrap it (no
   division in the loop), then padded and inverted row by row. `fdi` is S's
   fine rows a bin (1 when S is X) and `f` the step a slow row moves. */
static void
dd_tile_rows (const dp__dd_map_t *m, dd_slot_t *s, const float _Complex *S,
              size_t fdi, size_t f, const dp__dd_tile_t *tl, size_t i0,
              size_t i1, float _Complex *out, float _Complex *row1)
{
  const size_t N = m->g.n, L = m->g.l;
  for (size_t g0 = i0; g0 < i1; g0 += DD_ROW_GROUP)
    {
      const size_t g1 = i1 - g0 < DD_ROW_GROUP ? i1 : g0 + DD_ROW_GROUP;
      for (size_t j0 = 0; j0 < N; j0 += DD_GATHER)
        {
          const size_t          nc = N - j0 < DD_GATHER ? N - j0 : DD_GATHER;
          const float _Complex *col[DD_GATHER];
          float _Complex cw[DD_GATHER];
          size_t n0[DD_GATHER];
          for (size_t q = 0; q < nc; q++)
            {
              const size_t j = j0 + q;
              const size_t p
                  = j + tl->roll < N ? j + tl->roll : j + tl->roll - N;
              col[q] = S + p * fdi;
              cw[q]  = tl->wt ? tl->wt[j] : m->unit;
              /* The fine row slow row g0 reads, wrapped into [0, fdi). */
              n0[q] = ((tl->off ? (size_t)tl->off[j] : 0) + (f * g0) % fdi)
                      % fdi;
            }
          for (size_t i = g0; i < g1; i++)
            {
              float _Complex *pr = s->prod + (i - g0) * N + j0;
              for (size_t q = 0; q < nc; q++)
                {
                  pr[q] = col[q][n0[q]] * cw[q];
                  n0[q] += f;
                  if (n0[q] >= fdi)
                    n0[q] -= fdi;
                }
            }
        }
      for (size_t i = g0; i < g1; i++)
        dd_pad_inverse (m, s, s->prod + (i - g0) * N,
                        row1 ? row1 : out + tl->rows[i] * L);
    }
}

/* Rows over the flat index of every tile's DI rows, so chunks balance
   across tiles; a chunk's range is cut where a tile ends. */
static void
dd_rows_body (size_t t, void *ctx)
{
  const dd_run_t *c   = (const dd_run_t *)ctx;
  const size_t    DI  = c->m->g.di;
  const size_t    fdi = c->m->fdi;
  size_t          a, b;
  dd_slot_t      *s;
  dd_range (c, t, &a, &b, &s);
  while (a < b)
    {
      const size_t q = a / DI, i0 = a % DI;
      const size_t i1 = (b - q * DI < DI) ? b - q * DI : DI;
      dd_tile_rows (c->m, s, c->in, fdi, fdi > 1 ? c->m->g.f : 0, &c->tiles[q],
                    i0, i1, c->out, NULL);
      a = q * DI + i1;
    }
}

void
dp__dd_map_rows (dp__dd_map_t *m, dp_pool_t *pool, int slot,
                 const float _Complex *S, const dp__dd_tile_t *tiles,
                 size_t n_tiles, float _Complex *out)
{
  dd_run_t c = { .m     = m,
                 .slot  = slot,
                 .items = n_tiles * m->g.di,
                 .in    = S,
                 .out   = out,
                 .tiles = tiles };
  dd_run (&c, pool, dd_rows_body);
}

void
dp__dd_map_row1 (dp__dd_map_t *m, int slot, const float _Complex *X,
                 size_t roll, const float _Complex *wt, float _Complex *row)
{
  const dp__dd_tile_t tl = { roll, NULL, wt, NULL };
  dd_tile_rows (m, &m->slot[slot], X, 1, 0, &tl, 0, 1, NULL, row);
}

void
dp__dd_ramp_table (const dp__dd_geom_t *g, double dq,
                   const float _Complex *base, int32_t *off,
                   float _Complex *wt)
{
  const double fdi = (double)(g->f * g->di);
  const double mid = 0.5 * (double)(g->d - 1);
  for (size_t j = 0; j < g->n; j++)
    {
      const long r  = lround (fdi * dq * (double)dp_fftfreq_index (j, g->n)
                              / (double)g->n);
      const long fw = (long)(g->f * g->di);
      off[j]        = (int32_t)(((r % fw) + fw) % fw);
      wt[j] = base[j]
              * (float _Complex)cexp (2.0 * M_PI * I * mid * (double)r / fdi);
    }
}

/* ── Rows first: the same map in the other order ──────────────────────────
 *
 * The slow-time weight and the rounded ramp are per-epoch, per-bin factors,
 * so they commute with the per-epoch row transforms: each epoch's product
 * X_k[(j + roll) mod N] wt_j e^{-j 2 pi k off_j / (F DI)} is inverted first
 * and slow time taken last, down every lag column at DI points. That is the
 * same function as slow time first, whose fine-grid transform puts the
 * phase e^{-j 2 pi k r_j / (F DI)} on epoch k itself (r_j = off_j mod
 * F DI): the two differ by float rounding alone. The phases come from one
 * table of F DI unit phasors indexed by (k off_j) mod F DI, integers, so
 * every epoch's factor is exact to the table. */

/* Tile q's epochs [k0, k1): gathered, ramped, padded and inverted into the
   tile's D x L block. */
static void
dd_epoch (const dp__dd_map_t *m, dd_slot_t *s, const float _Complex *x,
          size_t k, const dp__dd_tile_t *tl, float _Complex *dst)
{
  const size_t    N = m->g.n, fdi = m->fdi, roll = tl->roll;
  float _Complex *pr = s->prod;
  /* The roll as two straight runs, and the weight and the ramp hoisted out
     of the loop: per bin, the same products in the same order. */
  const float _Complex *xa = x + roll, *xb = x + roll - N;
  const size_t          ja = N - roll;
  if (!tl->wt && !tl->off)
    {
      for (size_t j = 0; j < ja; j++)
        pr[j] = xa[j] * m->unit;
      for (size_t j = ja; j < N; j++)
        pr[j] = xb[j] * m->unit;
    }
  else if (!tl->off)
    {
      for (size_t j = 0; j < ja; j++)
        pr[j] = xa[j] * tl->wt[j];
      for (size_t j = ja; j < N; j++)
        pr[j] = xb[j] * tl->wt[j];
    }
  else
    for (size_t j = 0; j < N; j++)
      {
        const float _Complex v
            = (j < ja ? xa[j] : xb[j]) * (tl->wt ? tl->wt[j] : m->unit);
        pr[j] = v * m->tw[(k * (size_t)tl->off[j]) % fdi];
      }
  dd_pad_inverse (m, s, pr, dst);
}

static void
dd_epochs (const dp__dd_map_t *m, dd_slot_t *s, const float _Complex *X,
           const dp__dd_tile_t *tl, size_t k0, size_t k1, float _Complex *blk)
{
  for (size_t k = k0; k < k1; k++)
    dd_epoch (m, s, X + k * m->g.n, k, tl, blk + k * m->g.l);
}

void
dp__dd_map_epoch1 (dp__dd_map_t *m, int slot, const float _Complex *x,
                   size_t k, const dp__dd_tile_t *tl, float _Complex *dst)
{
  dd_epoch (m, &m->slot[slot], x, k, tl, dst);
}

static void
dd_epochs_body (size_t t, void *ctx)
{
  const dd_run_t *c = (const dd_run_t *)ctx;
  const size_t    D = c->m->g.d, L = c->m->g.l;
  size_t          a, b;
  dd_slot_t      *s;
  dd_range (c, t, &a, &b, &s);
  while (a < b)
    {
      const size_t q = a / D, k0 = a % D;
      const size_t k1 = (b - q * D < D) ? b - q * D : D;
      dd_epochs (c->m, s, c->in, &c->tiles[q], k0, k1, c->out + q * D * L);
      a = q * D + k1;
    }
}

void
dp__dd_map_epochs (dp__dd_map_t *m, dp_pool_t *pool, int slot,
                   const float _Complex *X, const dp__dd_tile_t *tiles,
                   size_t n_tiles, float _Complex *blk)
{
  dd_run_t c = { .m     = m,
                 .slot  = slot,
                 .items = n_tiles * m->g.d,
                 .in    = X,
                 .out   = blk,
                 .tiles = tiles };
  dd_run (&c, pool, dd_epochs_body);
}

/* Slow time down lag columns [l0, l0 + nc) of tile q's block
   (dd_col_ffts at DI points), scattered onto the tile's rows. */
static void
dd_columns_body (size_t t, void *ctx)
{
  const dd_run_t *c = (const dd_run_t *)ctx;
  const size_t    D = c->m->g.d, DI = c->m->g.di, L = c->m->g.l;
  const size_t    nt = (L + DD_SLOW_TILE - 1) / DD_SLOW_TILE;
  size_t          a, b;
  dd_slot_t      *s;
  dd_range (c, t, &a, &b, &s);
  for (size_t it = a; it < b; it++)
    {
      const size_t          q = it / nt, l0 = (it % nt) * DD_SLOW_TILE;
      const size_t          nc = L - l0 < DD_SLOW_TILE ? L - l0 : DD_SLOW_TILE;
      const float _Complex *blk  = c->in + q * D * L + l0;
      const size_t         *rows = c->tiles[q].rows;
      dd_col_ffts (s->slowr, s->colr, DI, blk, L, nc, D, c->w, s->colo);
      for (size_t i = 0; i < DI; i++)
        {
          float _Complex *dst = c->out + rows[i] * L + l0;
          for (size_t q2 = 0; q2 < nc; q2++)
            dst[q2] = s->colo[q2 * DI + i];
        }
    }
}

void
dp__dd_map_columns (dp__dd_map_t *m, dp_pool_t *pool, int slot,
                    const float _Complex *blk, const float _Complex *w,
                    const dp__dd_tile_t *tiles, size_t n_tiles,
                    float _Complex *out)
{
  const size_t nt = (m->g.l + DD_SLOW_TILE - 1) / DD_SLOW_TILE;
  dd_run_t     c  = { .m     = m,
                      .slot  = slot,
                      .items = n_tiles * nt,
                      .in    = blk,
                      .w     = w,
                      .out   = out,
                      .tiles = tiles };
  dd_run (&c, pool, dd_columns_body);
}

/* One slow row: each tile's one epoch through the epoch stage, inverted
   straight onto the tile's one row -- there is no slow time, so the
   epoch's inverse is the row. Fanned over tiles. */
static void
dd_rows1_body (size_t t, void *ctx)
{
  const dd_run_t *c = (const dd_run_t *)ctx;
  size_t          a, b;
  dd_slot_t      *s;
  dd_range (c, t, &a, &b, &s);
  for (size_t q = a; q < b; q++)
    dd_epoch (c->m, s, c->in, 0, &c->tiles[q],
              c->out + c->tiles[q].rows[0] * c->m->g.l);
}

/* The schedule rule's resolution: two counts within 1/16 of each other are
   a tie. A transform counted as n log2 n is that close to its cost at
   best -- radix, padding and memory traffic all move it -- so a smaller
   difference is not one the count can see (acq's 3-tile D = 150 block
   counts 0.08% apart, record §18.6). Not fitted to a machine: it states
   what the count can resolve. */
#define DD_COUNT_RESOLUTION (1.0 / 16.0)

/* n log2 n, the cost of one length-n transform; 0 for n <= 1. */
static double
dd_tcost (size_t n)
{
  return n > 1 ? (double)n * log2 ((double)n) : 0.0;
}

int
dp__dd_schedule (const dp__dd_geom_t *g, size_t n_tiles, size_t n_rates,
                 int fused)
{
  const double W = (double)n_tiles, R = (double)n_rates;
  const double D = (double)g->d, DI = (double)g->di;
  /* Slow first: N slow transforms of F DI and W DI inverses, per rate.
     Rows first: W D inverses once, then W L slow transforms of DI per
     rate -- the inverses left out for a fused caller, which runs them
     inside a frame stage it runs anyway, on rows still in cache. Rows first
     only when it counts fewer by more than DD_COUNT_RESOLUTION: within it
     the count cannot tell the two apart (n log2 n ignores radix and memory
     traffic), and such a tie goes to slow first, which on acq's working
     sets (blocks of megabytes) measured faster at every near-equal count
     (record §18.6). */
  const double slow
      = R
        * ((double)g->n * dd_tcost (g->f * g->di) + W * DI * dd_tcost (g->l));
  const double rows = (fused ? 0.0 : W * D * dd_tcost (g->l))
                      + R * W * (double)g->l * dd_tcost (g->di);
  return g->di <= 1 || rows < slow * (1.0 - DD_COUNT_RESOLUTION)
             ? DP__DD_ROWS_FIRST
             : DP__DD_SLOW_FIRST;
}

void
dp__dd_map_force (dp__dd_map_t *m, int schedule)
{
  m->force = schedule;
}

int
dp__dd_map_pick (const dp__dd_map_t *m, size_t n_tiles, size_t n_rates,
                 int fused)
{
  return m->force >= 0 ? m->force
                       : dp__dd_schedule (&m->g, n_tiles, n_rates, fused);
}

size_t
dp__dd_map_work_len (const dp__dd_map_t *m, size_t n_tiles, size_t n_rates)
{
  if (dp__dd_map_pick (m, n_tiles, n_rates, 0) == DP__DD_ROWS_FIRST)
    return m->g.di > 1 ? n_tiles * m->g.d * m->g.l : 0;
  return m->fdi > 1 ? m->fdi * m->g.n : 0;
}

int
dp__dd_map_run (dp__dd_map_t *m, dp_pool_t *pool, int slot,
                const float _Complex *X, const float _Complex *rates,
                size_t n_rates, const dp__dd_tile_t *tiles, size_t n_tiles,
                float _Complex *work, float _Complex *out, dp__dd_fold_fn fold,
                void *ctx)
{
  const size_t D = m->g.d, R = n_rates ? n_rates : 1;
  const int    sch = dp__dd_map_pick (m, n_tiles, R, 0);
  if (m->g.di <= 1 || m->fdi <= 1)
    {
      /* One slow row: no slow time in either order, and a rate's weight is
         one scalar an epoch of one: each tile's epoch is its row, through
         the epoch stage's straight gather. */
      for (size_t r = 0; r < R; r++)
        {
          dd_run_t c1 = { .m     = m,
                          .slot  = slot,
                          .items = n_tiles,
                          .in    = X,
                          .out   = out,
                          .tiles = tiles };
          dd_run (&c1, pool, dd_rows1_body);
          if (rates)
            for (size_t c = 0; c < n_tiles * m->g.l; c++)
              out[tiles[c / m->g.l].rows[0] * m->g.l + c % m->g.l]
                  *= rates[r * D];
          fold (ctx, r, out);
        }
      return DP__DD_ROWS_FIRST; /* one slow row: there is only one order */
    }
  if (sch == DP__DD_ROWS_FIRST)
    {
      dp__dd_map_epochs (m, pool, slot, X, tiles, n_tiles, work);
      for (size_t r = 0; r < R; r++)
        {
          dp__dd_map_columns (m, pool, slot, work,
                              rates ? rates + r * D : NULL, tiles, n_tiles,
                              out);
          fold (ctx, r, out);
        }
      return sch;
    }
  for (size_t r = 0; r < R; r++)
    {
      dp__dd_map_slow (m, pool, slot, X, rates ? rates + r * D : NULL, work);
      dp__dd_map_rows (m, pool, slot, work, tiles, n_tiles, out);
      fold (ctx, r, out);
    }
  return sch;
}

/* ── The search flavors (#2752 step 3.4b)
 * ────────────────────────────────────── The streaming DSSS acquisition
 * engine, burst and continuous: correlator_core.c until step 3.4b, moved here
 * whole (one rename table and one field map; PR #2930) so the correlator's
 * functions live in its one source.
 *
 * Owns the full receive-side acquisition pipeline: a raw cf32 ring, the
 * slow-time Doppler FFT, a single-row-reference 2-D code correlator (corr2d),
 * and a CFAR threshold gate whose grid (coherent depth, threshold,
 * non-coherent looks) is auto-configured from the physics — C/N0, chip rate,
 * and a target (Pfa, Pd) — using the detection-theory functions.  The CFAR
 * helpers
 * (det_noise_estimate, det_ring_create) are reused header-only from
 * correlator_private.h, the correlator's detection layer.
 */

/* sqrt(2/pi): maps the raw Rayleigh threshold eta into mean-estimator units,
 * since E|Rayleigh(1)| = sqrt(pi/2).  Its reciprocal recovers eta from the
 * mean-based test statistic for the per-sample SNR estimate. */
#define DP_CORRELATOR_SQRT_2_OVER_PI 0.7978845608028654f

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Converts a per-sample amplitude SNR back to an estimated C/N0 (dB-Hz) --
 * the exact inverse of search_create()'s sizing transform
 * (snr = sqrt(10^(cn0_dbhz/10) / fs)) -- so the reported figure is directly
 * comparable to the caller's own cn0_dbhz argument regardless of chip_rate
 * or sample rate, unlike a raw per-sample or coherently-integrated ratio
 * (both scale with the frame geometry and so aren't portable across
 * configurations).
 * amp_snr is always > 0 here: every call site sits behind a
 * test_stat > threshold gate, and threshold/eta_nc are always positive. */
static inline float
search_cn0_dbhz_from_amp_snr (float amp_snr, double fs)
{
  return (float)dp_det_snr_to_cn0 ((double)amp_snr, fs);
}

/* The chip phase a surface column reports, in chips:
 * dp_correlator_build_handoff()'s mapping, kept in one place so the surface
 * axis and the hand-off cannot drift apart. */
static inline double
search_chip_phase_of_col (size_t col, size_t code_len, size_t code_bins)
{
  const double cl = (double)code_len;
  return dp_fmod_pos (cl - (double)col * cl / (double)code_bins, cl);
}

/* The correlation mainlobe's half-width in surface columns: shape.zone is
 * counted in samples of the code period, and a padded lag axis has
 * lag_bins / code_bins columns a sample (rounded up, so a padded zone never
 * covers less of the mainlobe than the native one). */
static inline size_t
search_zone_cols (const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  return (srch->shape.zone * srch->lag_bins + srch->code_bins - 1)
         / srch->code_bins;
}

/* The chip phase a hit seeds a code loop with, in Dll's convention: the
 * inverted lag (search_chip_phase_of_col) advanced by the dwell's dilation.
 * Both dp_correlator_build_handoff() and the hit's own chip_phase (filled in
 * search_report_peaks) call this, so the two cannot drift apart.
 *
 * The dwell's dilation (doppler#1254): the non-coherent sum's peak is the
 * phase at the middle of the dwell; the seed is wanted at its end. A
 * positive Doppler runs the chip clock fast, so the code is AHEAD of the
 * peak by the drift over half the dwell -- n_noncoh looks of coherent_bins
 * epochs each, code_len chips per epoch. Uncoupled (no carrier set) there
 * is no drift to advance by, so the phase is the inverted lag alone. */
static double
search_handoff_chip_phase (const dp_correlator_state_t *state,
                           size_t doppler_bin, size_t code_phase,
                           size_t code_len)
{
  dp_correlator_search_t *const srch = state->search;
  double                        phase
      = search_chip_phase_of_col (code_phase, code_len, srch->lag_bins);
  if (srch->carrier_freq_hz > 0.0)
    {
      const double doppler_hz
          = dp_correlator_bin_doppler_hz (state, doppler_bin);
      const double dwell_chips = (double)srch->n_noncoh
                                 * (double)srch->coherent_bins
                                 * (double)code_len;
      phase                    = dp_fmod_pos (
          phase + doppler_hz / srch->carrier_freq_hz * 0.5 * dwell_chips,
          (double)code_len);
    }
  return phase;
}
/* Doppler-band mask: with a doppler_uncertainty prior the engine scans only
 * searched_bins rows centred on DC, so the peak search must match (else the
 * lowered Sidak threshold over-counts and realized Pfa exceeds target).
 * Returns 1 if the cell at flat index k is inside the scanned band.  The
 * folded distance |k_row| from DC (rows >coherent_bins/2 are negative
 * Doppler) must not exceed half = (searched_bins-1)/2.  When searched_bins ==
 * coherent_bins this admits every row (byte-identical to the full search). */
static inline int
search_in_doppler_band (const dp_correlator_state_t *st, size_t k)
{
  dp_correlator_search_t *const srch = st->search;
  /* Wideband mode always searches its whole window_bins grid by
   * construction (there is no further-narrowing prior beyond the windows
   * window_bins itself already tiles) -- every hypothesis is in-band. */
  if (srch->window_bins > 1)
    return 1;
  /* k indexes the (interpolated) SURFACE, so fold on the fine grid and
     compare against a band widened by the same factor -- otherwise
     interpolation would silently narrow the searched Doppler range. */
  const size_t rows = srch->coherent_bins * srch->interp;
  const size_t row  = k / srch->lag_bins;
  const size_t fold = (row <= rows - row) ? row : rows - row;
  /* The FULL native band is every row: its edge is +/-span = D/2 bins, so
     for an even D the Nyquist bin belongs to it, and for an odd D so does
     the outer half of the outermost bin. The narrowed-prior rule below is
     right only when the prior's edge is a bin CENTRE (search_searched_bins
     builds sb = 2*half+1 that way). Applied to the full band it dropped a
     whole native bin at even D -- 1/D of a uniform Doppler prior, never
     searched (doppler#1183). */
  if (srch->searched_bins >= srch->coherent_bins)
    return 1;
  return fold <= ((srch->searched_bins - 1) / 2) * srch->interp;
}

static size_t search_interp_for (size_t D, size_t window_bins);

/* The cells a dwell's maximum ranges over: the SURFACE rows
 * search_in_doppler_band() admits, times code_bins. This is the count the
 * Sidak split divides the false-alarm budget over, at every site that
 * prices a gate (search_commit_thresholds, search_ascend_n_noncoh, the burst
 * sizer), so a gate is priced over exactly the cells its peak search can
 * pick. Counting native cells instead fired every D > 1 engine at about
 * twice its `pfa` (doppler#2218; see DP_CORRELATOR_DOPPLER_INTERP).
 *
 * The rows are the band predicate's, counted: every row of the full band
 * and of every wideband tile (sb * interp), else the rows within
 * (sb - 1) / 2 native bins of DC on the fine grid, which is
 * (sb - 1) * interp + 1 of them. test_correlator_search.c pins this count to
 * the committed band_mask's, so the two cannot drift apart. */
static size_t
search_band_cells (size_t D, size_t sb, size_t window_bins, size_t cb)
{
  const size_t interp = search_interp_for (D, window_bins);
  const size_t rows
      = (window_bins > 1 || sb >= D) ? sb * interp : (sb - 1) * interp + 1;
  return rows * cb;
}

/* Doppler-axis interpolation of the SLOW-TIME FFT.
 *
 * 2 halves the worst-case scalloping loss of the slow-time transform from
 * ~3.9 dB (a peak exactly between two bins) to ~0.9 dB. That null was not a
 * margin a caller could buy back with signal: `test_stat` saturates against
 * the code's own autocorrelation-sidelobe floor, so a burst sitting at half
 * a bin was undetectable at ANY C/N0 (gh-1002).
 *
 * THIS FACTOR PADS THE SLOW-TIME TRANSFORM. The padded sequence is the
 * per-column series of epochs, whose transform is an ordinary spectral
 * estimate, and lengthening that transform samples the same DTFT more
 * finely. The code axis is never padded in TIME: the correlation along it
 * is CIRCULAR, so zero-padding the replica or the epoch would not be an
 * interpolation at all -- it would change the correlation being computed
 * (corr2d's own header states this: the forward FFT2 must stay at the
 * code-period size). The forward transform stays at `code_bins`; what a
 * padded lag axis pads is the CROSS-SPECTRUM, zero-padded from `code_bins`
 * to `lag_bins` bins before the inverse (dp_fft_pad_spectrum), which
 * interpolates the same circular correlation (lag_bins' doc in
 * search_core.h). Verified alongside: the reported code_phase is exact at
 * every lag across a whole period.
 *
 * It is done HERE and NOT via corr2d's `ny_out`. corr2d sees a single-row
 * reference (a code replica with no Doppler content) and takes its fast
 * path, in which the row axis provably cancels to an identity and is never
 * transformed at all -- so `ny_out` would force it onto the general 2-D path
 * and re-transform rows this engine has already transformed. The Doppler
 * axis is this engine's.
 *
 * Frequency-domain zero-padding is exact band-limited interpolation, so the
 * added rows carry no new INFORMATION -- but they are new CHANCES for a
 * maximum. The gate is applied to the largest cell of the interpolated
 * surface, and a half-bin row sits between two native ones with a
 * correlation of about 0.64 to each, which a deep-tail exceedance (a cell
 * probability near 1e-6) all but ignores: on noise the interpolated surface
 * behaves like ~0.96 * interp * n independent cells, not n. Sized from the
 * native count, every D > 1 engine fired at 1.86-1.93x its `pfa`
 * (doppler#2218, 1e6 dwells a row at D = 2..7, Zadoff-Chu and PN alike; a
 * native-only maximum read 0.94-0.99x). So the ladder is sized from the
 * cells the search ranges over, search_band_cells(). */
#define DP_CORRELATOR_DOPPLER_INTERP 2u

/* The native row a surface pick reports: the surface is the Doppler axis
 * interpolated `interp`-fold, and every consumer scales `doppler_bin` by
 * `doppler_res_hz`, so a pick on an interpolated row is reported at the
 * native row of the strongest native cell within its own zone (the two
 * native neighbours), at its column -- the same rule as before the list,
 * applied per peak instead of once to the global maximum. `interp == 1`
 * (wideband mode, and D == 1) is the identity. */
static size_t
search_native_row (const dp_correlator_state_t *st, const float *surf,
                   size_t row, size_t col)
{
  dp_correlator_search_t *const srch = st->search;
  if (srch->interp <= 1)
    return row;
  const size_t rows = srch->n_surf / srch->lag_bins;
  size_t       best = rows;
  for (size_t d = 0; d <= 2 * srch->interp; d++)
    {
      size_t r = (row + rows + d - srch->interp) % rows;
      if (r % srch->interp)
        continue;
      size_t k = r * srch->lag_bins + col;
      if (best == rows || surf[k] > surf[best * srch->lag_bins + col])
        best = r;
    }
  return best / srch->interp;
}

/* ── The decided surface's passes, per tile ─────────────────────────────
 *
 * Every pass that reads or writes a whole surface after the fan -- the
 * magnitude of the coherent dump, the CFAR reference, the working mask's
 * copy of the band mask, each scan of the peak list, the non-coherent
 * accumulate -- runs per CHUNK on the pool (docs/design/acq-surface.md
 * §13, #1243): the
 * surface is `window_bins` chunks of whole rows, cells [r*len, (r+1)*len),
 * one per tile, and a body writes only its chunk's cells and its own
 * dp_correlator_part_t slot. The merges are serial in chunk order, so the
 * result is bit-identical at any thread count and their cost is per tile, not
 * per cell. A single-tile engine has one chunk and no pool: the same code,
 * serially, and the classic result to the bit. */
typedef struct
{
  dp_correlator_state_t *st;
  const float           *surf; /* the surface being decided                 */
  int magnitude;               /* fill mag_buf from out_buf first           */
  int reference;               /* take the chunk's reference (not MEDIAN)   */
} search_scan_t;

static inline void
search_chunk (const dp_correlator_state_t *st, size_t r, size_t *k0,
              size_t *k1)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  len  = srch->n_surf / srch->window_bins;
  *k0                                = r * len;
  *k1                                = *k0 + len;
}

/* The first pass over a decided surface: the magnitude (when asked), the
 * working mask from the band mask, the reference and the first maximum
 * among the band's cells, all of this chunk. */
/* Which pool a tile stage fans on (#2775). A stage that runs every epoch
   (the roll, the non-coherent sum, and the decision and column stages at
   D = 1) fans unless the engine's epoch_fan is off; a per-block stage at
   D > 1 hands each wake-up a block's worth of work and always fans.
   epoch_fan is a composition's call, not a tile count: the same 3-tile
   D = 1 pool fans to 0.84 of serial on bare metal and to 3.5x on WSL2
   (async-dsss-receiver-measurements.md §9.7). Inline or fanned, every
   tile owns its rows and scratch, so the result is bit-identical. */
static dp_pool_t *
search_epoch_pool (const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  return srch->epoch_fan ? st->pool : NULL;
}

static dp_pool_t *
search_block_pool (const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  return srch->coherent_bins > 1 ? st->pool : search_epoch_pool (st);
}

static void
search_tile_decide (size_t r, void *ctx)
{
  const search_scan_t          *sc   = (const search_scan_t *)ctx;
  dp_correlator_state_t        *st   = sc->st;
  dp_correlator_search_t *const srch = st->search;
  size_t                        k0, k1;
  search_chunk (st, r, &k0, &k1);
  if (sc->magnitude)
    for (size_t k = k0; k < k1; k++)
      st->mag_buf[k] = cabsf (srch->out_buf[k]);
  memcpy (st->peak_mask + k0, st->band_mask + k0, k1 - k0);
  dp_correlator_part_t *p = &srch->parts[r];
  /* A tile has no scratch buffer, so it asks for the per-chunk aggregate,
     which cannot be a median. sc->reference is never set for MEDIAN anyway
     (search_scan_surface: it has no per-chunk form). */
  p->ref         = sc->reference
                       ? det_noise_chunk (sc->surf, k0, k1 - 1, st->noise_mode)
                       : 0.0f;
  const size_t b = det_peak_scan (sc->surf, st->band_mask, k0, k1);
  p->best        = b == k1 ? srch->n_surf : b;
}

/* A later scan of the list: the chunk's first maximum among the cells the
 * working mask -- the band and every listed pick's zone -- leaves. */
static void
search_tile_pick (size_t r, void *ctx)
{
  const search_scan_t          *sc   = (const search_scan_t *)ctx;
  dp_correlator_state_t        *st   = sc->st;
  dp_correlator_search_t *const srch = st->search;
  size_t                        k0, k1;
  search_chunk (st, r, &k0, &k1);
  const size_t b      = det_peak_scan (sc->surf, st->peak_mask, k0, k1);
  srch->parts[r].best = b == k1 ? srch->n_surf : b;
}

/* The non-coherent path's accumulate: |dump|^2 onto this chunk. */
static void
search_tile_nc_acc (size_t r, void *ctx)
{
  dp_correlator_state_t        *st   = ((const search_scan_t *)ctx)->st;
  dp_correlator_search_t *const srch = st->search;
  size_t                        k0, k1;
  search_chunk (st, r, &k0, &k1);
  for (size_t k = k0; k < k1; k++)
    {
      float m = cabsf (srch->out_buf[k]);
      srch->nc_surface[k] += m * m;
    }
}

/* The merged pick: the first of the chunks' first maxima, strict `>` in
 * chunk order -- det_peak_scan()'s own rule, so it is the cell one scan
 * over the whole surface picks. `n_surf` when no chunk had a candidate. */
static size_t
search_merge_best (const dp_correlator_state_t *st, const float *surf)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  n    = srch->n_surf;
  size_t                        best = n;
  for (size_t r = 0; r < srch->window_bins; r++)
    {
      const size_t b = srch->parts[r].best;
      if (b < n && (best == n || surf[b] > surf[best]))
        best = b;
    }
  return best;
}

/* The reference over a decided surface, and the decide pass that computes
 * it: the chunks' references merged -- the mean of equal-sized chunks'
 * means, the least of their minima, the largest of their maxima -- for
 * every mode but MEDIAN, which has no per-chunk form and runs the classic
 * serial estimate over the reference range instead. Returns the reference
 * in the surface's units. */
static float
search_scan_surface (dp_correlator_state_t *st, const float *surf,
                     int magnitude)
{
  dp_correlator_search_t *const srch = st->search;
  const int fan_ref = st->noise_mode != DET_NOISE_MEDIAN && st->noise_lo == 0
                      && st->noise_hi == srch->n_surf - 1;
  search_scan_t sc  = { st, surf, magnitude, fan_ref };
  dp_pool_run (search_block_pool (st), srch->window_bins, search_tile_decide,
               &sc);
  if (!fan_ref)
    return det_noise_estimate (surf, st->noise_lo, st->noise_hi,
                               st->noise_scratch, st->noise_mode);
  float ref = srch->parts[0].ref;
  for (size_t r = 1; r < srch->window_bins; r++)
    {
      const float v = srch->parts[r].ref;
      if (st->noise_mode == DET_NOISE_MIN)
        ref = v < ref ? v : ref;
      else if (st->noise_mode == DET_NOISE_MAX)
        ref = v > ref ? v : ref;
      else
        ref += v;
    }
  return st->noise_mode == DET_NOISE_MEAN ? ref / (float)srch->window_bins
                                          : ref;
}

/* The list over a surface, after search_scan_surface() has run the decide
 * pass on it: the working mask holds the band mask, the slots the band's
 * first maxima. The gate is `eta` in the surface's units, the zone one
 * Doppler row (`interp` surface rows) by the mainlobe (`shape.zone`
 * columns -- one chip for a code); every
 * pick after the first is one fanned scan of the working mask. When
 * nothing crosses the gate the strongest cell is still taken for the
 * inspection fields, as the classic detector always reported its maximum
 * -- and it is the decide pass's own pick, at no further scan. */
static void
search_list_peaks (dp_correlator_state_t *st, const float *surf, float gate)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  nx   = srch->lag_bins;
  const size_t                  rows = srch->n_surf / nx;
  search_scan_t                 sc   = { st, surf, 0, 0 };
  size_t                        best = search_merge_best (st, surf);
  st->n_peaks                        = 0;
  while (best < srch->n_surf && surf[best] > gate)
    {
      st->peaks[st->n_peaks++]
          = (det_peak_t){ best / nx, best % nx, surf[best] };
      if (st->n_peaks == st->max_peaks)
        break;
      det_peak_zone (st->peak_mask, rows, nx, best / nx, best % nx,
                     srch->interp, search_zone_cols (st));
      dp_pool_run (search_block_pool (st), srch->window_bins, search_tile_pick,
                   &sc);
      best = search_merge_best (st, surf);
    }
  if (st->n_peaks == 0 && best < srch->n_surf)
    st->peaks[0] = (det_peak_t){ best / nx, best % nx, surf[best] };
  st->peak_row
      = search_native_row (st, surf, st->peaks[0].row, st->peaks[0].col);
  st->peak_col = st->peaks[0].col;
}

/* The strongest pick's concentration: the power of its main lobe -- its row
 * and the `interp` rows either side, the exclusion zone's width (§7.1), so
 * an emitter straddling two tiles is not charged for its own scalloping --
 * over the total power of its code-phase column across EVERY Doppler row,
 * every tile and every slow-time row. One emitter's splatter (a data
 * transition's twins two or more tiles away, a coherent block straddling
 * data spread over its rows) then reads as a low concentration at ONE code
 * phase, where a second emitter is a second column
 * (docs/design/async-dsss-receiver.md §2.4). `power`
 * says whether `surf` already holds power (the non-coherent accumulator) or
 * amplitude. */
static float
search_peak_concentration (const dp_correlator_state_t *st, const float *surf,
                           int power)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  nx   = srch->lag_bins;
  const size_t                  rows = srch->n_surf / nx;
  const size_t                  col  = st->peaks[0].col;
  const size_t                  prow = st->peaks[0].row;
  const size_t                  zone = srch->interp;
  double                        tot = 0.0, lobe = 0.0;
  for (size_t r = 0; r < rows; r++)
    {
      double v = surf[r * nx + col];
      v        = power ? v : v * v;
      tot += v;
      size_t d = r > prow ? r - prow : prow - r;
      if (d > rows - d)
        d = rows - d; /* the Doppler axis is circular (the FFT fold) */
      if (d <= zone)
        lobe += v;
    }
  return tot > 0.0 ? (float)(lobe / tot) : 0.0f;
}
/* A dwell has been decided on `surf`, `stat_of` turning a cell into its test
 * statistic: keep the surface in those units when a reader is armed, hand
 * it to the sink on its decimation, and emit the dwell's probes. Detached
 * and unarmed, this is three predicted-not-taken branches per dwell. */
static void
search_dwell_decided (dp_correlator_state_t *st, const float *surf,
                      float (*stat_of) (const dp_correlator_state_t *, float),
                      int hit)
{
  dp_correlator_search_t *const srch = st->search;
  srch->dwells++;
  /* An attached sink keeps the surface computed whatever keep_surface
     reads: a restore (keep_surface comes from the blob, the sink is the
     caller's and stays attached) or set_keep_surface(0) must not silence
     it. */
  if (srch->keep_surface || srch->sink)
    {
      if (!srch->stat_surface)
        srch->stat_surface
            = (float *)dp_xmallocn (srch->n_surf, sizeof (float));
      for (size_t k = 0; k < srch->n_surf; k++)
        srch->stat_surface[k] = stat_of (st, surf[k]);
      srch->surface_at = srch->samples_consumed;
      if (srch->sink
          && srch->dwells % (srch->sink_decim ? srch->sink_decim : 1) == 0)
        srch->sink (srch->sink_ctx, srch->stat_surface,
                    srch->n_surf / srch->lag_bins, srch->lag_bins,
                    srch->samples_consumed);
    }
  if (srch->tlm.ctx)
    {
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_stat, st->test_stat);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_gate,
                   srch->n_noncoh > 1 ? srch->eta_nc : st->threshold);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_noise, st->noise_est);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_peak, st->peak_mag);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_row, (double)st->peak_row);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_col, (double)st->peak_col);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_n_peaks, (double)st->n_peaks);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_n_held, (double)srch->n_held);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_conc, srch->peak_conc);
      dp_tlm_emit (srch->tlm.ctx, srch->tlm.id_hit, (double)hit);
    }
}
/* Peak, CFAR noise, and test statistic from the coherent dump in out_buf. */
static void
search_compute_stat (dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  /* Two maxima, and the split is the whole point of interpolating.
   *
   * `peak` ranges over the INTERPOLATED surface and sets peak_mag /
   * test_stat: that is where the scalloping loss is recovered, and it is
   * what decides whether the gate fires at all.
   *
   * `nat` is restricted to NATIVE rows and sets the reported cell. The
   * contract is that doppler_bin lives on the native grid -- every consumer
   * scales it by doppler_res_hz -- so reporting a half-bin row, or rounding
   * one to its nearer neighbour, would make the estimate jitter by a whole
   * bin on noise. Measured: rounding cost ~5 points of Pd on the
   * characterization's true-cell criterion, because a peak landing on an odd
   * fine row reported the wrong neighbour. */
  st->noise_est = search_scan_surface (st, st->mag_buf, 1);
  /* The gate in the surface's units: test_stat > threshold is
     mag > threshold * noise_est. A zero reference gates nothing through
     (test_stat reads 0), so the gate is then unreachable. */
  const float gate
      = st->noise_est > 0.0f ? st->threshold * st->noise_est : HUGE_VALF;
  search_list_peaks (st, st->mag_buf, gate);
  st->peak_mag = st->peaks[0].value;
  st->test_stat
      = (st->noise_est > 0.0f) ? (st->peak_mag / st->noise_est) : 0.0f;
  srch->peak_conc = search_peak_concentration (st, st->mag_buf, 0);
  srch->n_held    = 0;
}

/* Non-coherent CFAR: peak + normalized order-N_nc statistic from nc_surface
 * (the per-cell sum of |dump|^2 over n_noncoh coherent looks).
 *
 * Under H0 each look's |z|^2 = sigma^2 * chi2(2)/2, so nc_surface ~
 * (sigma^2/2)*chi2(2*n_noncoh).  Estimating the per-look power sigma^2 from
 * the mean reference-cell power (noise_pow / n_noncoh), the normalized
 * statistic R = sqrt(2 * n_noncoh * peak / noise_pow) has P(R > b) =
 * marcum_q(n_noncoh, 0, b) under H0 — exactly the order-N_nc threshold eta_nc.
 * Validated against Monte-Carlo to <1% (det_pd_noncoherent tests). */
static void
search_compute_stat_nc (dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  float noise_pow = search_scan_surface (st, srch->nc_surface, 0);
  st->noise_est   = sqrtf (noise_pow);
  /* The gate in the surface's (power) units: R > eta_nc is
     cell > eta_nc^2 * noise_pow / (2 n_noncoh). */
  const float gate = noise_pow > 0.0f ? srch->eta_nc * srch->eta_nc * noise_pow
                                            / (2.0f * (float)srch->n_noncoh)
                                      : HUGE_VALF;
  search_list_peaks (st, srch->nc_surface, gate);
  st->peak_mag  = sqrtf (st->peaks[0].value);
  st->test_stat = (noise_pow > 0.0f) ? sqrtf (2.0f * (float)srch->n_noncoh
                                              * st->peaks[0].value / noise_pow)
                                     : 0.0f;
  srch->peak_conc = search_peak_concentration (st, srch->nc_surface, 1);
  srch->n_held    = 0;
}

double _Complex dp_correlator_cell_corr (const dp_correlator_state_t *st,
                                         const float _Complex *x, size_t col,
                                         double f_hz, double t0)
{
  const size_t nx            = st->search->code_bins;
  const double _Complex step = cexp (-2.0 * M_PI * I * f_hz / st->search->fs);
  double _Complex ph         = 1.0;
  double _Complex acc        = 0.0;
  for (size_t m = 0; m < nx; m++)
    {
      /* The mixer: a recurrence from an exact start, resynced every 256
         samples (the ramp's pattern). */
      if ((m & 255) == 0)
        ph = cexp (-2.0 * M_PI * I * f_hz * (t0 + (double)m) / st->search->fs);
      else
        ph *= step;
      const size_t k = (m + nx - col % nx) % nx;
      acc += (double _Complex) (x[m] * conjf (st->search->ref[k])) * ph;
    }
  return acc;
}

/* dp_correlator_cell_corr_grid(): blocks per epoch for each cycle per epoch
   the frequencies spread either side of their centre, and the widest spread it
   expands before falling back to one cell at a time. */
#define DP_CORRELATOR_GRID_BLOCKS_PER_CYCLE 48.0
#define DP_CORRELATOR_GRID_MAX_CYCLES 1.0

void
dp_correlator_cell_corr_grid (const dp_correlator_state_t *st,
                              const float _Complex *x, size_t n_epochs,
                              size_t col, const double *f_hz, size_t n_f,
                              double t0, double _Complex *out)
{
  dp_correlator_search_t *const srch = st->search;
  if (n_f == 0 || n_epochs == 0)
    return;
  const size_t nx = srch->code_bins;
  const double w0 = 2.0 * M_PI / srch->fs;
  double       lo = f_hz[0], hi = f_hz[0];
  for (size_t j = 1; j < n_f; j++)
    {
      lo = f_hz[j] < lo ? f_hz[j] : lo;
      hi = f_hz[j] > hi ? f_hz[j] : hi;
    }
  /* Every frequency is `fb + residual`, the residual at most `r` cycles
     per epoch; past DP_CORRELATOR_GRID_MAX_CYCLES the blocks would get too
     short to pay, and the exact primitive is the answer. */
  const double fb = 0.5 * (lo + hi);
  const double r  = 0.5 * (hi - lo) * (double)nx / srch->fs;
  if (r > DP_CORRELATOR_GRID_MAX_CYCLES)
    {
      for (size_t e = 0; e < n_epochs; e++)
        for (size_t j = 0; j < n_f; j++)
          out[e * n_f + j] = dp_correlator_cell_corr (
              st, x + e * nx, col, f_hz[j], t0 + (double)(e * nx));
      return;
    }

  /* Blocks of (nearly) nx/B samples, each expanded about its nominal
     centre c_b = c0 + b*dc: the uniform spacing is what lets every
     per-block phase below be a recurrence rather than a cexp. B grows
     with the spread, so the residual turns at most pi/48 either side of
     a block's centre whatever the spread. */
  size_t B        = (size_t)ceil (DP_CORRELATOR_GRID_BLOCKS_PER_CYCLE * r);
  B               = B < 1u ? 1u : (B > nx ? nx : B);
  const double dc = (double)nx / (double)B;
  const double c0 = 0.5 * dc - 0.5;

  /* The replica with the base mixer folded in, once per call: what each
     epoch correlates against, so the per-sample loop is one multiply and
     three sums with nothing carried from sample to sample -- the shape
     the compiler vectorizes. The mixer is referenced to the epoch's own
     first sample; each epoch's start phase is applied once, below. */
  double _Complex *rm = dp_xmallocn (nx, sizeof *rm);
  {
    const double _Complex step = cexp (-I * w0 * fb);
    double _Complex ph         = 1.0;
    size_t k                   = (nx - col % nx) % nx; /* ref index, m = 0 */
    for (size_t m = 0; m < nx; m++)
      {
        rm[m] = conj ((double _Complex)srch->ref[k]) * ph;
        ph *= step;
        k = k + 1 == nx ? 0 : k + 1;
      }
  }

  double          *w    = dp_xmallocn (n_f, sizeof *w);
  double _Complex *rot  = dp_xmallocn (dp_size_mul_sat (5u, n_f), sizeof *rot);
  double _Complex *adv  = rot + n_f;
  double _Complex *base = rot + 2u * n_f;
  double _Complex *p    = rot + 3u * n_f; /* per cell, this epoch */
  double _Complex *acc  = rot + 4u * n_f;
  for (size_t j = 0; j < n_f; j++)
    {
      w[j]    = w0 * (f_hz[j] - fb);           /* residual, rad/sample */
      rot[j]  = cexp (-I * w[j] * dc);         /* block to block */
      adv[j]  = cexp (-I * w[j] * (double)nx); /* epoch to epoch */
      base[j] = cexp (-I * w[j] * (t0 + c0));  /* epoch 0, block 0 */
    }

  double _Complex (*S)[3] = dp_xmallocn (B, sizeof *S);
  for (size_t e = 0; e < n_epochs; e++)
    {
      const float _Complex *xe = x + e * nx;
      for (size_t b = 0; b < B; b++)
        {
          const size_t m0 = b * nx / B, m1 = (b + 1u) * nx / B;
          const double cb    = c0 + (double)b * dc;
          double _Complex s0 = 0.0, s1 = 0.0, s2 = 0.0;
          for (size_t m = m0; m < m1; m++)
            {
              const double _Complex z = (double _Complex)xe[m] * rm[m];
              const double d          = (double)m - cb;
              s0 += z;
              s1 += z * d;
              s2 += z * (d * d);
            }
          S[b][0] = s0;
          S[b][1] = s1;
          S[b][2] = s2;
        }
      /* exp(-j w d) = 1 - j w d - (w d)^2 / 2 + O((w d)^3), |w d| <= pi/48
         at the block's edge; then the base mixer's phase at this epoch's
         first sample. */
      const double _Complex em = cexp (-I * w0 * fb * (t0 + (double)(e * nx)));
      /* Blocks outer, cells inner: each cell's accumulation is independent
         of every other's, so the inner loop vectorizes across cells. */
      for (size_t j = 0; j < n_f; j++)
        {
          p[j]   = base[j];
          acc[j] = 0.0;
        }
      for (size_t q = 0; q < B; q++)
        {
          const double _Complex s0 = S[q][0], s1 = S[q][1], s2 = S[q][2];
          for (size_t j = 0; j < n_f; j++)
            {
              const double wj = w[j];
              acc[j] += p[j] * (s0 - I * wj * s1 - 0.5 * wj * wj * s2);
              p[j] *= rot[j];
            }
        }
      for (size_t j = 0; j < n_f; j++)
        {
          out[e * n_f + j] = em * acc[j];
          base[j] *= adv[j];
        }
    }
  free (S);
  free (rot);
  free (w);
  free (rm);
}

static size_t
search_rate_count (const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  return search_rate_count_at (srch->rate_hyps, srch->coherent_bins);
}

/* Rate hypothesis `h` of `n` over [-span, +span], Hz/s; one hypothesis is
 * the rate 0. Exported as dp_correlator_rate_hypothesis(): refine searches the
 * engine's grid, not its own. */
static double
search_rate_of (double span, size_t n, size_t h)
{
  return n > 1 ? -span + 2.0 * span * (double)h / (double)(n - 1) : 0.0;
}

/* A pick's statistic on its own lag (dp_correlator_hit_t::test_stat_peak): the
 * centre column `c` and the larger of its two neighbours, on the pick's
 * surface row, under the triangular mainlobe `shape.zone` samples wide. A
 * column is lag_bins/code_bins of a sample, so the neighbour sits s =
 * code_bins/lag_bins samples off; a true lag a samples from the centre's
 * (0 <= a <= s/2, toward that neighbour) reads c = P (1 - a/zone) and
 * n = P (1 - (s - a)/zone), so P = (c + n) / (2 - s/zone). The clamp keeps
 * the estimate inside what a straddle can do. Statistics are amplitudes on
 * both paths (stat_of is linear in |z| coherently, in sqrt of the power
 * sum non-coherently), so the triangle applies to them as read. */
static float
search_peak_on_lag (const dp_correlator_state_t *st, const float *surf,
                    size_t row, size_t c, float centre,
                    float (*stat_of) (const dp_correlator_state_t *, float))
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  lb   = srch->lag_bins;
  const double                  s    = (double)srch->code_bins / (double)lb;
  const double                  z    = (double)srch->shape.zone;
  if (!(z > s) || !(centre > 0.0f))
    return centre;
  const float  l  = stat_of (st, surf[row * lb + (c + lb - 1) % lb]);
  const float  h  = stat_of (st, surf[row * lb + (c + 1) % lb]);
  const double n  = (double)(l > h ? l : h);
  double       p  = ((double)centre + n) / (2.0 - s / z);
  const double hi = (double)centre / (1.0 - s / (2.0 * z));
  p               = p < (double)centre ? (double)centre : p > hi ? hi : p;
  return (float)p;
}

/* The listed peaks of the dwell just decided, as results: each pick's
 * native row and column, its own statistic against the dwell's reference,
 * and the two-epoch rule of docs/design/async-dsss-receiver.md §7.1 -- a
 * pick within a chip of an already-listed pick's code phase, at any row, is
 * that emitter's candidate twin (a data transition inside the epoch splits
 * one emitter into equal peaks at its own code phase on other tiles; a
 * half-tile straddle throws a sidelobe there), held for one dwell and
 * listed only if it was there, at the same row, on the previous dwell --
 * listed or held: two equal emitters at one code phase swap places as the
 * strongest from dwell to dwell, and the one listed last time must not be
 * held this time. A twin moves with the transition and is absent in the
 * emitter's data-free window; a real second emitter at the same code
 * phase stays at its tile. `twin_*` therefore carry every pick of the
 * previous dwell. `stat_of`
 * turns a surface value into the statistic for this path. Returns the
 * results appended, at most `room`: a dwell's list is truncated to the
 * room left, strongest first. */
static size_t
search_report_peaks (dp_correlator_state_t *st, const float *surf,
                     float (*stat_of) (const dp_correlator_state_t *, float),
                     float (*mag_of) (float), dp_correlator_hit_t *result,
                     size_t room)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  nx   = srch->lag_bins;
  const size_t                  zone = search_zone_cols (st);
  size_t                        ndet = 0, n_listed = 0, n_seen = 0;
  size_t                        listed_col[DP_CORRELATOR_MAX_PEAKS];
  uint32_t                      seen_row[DP_CORRELATOR_MAX_PEAKS],
      seen_col[DP_CORRELATOR_MAX_PEAKS];
  for (size_t i = 0; i < st->n_peaks && ndet < room; i++)
    {
      const size_t c = st->peaks[i].col;
      const size_t r = search_resolve_tile_alias (
          st, search_native_row (st, surf, st->peaks[i].row, st->peaks[i].col),
          c);
      seen_row[n_seen] = (uint32_t)r;
      seen_col[n_seen] = (uint32_t)c;
      n_seen++;
      int twin = 0;
      for (size_t j = 0; j < n_listed && !twin; j++)
        {
          size_t d = c > listed_col[j] ? c - listed_col[j] : listed_col[j] - c;
          if (d > nx / 2)
            d = nx - d;
          twin = d <= zone;
        }
      if (twin)
        {
          int still = 0;
          for (size_t k = 0; k < st->n_twins && !still; k++)
            {
              size_t d = c > st->twin_col[k] ? c - st->twin_col[k]
                                             : st->twin_col[k] - c;
              if (d > nx / 2)
                d = nx - d;
              still = st->twin_row[k] == (uint32_t)r && d <= zone;
            }
          if (!still)
            continue; /* held: seen this dwell, listed next if it stays */
        }
      listed_col[n_listed++] = c;
      const float stat       = stat_of (st, st->peaks[i].value);
      /* The coherent length behind one look: the frame on a burst engine
         (D epochs already), the block of D epochs on a tiled one. */
      const float coh_n
          = (float)(srch->window_bins > 1 ? srch->coherent_bins * srch->frame_n
                                          : srch->frame_n);
      const float amp_snr
          = stat
            / (srch->n_noncoh > 1
                   ? sqrtf (2.0f * coh_n * (float)srch->n_noncoh)
                   : DP_CORRELATOR_SQRT_2_OVER_PI * sqrtf (2.0f * coh_n));
      result[ndet++] = (dp_correlator_hit_t){
        .row              = r,
        .col              = c,
        .peak_mag         = mag_of (st->peaks[i].value),
        .noise_est        = st->noise_est,
        .test_stat        = stat,
        .cn0_dbhz_est     = search_cn0_dbhz_from_amp_snr (amp_snr, srch->fs),
        .samples_consumed = srch->samples_consumed,
        .chip_phase       = search_handoff_chip_phase (st, r, c, srch->sf),
        .doppler_rate_hz_s
        = search_rate_count (st) > 1
              ? search_rate_of (srch->doppler_rate, srch->rate_hyps,
                                srch->rate_best_h)
              : 0.0,
        .test_stat_peak
        = search_peak_on_lag (st, surf, st->peaks[i].row, c, stat, stat_of),
      };
    }
  /* This dwell's picks, listed and held, are what the next dwell's
     same-phase candidates are checked against. */
  srch->n_held = n_seen - n_listed;
  st->n_twins  = n_seen;
  for (size_t k = 0; k < n_seen; k++)
    {
      st->twin_row[k] = seen_row[k];
      st->twin_col[k] = seen_col[k];
    }
  return ndet;
}

static float
search_stat_coherent (const dp_correlator_state_t *st, float value)
{
  return st->noise_est > 0.0f ? value / st->noise_est : 0.0f;
}

static float
search_stat_noncoherent (const dp_correlator_state_t *st, float value)
{
  dp_correlator_search_t *const srch      = st->search;
  const float                   noise_pow = st->noise_est * st->noise_est;
  return noise_pow > 0.0f
             ? sqrtf (2.0f * (float)srch->n_noncoh * value / noise_pow)
             : 0.0f;
}

static float
search_mag_identity (float value)
{
  return value;
}

static float
search_mag_sqrt (float value)
{
  return sqrtf (value);
}

/* Scanned Doppler bins for a coherent depth D under a doppler_uncertainty
 * prior du (Hz, one-sided half-range).  The slow-time FFT always spans the
 * full +/- span = chip_rate/(2*sf); a tighter du restricts the search to the
 * central band, shrinking the Sidak cell count.  Returns an odd count
 * (2*half+1) centred on DC, capped at D; du <= 0 (or >= span) means full. */
/* A count of bins that must cover `x` bins, as ceil(x) -- less a guard of
 * 1e-9 bins. A prior that is an exact whole number of bins lands on an
 * integer, and rounding can put it one ULP ABOVE it (du = 3*span at
 * span = 1e6/62 gives 1.0000000000000002; under -ffast-math the division
 * may also become a multiply by a reciprocal, so WHICH side it lands on
 * differs by compiler -- GCC 13 on aarch64 against GCC 15), whose ceil
 * provisions a spurious extra bin. The guard is far below any real sizing
 * decision: a prior that genuinely needs another bin exceeds the boundary
 * by vastly more. Every bin count sized from a prior goes through here. */
static double
search_ceil_bins (double x)
{
  return ceil (x - 1e-9);
}

static size_t
search_searched_bins (size_t D, double du, double span)
{
  if (du <= 0.0 || du >= span)
    return D;
  double frac = du / span; /* fraction of half-range */
  size_t half = (size_t)search_ceil_bins (0.5 * frac * (double)D);
  size_t sb   = 2 * half + 1;
  return (sb > D) ? D : sb;
}

/* Mean amplitude derating of the correlation peak from grid straddle — the
 * gap between the on-grid best case dp_det_pd() sees and the operating average
 * the Monte-Carlo characterization measures.  Three independent losses, each
 * averaged over a uniform prior across its straddle range:
 *
 *  - slow-time scalloping: a Doppler off the FFT bin centre by delta in
 *    [-1/2, 1/2] bins loses the Dirichlet factor, ~ sinc(delta) for the
 *    depths used here (absent when D == 1: no slow-time FFT, no bins);
 *  - intra-segment rotation: the slow-time FFT compensates rotation
 *    BETWEEN segments only; the residual rotation across one segment,
 *    u = f*code_bins cycles, loses sinc(u).  f spans the searched band, so
 *    u spans +/- sb/(2*D) — this is the band-edge loss that dominates a
 *    full-span search (sinc(1/2) = -3.9 dB at the edge);
 *  - code-phase straddle: the true code phase lands up to half a sample
 *    off the grid, losing whatever the preamble's autocorrelation gives up
 *    over that offset -- the shape's delay_loss_mean (for a PN code, the
 *    triangle's 1 - 1/(4*spc); search_shape_of_chips).
 */
/* Intra-segment rotation range: the residual u = f*code_bins spans the
 * TIGHTER of the searched band (sb bins of width 2*span/D) and the
 * doppler_uncertainty prior itself — search_searched_bins rounds the bin count
 * up (and clamps at D >= sb >= 1), so sb/D alone over-states the range
 * whenever the prior is tighter than the grid, structurally so at D == 1
 * where sb is pinned to 1 (up to ~1.2 dB of phantom loss otherwise). */
static double
search_intra_umax (size_t D, size_t sb, double du, double span)
{
  double frac = (du > 0.0 && du < span) ? du / span : 1.0;
  double grid = (double)sb / (double)D;
  return 0.5 * (frac < grid ? frac : grid);
}

/* The Doppler-axis interpolation a grid gets: none for a single coherent
 * bin (there is no slow-time transform to pad), DP_CORRELATOR_DOPPLER_INTERP
 * for a coherent slow-time axis, including one inside each wideband tile. One
 * rule, because dp_correlator_regrid() applies it to the surface and the Pd
 * model derates scalloping over the bin it produces -- a model still averaging
 * over the NATIVE bin read 0.47 where the engine measured 0.72
 * (doppler#1183). */
static size_t
search_interp_for (size_t D, size_t window_bins)
{
  /* Any slow-time axis is interpolated: the burst engine's, and the block
     depth inside every tile of a continuous one. Tiles alone (D == 1) are
     hypotheses, not FFT bins, with no scalloping to fix. */
  (void)window_bins;
  return D <= 1 ? 1u : DP_CORRELATOR_DOPPLER_INTERP;
}

/* The deepest coherent block a Doppler rate allows, capped at `d_max`: the
 * drift over one block stays inside half a slow-time row (rate * D *
 * T_epoch <= f_epoch / (2 D), so D <= f_epoch / sqrt(2 rate)), and at least
 * 1. A rate of 0 is no bound. The ONE home of the drift rule: the
 * continuous engine's block depth and the burst sizer's ceiling both read
 * it (doppler#1482). */
static size_t
search_drift_depth (double f_epoch, double doppler_rate, size_t d_max)
{
  size_t d = d_max;
  if (doppler_rate > 0.0)
    {
      double dr = floor (f_epoch / sqrt (2.0 * doppler_rate));
      if (dr < (double)d)
        d = dr < 1.0 ? 1 : (size_t)dr;
    }
  return d < 1 ? 1 : d;
}

/* The largest rate a block of `depth` epochs tolerates: search_drift_depth()
 * inverted, f_epoch^2 / (2 depth^2). dp_correlator_rate_hypotheses_for() reads
 * it. */
static double
search_drift_rate_limit (double f_epoch, size_t depth)
{
  return f_epoch * f_epoch / (2.0 * (double)depth * (double)depth);
}

/* The Doppler rate the drift bound sees: the engine's `doppler_rate` when it
 * searches no rate hypotheses, otherwise the farthest a true rate in
 * [-doppler_rate, +doppler_rate] can sit from the nearest of its
 * `rate_hyps` evenly spaced hypotheses, half a spacing =
 * doppler_rate / (rate_hyps - 1). The ONE place the compensation reaches
 * the sizer: search_drift_depth() and the cell count both read it. */
static double
search_residual_rate (const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  return srch->rate_hyps > 1
             ? srch->doppler_rate / (double)(srch->rate_hyps - 1)
             : srch->doppler_rate;
}

/* The block-coherent depth a continuous engine runs inside every tile
 * (docs/design/async-dsss-receiver.md §2.1): the larger of one and the
 * smaller of two bounds -- a whole block always fits in the window
 * (non-overlapping blocks at an unknown window phase need W >= 2D - 1), and
 * the drift bound, search_drift_depth() -- rounded down to a 7-smooth length.
 * D is a slow-time FFT length (D * DP_CORRELATOR_DOPPLER_INTERP), and both
 * bounds are ceilings, not targets: a prime factor of 13 or more costs
 * pocketfft's slow path (94 = 2 x 47 cost 1.46x 90, doppler#2457), the
 * round-down at most about 9% of the depth (dp_fft_smooth_floor()). */
static size_t
search_block_depth (double f_epoch, size_t code_only_epochs,
                    double doppler_rate)
{
  return dp_fft_smooth_floor (
      search_drift_depth (f_epoch, doppler_rate, (code_only_epochs + 1) / 2));
}

/* The surface row of slow-time row `i` (of `dI = D * interp`, in FFT order)
 * inside tile `r` (of `tiles`, in FFT order): the tiles and the rows inside
 * them make ONE uniform grid of tiles * dI fine bins over the tiled span,
 * kept in native FFT-bin order so every fold downstream -- the zone, the
 * native-row report, dp_correlator_build_handoff(), the surface axis -- is the
 * same dp_fftfreq_index() over the combined count. */
static inline size_t
search_block_row (size_t r, size_t tiles, size_t i, size_t dI)
{
  const long total = (long)(tiles * dI);
  long m = dp_fftfreq_index (r, tiles) * (long)dI + dp_fftfreq_index (i, dI);
  return (size_t)(((m % total) + total) % total);
}

static double
search_straddle_loss (size_t D, size_t sb, const dp_correlator_shape_t *sh,
                      double du, double span, size_t interp)
{
  /* Half of the bin the peak search actually samples: the slow-time axis is
     interpolated, so the worst straddle is half an INTERPOLATED bin. */
  double l_scallop = (D > 1) ? dp_mean_sinc (0.5 / (double)interp) : 1.0;
  double l_intra   = dp_mean_sinc (search_intra_umax (D, sb, du, span));
  double l_code    = sh->delay_loss_mean;
  return l_scallop * l_intra * l_code;
}

/* The Pd of one coherent cell at amplitude SNR `se` against the gate the
 * engine actually runs: `eta` scaled by a noise reference MEASURED from the
 * surface -- the mean magnitude of all `k` cells, the peak's own included
 * (doppler#1501). The CFAR model itself is dp_det_pd_cfar(); what is the
 * engine's is WHERE the signal sits in that reference.
 *
 * Everything off the peak: the preamble's own sidelobes plus what the
 * straddle slid out of the peak (`se_tot` is the node's unstraddled
 * amplitude). A band-limited delay conserves the correlation's energy, so
 * the two are ONE budget, a2t (1 + E2) - a2p, and it lies where the
 * autocorrelation puts it: m = E1^2/E2 cells at one level
 * (dp_correlator_shape_t's off_peak_amp and off_peak), which matches the true
 * profile's added mean in both limits (E2/(2 mu0) per unit power when weak, E1
 * per unit amplitude when strong). Charging the straddle again on top counted
 * a chip code's triangle twice and capped its Pd below 1 at any SNR. A perfect
 * sequence has no profile (E2 = 0); its leak is spread over every cell, which,
 * the root being concave, is the most mean it can add.
 *
 * Measured on a 7-chip code held 2 samples a chip (14 cells at D = 1, 65
 * dB-Hz): the reference 72% high, the engine 0.229 against 0.955 with the
 * noise known. At D = 1 (127 cells; Zadoff-Chu 127, continuous delay, 6000
 * trials): the engine 0.558 and 0.146 against a known-noise model of 0.611
 * and 0.179 -- optimistic -- and 0.525 and 0.147 from this one. Excluding
 * the peak from the reference recovered 0.045 of it, so the signal in its
 * own reference is most of the loss. */
static double
search_cfar_pd (double se, double se_tot, int n, double eta, double k,
                const dp_correlator_shape_t *sh)
{
  const double a2p = 2.0 * (double)n * se * se;
  const double a2t = 2.0 * (double)n * se_tot * se_tot;
  const double off = a2t * (1.0 + sh->off_peak) - a2p;
  const double m   = (sh->off_peak > 0.0 && sh->off_peak_amp > 0.0)
                         ? sh->off_peak_amp * sh->off_peak_amp / sh->off_peak
                         : 0.0;
  return dp_det_pd_cfar (se, n, eta, k, off, m);
}

/* Midpoint nodes the burst Pd averages the preamble's alignment over. 8 is
   within 0.0013 of a 128-node reference across Zadoff-Chu 127 x 8 at every
   D (32 is within 1e-4 and costs 4x at every create, doppler#1498). */
#define DP_CORRELATOR_ALIGN_NODES 8

/* The Pd of one BURST of `reps` repetitions against non-overlapping dwells
 * of D, at one straddle node's amplitude `amp` (doppler#1498). The dwells
 * are aligned to the stream, not the burst, so the preamble starts a uniform
 * s0 in [0, D) periods into a dwell. The dwells it overlaps each hold a
 * fraction f of preamble and see amplitude f * amp: signal in f*D of the D
 * epochs sums coherently to f of a whole dwell's peak, over the same noise.
 * The burst is captured when ANY of them detects, and their noise is
 * independent, so the burst misses with probability prod(1 - Pd_i).
 *
 * At most two dwells are partial (the first and the last); the rest are
 * whole and share one det_pd. So a node costs 1 + 2*DP_CORRELATOR_ALIGN_NODES
 * evaluations whatever reps is. At amp = 0 it is the burst's false-alarm
 * floor: noise alone crossing the gate in some dwell it spans.
 *
 * The straddle is NOT drawn per dwell: one burst has one Doppler and one
 * code delay, so every dwell it spans shares the node's `amp`. Combining
 * dwells after averaging each over the straddle credits them as
 * independent, and over-predicted Zadoff-Chu 127 x 8 by up to 0.05 at
 * D = 1 against the engine. */
static double
search_burst_pd_at (double amp, double tot, size_t D, size_t reps, int n,
                    double eta, double k, const dp_correlator_shape_t *sh)
{
  const double d = (double)D, r = (double)reps;
  const double p1  = search_cfar_pd (amp, tot, n, eta, k, sh);
  double       acc = 0.0;
  double       u[DP_CORRELATOR_ALIGN_NODES];
  dp_midpoint_nodes (u, DP_CORRELATOR_ALIGN_NODES);
  for (int o = 0; o < DP_CORRELATOR_ALIGN_NODES; o++)
    {
      const double s0    = u[o] * d;
      const double first = fmin (d - s0, r); /* preamble in the first dwell */
      const double rest  = r - first;        /* after it, in periods */
      const double whole = floor (rest / d);
      const double tail  = rest - whole * d; /* the last, partial dwell */
      double       miss
          = 1.0
            - (first >= d ? p1
                          : search_cfar_pd (amp * first / d, tot * first / d,
                                            n, eta, k, sh));
      miss *= pow (1.0 - p1, whole);
      if (tail > 0.0)
        miss *= 1.0
                - search_cfar_pd (amp * tail / d, tot * tail / d, n, eta, k,
                                  sh);
      acc += 1.0 - miss;
    }
  return acc / DP_CORRELATOR_ALIGN_NODES;
}

/* Average Pd over the straddle priors — Pd(snr) is concave over the loss
 * range, so Pd at the MEAN amplitude overstates the mean Pd (Jensen; ~0.11
 * at a marginal design point). Midpoint quadrature over the three
 * independent uniform factors (8 x 8 x 4 = 256 det_pd evaluations, setup
 * path only; each factor is symmetric, so half-ranges suffice). nc == 1
 * evaluates the coherent path, nc > 1 the non-coherent one.
 *
 * `reps` == 0 is ONE dwell lying wholly inside the signal: `pd_predicted`,
 * and every continuous engine. `reps` > 0 is a BURST of that many
 * repetitions at a uniform alignment (search_burst_pd_at, coherent only):
 * `pd_burst`. Same nodes, so the two numbers differ by the alignment alone.
 *
 * `k_ref` is the cell count of the noise reference the coherent gate
 * divides by (search_cfar_pd); 0 prices the noise as known, as the
 * non-coherent path and the uncalibrated CFAR modes still do. */
static double
search_mean_pd (double snr, size_t D, double umax,
                const dp_correlator_shape_t *sh, int n, double eta, int nc,
                size_t interp, size_t reps, double k_ref)
{
  enum
  {
    nd = 8,
    nu = 8,
    nk = DP_CORRELATOR_DELAY_LOSS_NODES
  };
  double ud[nd], uu[nu];
  dp_midpoint_nodes (ud, nd);
  dp_midpoint_nodes (uu, nu);
  const double half_bin = 0.5 / (double)interp; /* the SAMPLED bin */
  double       acc      = 0.0;
  for (int i = 0; i < nd; i++)
    {
      double ls = (D > 1) ? dp_sinc (half_bin * ud[i]) : 1.0;
      for (int j = 0; j < nu; j++)
        {
          double li = dp_sinc (umax * uu[j]);
          for (int k = 0; k < nk; k++)
            {
              double se = snr * ls * li * sh->delay_loss[k];
              acc += reps     ? search_burst_pd_at (se, snr, D, reps, n, eta,
                                                    k_ref, sh)
                     : nc > 1 ? dp_det_pd_noncoherent (se, n, nc, eta)
                              : search_cfar_pd (se, snr, n, eta, k_ref, sh);
            }
        }
    }
  return acc / (double)(nd * nu * nk);
}

/* C/N0 (dB-Hz) -> per-sample amplitude SNR: power SNR = (C/N0)/fs, and the
 * detection model's non-centrality is a = sqrt(2M)*snr (amplitude).
 *
 * DP_CORRELATOR_CN0_NONE (NaN) means "no design point" (burst mode only; see
 * dp_correlator_create_burst). Every finite C/N0 is a design point, negative
 * included: at fs = 1 it IS the per-sample SNR, negative wherever acquisition
 * is hard (doppler#1484). "None" is returned as 0.0 -- no finite C/N0 maps
 * there -- so every consumer can still test `snr > 0.0` for "was a design C/N0
 * given" and nothing sizes against a signal nobody specified. */
static double
search_design_snr (double cn0_dbhz, double fs)
{
  return isnan (cn0_dbhz) ? 0.0 : dp_det_cn0_to_snr (cn0_dbhz, fs);
}

/* Derive and commit the threshold ladder (searched_bins / pfa_cell / eta /
 * eta_nc / threshold / straddle_loss / pd_predicted / underpowered) for the
 * grid already set on st (st->search->coherent_bins / st->search->n_noncoh),
 * given the sizing physics.  Shared by both auto-sizers and
 * dp_correlator_configure_search_raw, so a caller-pinned grid gets exactly the
 * same threshold derivation an auto-sized one would.
 *
 * The thresholds depend on pfa and the grid alone. Only pd_predicted and
 * underpowered need a design C/N0; without one (snr == 0) the prediction is
 * NAN and the engine is never underpowered -- there is no target to be under
 * (doppler#1181). */
static void
search_commit_thresholds (dp_correlator_state_t *st, double pfa, double pd,
                          double snr, double du)
{
  dp_correlator_search_t *const srch = st->search;
  const double                  span = srch->doppler_span_hz;
  const size_t                  D    = srch->coherent_bins;
  const size_t                  nc   = srch->n_noncoh;

  /* Wideband mode already tiles the FULL requested uncertainty with
   * window_bins windows (no further within-mode narrowing prior exists), so
   * every window is "searched" by construction -- unlike the native
   * du-narrows-the-native-span case search_searched_bins models. */
  srch->searched_bins  = (srch->window_bins > 1)
                             ? srch->window_bins * D
                             : search_searched_bins (D, du, span);
  srch->doppler_res_hz = srch->chip_rate / ((double)srch->sf * (double)D);
  /* Every rate hypothesis is another independent look at each cell. */
  srch->pfa_cell = dp_det_pfa_cell (
      pfa, (double)(search_band_cells (D, srch->searched_bins,
                                       srch->window_bins, srch->lag_bins)
                    * search_rate_count (st)));
  double umax         = search_intra_umax (D, srch->searched_bins, du, span);
  srch->straddle_loss = search_straddle_loss (
      D, srch->searched_bins, &srch->shape, du, span, srch->interp);
  srch->eta = (float)dp_det_threshold (srch->pfa_cell);

  double gate;
  if (nc > 1)
    {
      gate          = dp_det_threshold_noncoherent (srch->pfa_cell, (int)nc);
      srch->eta_nc  = (float)gate;
      st->threshold = 0.0f; /* coherent gate unused on the non-coherent path */
    }
  else
    {
      gate          = srch->eta;
      srch->eta_nc  = 0.0f;
      st->threshold = srch->eta * DP_CORRELATOR_SQRT_2_OVER_PI;
    }
  srch->pd_predicted = NAN;
  srch->pd_burst     = NAN;
  srch->underpowered = 0;
  if (snr > 0.0)
    {
      /* The coherent gate divides by a reference measured over the whole
         native surface: price that, in the mode the gate is derived for. */
      const double k_ref
          = st->noise_mode == DET_NOISE_MEAN
                ? (double)(srch->n / srch->code_bins * srch->lag_bins)
                : 0.0;
      srch->pd_predicted
          = search_mean_pd (snr, D, umax, &srch->shape, (int)srch->n, gate,
                            (int)nc, srch->interp, 0, k_ref);
      /* A burst is judged on the burst: the Pd a caller of a burst engine
         gets is any dwell the preamble spans detecting. Non-coherent looks
         on a burst have no alignment model (see search_auto_config_burst for
         why the sizer never picks them), so they keep the one-dwell Pd. */
      if (srch->burst && nc == 1)
        srch->pd_burst
            = search_mean_pd (snr, D, umax, &srch->shape, (int)srch->n, gate,
                              1, srch->interp, srch->reps, k_ref);
      srch->underpowered
          = (uint8_t)((isnan (srch->pd_burst) ? srch->pd_predicted
                                              : srch->pd_burst)
                      < pd);
    }
  /* The band mask the peak list starts from: the cells outside the searched
     Doppler band are never candidates. searched_bins is decided just above,
     so this is the one place the mask is built. */
  for (size_t k = 0; k < srch->n_surf; k++)
    st->band_mask[k] = (uint8_t)!search_in_doppler_band (st, k);
}

/* Ascend n_noncoh from 1 until the AVERAGED Pd meets pd, capped at
 * DP_CORRELATOR_N_NONCOH_SAFETY_CEILING (the internal safety valve replacing
 * the old caller-supplied max_noncoh cap -- see its doc comment in
 * search_core.h). Shared by the continuous engine's window-tiling path (D
 * pinned to 1, sb = window count) and the burst path's non-coherent fallback
 * (D = the coherent depth that fell short, sb = its searched-bin count).
 * det_n_noncoh sizes at the mean straddle amplitude -- a fast initializer that
 * Jensen makes slightly optimistic -- then the count escalates until the
 * AVERAGED Pd meets the target. */
static size_t
search_ascend_n_noncoh (double snr, size_t D, size_t sb, size_t window_bins,
                        size_t cb, size_t lb, double pfa, double pd,
                        const dp_correlator_shape_t *sh, double du,
                        double span, double k_ref)
{
  const size_t interp = search_interp_for (D, 1);
  const double umax   = search_intra_umax (D, sb, du, span);
  /* `cb` samples set the coherent gain; the `lb` columns a row are the
     cells the maximum ranges over (lag_bins). */
  const double pc = dp_det_pfa_cell (
      pfa, (double)search_band_cells (D, sb, window_bins, lb));
  const double sloss = search_straddle_loss (D, sb, sh, du, span, interp);

  int    k  = dp_det_n_noncoh (snr * sloss, (int)(D * cb), pd, pc,
                               (int)DP_CORRELATOR_N_NONCOH_SAFETY_CEILING);
  size_t nc = (k > 0) ? (size_t)k : DP_CORRELATOR_N_NONCOH_SAFETY_CEILING;
  while (nc < DP_CORRELATOR_N_NONCOH_SAFETY_CEILING)
    {
      double e = (nc > 1) ? dp_det_threshold_noncoherent (pc, (int)nc)
                          : dp_det_threshold (pc);
      if (search_mean_pd (snr, D, umax, sh, (int)(D * cb), e, (int)nc, interp,
                          0, k_ref)
          >= pd)
        break;
      nc++;
    }
  return nc;
}

/* How many frequency-window hypotheses cover +/- du at this engine's own bin
 * spacing.  ALWAYS ODD, and sized against the spacing the bins are actually
 * reported at -- both of which are load-bearing:
 *
 * - The hypothesis spacing is doppler_res_hz = chip_rate/(sf*D) = 2*span at
 *   the D=1 every wideband path uses, NOT `span` (= chip_rate/(2*sf), the
 *   half-span).  Sizing with `ceil(du/span)` counted half-bins and produced
 *   roughly twice the windows the coverage needed.
 * - The centre bin covers +/- res/2 and each further bin per side adds res,
 *   so covering du needs n_side = ceil((du - res/2)/res) per side and
 *   2*n_side+1 total.  An ODD count is what makes coverage symmetric: an even
 *   count folds to [-n/2, +n/2-1] (one more negative hypothesis than
 *   positive), leaving the top positive offset unreachable, and puts a bin at
 *   exactly n/2 whose signed reading is the one index the search and the
 *   handoff used to disagree on (see dp_fftfreq_index()).
 *
 * Worked example, the specification's geometry
 * (docs/design/async-dsss-receiver.md §1: chip_rate 3.069 Mcps, sf 1023 ->
 * span 1500 Hz, res 3000 Hz) at du = 50 kHz: n_side = ceil(48500/3000) = 17 ->
 * 35 windows, k in [-17, +17], reach +/-51000 Hz.  The old formula gave
 * ceil(50000/1500) = 34, k in [-16, +17], reach [-48000, +51000] -- so a true
 * -50 kHz had no hypothesis within half a bin. */
static size_t
search_cover_window_bins (double du, double span)
{
  if (!(du > 0.0))
    return 1;
  /* The spacing of the TILES, one native span wide each, at any depth: a
     depth D inside a tile refines the combined grid to res / D, it does
     not move the tiles. */
  const double res = 2.0 * span;
  /* search_ceil_bins: an exact whole number of bins must not provision a
     spurious extra PAIR of windows. */
  double ns = search_ceil_bins ((du - 0.5 * res) / res);
  if (!(ns > 0.0))
    return 1;
  /* A count past what size_t holds saturates: converting an out-of-range
     double is undefined (x86 yields 2^63, which 2n + 1 wraps to ONE tile).
     The constructor refuses a count no grid can hold before using it. */
  size_t whole, tiles;
  if (!dp_size_from_double (ns, &whole) || !dp_size_mul (whole, 2, &tiles)
      || !dp_size_add (tiles, 1, &tiles))
    return SIZE_MAX;
  return tiles;
}

/* Search the grid for a BURST-mode engine: WHICH (coherent_bins, n_noncoh,
 * window_bins) to use, written to *out_d / *out_nc / *out_window_bins.
 * Deliberately does not touch st and does not allocate anything -- the
 * caller commits the decision via dp_correlator_regrid() (which needs to see
 * the PRIOR grid to know what changed) and then search_commit_thresholds()
 * (which needs the grid dp_correlator_regrid() just committed).  Search only;
 * no side effects.
 *
 * n_noncoh is ALWAYS 1 for a burst engine. The non-coherent path sums
 * |dump|^2 over n_noncoh CONSECUTIVE frames, and the Pd model behind
 * search_ascend_n_noncoh assumes the signal is present in every one of them --
 * true of a continuous signal, false of a burst, whose preamble occupies ONE
 * frame (two when it straddles a boundary). Extra looks add noise to the
 * statistic and nothing else. They also move the hit: samples_consumed is
 * stamped at the end of the LAST accumulated frame, so a consumer resolving
 * the preamble's position sees an anchor up to n_noncoh*coherent_bins code
 * periods late. Measured (doppler#1181): a BurstCapture sized at its DEFAULT
 * cn0_dbhz got n_noncoh=6 from the old escalation and reported preamble
 * starts 9 and 3 periods late on a 34 dB burst, with a refine margin that
 * read BETTER than a correct one. When the coherent ceiling falls short of
 * Pd the honest answer is `underpowered`, which search_commit_thresholds()
 * sets from pd_predicted; it is not more looks.
 *
 * du > span (wideband): *out_window_bins = 2*ceil((du - span)/(2*span)) + 1
 * (search_cover_window_bins(), always odd) tiles the requested uncertainty,
 * and the sizer still chooses a coherent depth inside each tile. Each tile is
 * one native span wide; the slow-time FFT resolves the residual within it. The
 * tile count and depth are both priced into the global threshold.
 *
 * du <= span: picks the smallest coherent depth D in [1, d_max] whose
 * burst Pd (pd_burst: every dwell a uniformly aligned preamble spans, at the
 * (doppler_uncertainty-shrunk) Sidak threshold) meets pd -- minimum
 * latency for a strong signal. If no depth does, D is the one with the most
 * burst Pd and the engine is underpowered. The ceiling d_max is `reps`,
 * lowered by the Doppler rate's drift bound (search_drift_depth(),
 * doppler#1482): a block deeper than that smears the carrier across more than
 * half a slow-time row, a loss the Pd model does not carry, so a deeper D
 * would be sized on a Pd it cannot deliver. */
static void
search_auto_config_burst (const dp_correlator_state_t *st, double pfa,
                          double pd, double snr, double du, size_t *out_d,
                          size_t *out_nc, size_t *out_window_bins)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  cb   = srch->code_bins;
  const double                  span = srch->doppler_span_hz;

  *out_nc          = 1; /* see above: a burst has one frame of preamble */
  *out_window_bins = (du > span) ? search_cover_window_bins (du, span) : 1u;
  const size_t d_max
      = search_drift_depth (srch->chip_rate / (double)srch->sf,
                            search_residual_rate (st), srch->reps);

  /* No design C/N0: integrate the whole preamble. The smallest-depth search
     below trades sensitivity for latency against a stated signal; with no
     signal stated there is nothing to trade, and a burst's preamble is all
     there is to integrate.
     EXCEPT in a tiled search: there the depth multiplies the buffers (a
     block of window_bins * D epochs, 0.1 MB -> 14.5 MB at sf 127, reps 64,
     du 10 * span), the decision latency (reps epochs before the first
     verdict) and the state blob, and the default-built object -- no design
     point given -- has always been D = 1. A depth in a tiled search is the
     answer to a stated C/N0, never a default. */
  if (!(snr > 0.0))
    {
      *out_d = (*out_window_bins > 1) ? 1u : d_max;
      return;
    }

  /* Smallest coherent depth D whose BURST Pd meets pd (minimum latency for
   * strong signals); Sidak uses only the cells actually scanned at that
   * depth.  Sizing and prediction both use the straddle-derated SNR: the
   * on-grid best case would under-size the search (real Pd, averaged over
   * random Doppler/code phase, would miss the target — the gap the
   * Monte-Carlo characterization measures). And both use the burst's
   * alignment: one aligned dwell under-credits a small D, which gets about
   * reps/D dwells, and over-credits a large one, whose dwells straddle the
   * preamble's edge (doppler#1498). */
  size_t       best_d      = 1;
  double       best_p      = -1.0;
  int          met         = 0;
  const size_t window_bins = *out_window_bins;
  for (size_t D = 1; D <= d_max; D++)
    {
      const size_t sb   = window_bins > 1 ? window_bins * D
                                          : search_searched_bins (D, du, span);
      double       umax = search_intra_umax (D, sb, du, span);
      double       pc   = dp_det_pfa_cell (
          pfa, (double)(search_band_cells (D, sb, window_bins, cb)
                        * search_rate_count_at (srch->rate_hyps, D)));
      double       eta   = dp_det_threshold (pc);
      const double k_ref = st->noise_mode == DET_NOISE_MEAN
                               ? (double)(window_bins * D * cb)
                               : 0.0;
      double p = search_mean_pd (snr, D, umax, &srch->shape, (int)(D * cb),
                                 eta, 1, search_interp_for (D, window_bins),
                                 srch->reps, k_ref);
      if (p >= pd)
        {
          best_d = D;
          met    = 1;
          break;
        }
      /* Falling short everywhere, the best-effort grid is the depth with
         the most burst Pd -- NOT the deepest: past (R + 1)/2 a deeper dwell
         straddles the preamble's edge at more alignments and loses more
         than its coherence gains (doppler#1498). Ranked ABOVE each depth's
         own false-alarm floor: far below the design point every depth's Pd
         is that floor, which grows with the dwell count and would pick
         D = 1, the least sensitive depth, for no signal at all. */
      p -= search_burst_pd_at (0.0, 0.0, D, srch->reps, (int)(D * cb), eta,
                               k_ref, &srch->shape);
      if (p > best_p)
        {
          best_p = p;
          best_d = D;
        }
    }
  /* A signal that adds less Pd than the false-alarm rate at every depth is
     indistinguishable from none, and the ranking above is rounding. Do what
     no design C/N0 does: integrate the whole preamble. */
  if (!met && best_p < pfa)
    best_d = d_max;
  *out_d = best_d;
}

/* Search the grid for a CONTINUOUS-mode engine: ALWAYS the wideband
 * window-tiling mechanism, unconditionally (never gated on du > span,
 * unlike the burst path above) -- coherent_bins stays pinned to 1 by the
 * caller regardless of what this returns; only window_bins/n_noncoh need
 * sizing.  search_cover_window_bins() covers du <= 0 (native, single window)
 * through du > span (multi-window tiling) with one formula, never a
 * coherent-depth search -- see the file doc comment for why this mode never
 * attempts coherent multi-epoch combining at all. */
static void
search_auto_config_continuous (const dp_correlator_state_t *st, size_t D,
                               double pfa, double pd, double snr, double du,
                               size_t *out_nc, size_t *out_window_bins)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  cb   = srch->code_bins;
  const size_t                  lb   = srch->lag_bins;
  const double                  span = srch->doppler_span_hz;
  const size_t window_bins           = search_cover_window_bins (du, span);

  *out_window_bins = window_bins;
  /* The searched Doppler cells are the tiles times the rows inside each. */
  *out_nc = search_ascend_n_noncoh (
      snr, D, window_bins * D, window_bins, cb, lb, pfa, pd, &srch->shape, du,
      span,
      st->noise_mode == DET_NOISE_MEAN ? (double)(window_bins * D * lb) : 0.0);
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/* Rebuild every grid-dependent buffer and plan for (new_db, new_nc,
 * new_freq_bins, new_carrier): the reference, the surface and its scratch
 * (out_buf/mag_buf/noise_scratch, the masks), a tiled block's raw epochs,
 * the one delay-Doppler map with its tables (search_map_build: rebuilt when
 * the grid, the fine grid or a ramping carrier moves), growing the ring if
 * the new frame no longer fits, and the non-coherent power accumulator if
 * the grid or new_nc moves.  Allocates the replacements FIRST and only
 * frees/adopts them once every allocation needed has succeeded, so a failure
 * leaves `st` fully untouched at its prior grid (the contract
 * dp_correlator_configure_search_raw() promises its caller).
 *
 * `code`/`code_len` are non-NULL only from dp_correlator_create_burst()'s/
 * dp_correlator_create_continuous()'s initial build (fresh reference from the
 * caller's chips); a later regrid (dp_correlator_configure_search_raw) passes
 * NULL and copies row 0 of the EXISTING reference forward instead of
 * rebuilding it from scratch -- row 0 is the only nonzero row regardless of
 * coherent_bins (see the comment below), so it alone fully captures the
 * code -- unaffected by new_freq_bins growing new_n, since the fill loop
 * below only ever touches indices [0, cb). */
/* The one delay-Doppler map's side of a grid (docs/design/acq-surface.md
 * §18): the map itself, the template's spectrum, the block's epoch spectra,
 * the map's scratch for its schedule, and the tiles -- each one's roll, its
 * rows on the combined axis (search_block_row), its rounded-ramp offsets and
 * its weights (the template at the one scale, 1/code_bins, ramped when the
 * tile runs a code-rate hypothesis). Built apart from the engine and
 * adopted only at the regrid's commit, so a map that will not build fails
 * the regrid with the engine untouched (#2761). The sizes follow the grid,
 * so a failure is reachable and returns -1, never aborts. */
typedef struct
{
  dp__dd_map_t   *map;
  float _Complex *ref_spec;
  float _Complex *spec;
  float _Complex *work;
  dp__dd_tile_t  *tiles;
  size_t         *rows;
  int32_t        *off;
  float _Complex *wt;
  size_t          f;
} search_map_t;

static void
search_map_release (search_map_t *m)
{
  dp__dd_map_destroy (m->map);
  free (m->ref_spec);
  free (m->spec);
  free (m->work);
  free (m->tiles);
  free (m->rows);
  free (m->off);
  free (m->wt);
  memset (m, 0, sizeof *m);
}

/* The map's fine grid: a code-rate ramp needs a carrier, a tiled engine
   (tile 0's dilation is 0) and a block to ramp across (D > 1). */
static size_t
search_map_f (double carrier, size_t D, size_t W)
{
  return carrier > 0.0 && W > 1 && D > 1 ? DP__DD_MAP_FINE : 1;
}

static int
search_map_build (const dp_correlator_state_t *st, const float _Complex *ref,
                  size_t D, size_t W, size_t interp, double carrier,
                  search_map_t *m)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t cb = srch->code_bins, lb = srch->lag_bins, DI = D * interp;
  memset (m, 0, sizeof *m);
  m->f                     = search_map_f (carrier, D, W);
  const dp__dd_geom_t geom = { cb, lb, D, DI, m->f };
  m->map                   = dp__dd_map_create (&geom, 1);
  m->ref_spec              = dp_mallocn (cb, sizeof *m->ref_spec);
  m->spec  = dp_mallocn (dp_size_mul_sat (D, cb), sizeof *m->spec);
  m->tiles = dp_mallocn (W, sizeof *m->tiles);
  m->rows  = dp_mallocn (dp_size_mul_sat (W, DI), sizeof *m->rows);
  m->wt = dp_mallocn (dp_size_mul_sat ((m->f > 1 ? W : 1), cb), sizeof *m->wt);
  if (m->f > 1)
    m->off = dp_mallocn (dp_size_mul_sat (W, cb), sizeof *m->off);
  dp_fft_state_t *fwd = dp_fft_create (cb, -1, 1);
  if (!m->map || !m->ref_spec || !m->spec || !m->tiles || !m->rows || !m->wt
      || (m->f > 1 && !m->off) || !fwd)
    {
      dp_fft_destroy (fwd);
      search_map_release (m);
      return -1;
    }
  const size_t wl = dp__dd_map_work_len (
      m->map, W, search_rate_count_at (srch->rate_hyps, D));
  m->work = dp_mallocn ((wl ? wl : 1), sizeof *m->work);
  if (!m->work)
    {
      dp_fft_destroy (fwd);
      search_map_release (m);
      return -1;
    }
  /* conj(FFT(replica)), and the unramped weights at the one scale: the
     template over code_bins (the inverse is unnormalized). Without a ramp
     every tile shares them; with one, each tile's are its own, ramped. */
  dp_fft_execute_cf32 (fwd, ref, cb, m->ref_spec, cb);
  dp_fft_destroy (fwd);
  float _Complex *base = m->f > 1 ? dp_mallocn (cb, sizeof *base) : m->wt;
  if (!base)
    {
      search_map_release (m);
      return -1;
    }
  for (size_t j = 0; j < cb; j++)
    {
      m->ref_spec[j] = conjf (m->ref_spec[j]);
      base[j]        = m->ref_spec[j] / (float)cb;
    }
  for (size_t q = 0; q < W; q++)
    {
      const long sq = dp_fftfreq_index (q, W);
      for (size_t i = 0; i < DI; i++)
        m->rows[q * DI + i] = search_block_row (q, W, i, DI);
      dp__dd_tile_t *t = &m->tiles[q];
      t->roll          = (size_t)(((sq % (long)cb) + (long)cb) % (long)cb);
      t->rows          = m->rows + q * DI;
      t->off           = NULL;
      t->wt            = m->wt;
      if (m->f > 1)
        {
          /* The tile's code-rate hypothesis (doppler#1256): its frequency,
             sq bins of fs/cb, dilates the chip clock by f/carrier, so the
             code advances sq fs / carrier samples an epoch; the map reads
             it on its fine grid about the block's middle (§18.1). */
          const double dq = (double)sq * srch->fs / carrier;
          dp__dd_ramp_table (&geom, dq, base, m->off + q * cb, m->wt + q * cb);
          t->off = m->off + q * cb;
          t->wt  = m->wt + q * cb;
        }
    }
  if (m->f > 1)
    free (base);
  return 0;
}

/* Run the caller's request (threads_req) on the grid in effect. Never more
   workers than tiles: every fan hands out one tile a task, so a worker past
   window_bins only wakes and waits, every epoch. At 0 (the online cores) on
   a three-tile search that was 15 idle workers of 18, and the pool's scene
   example spent 26 s of 35 in the kernel. A count already running is kept,
   so a regrid that leaves the cap where it was does not rebuild the pool. */
static void
search_apply_threads (dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  const int                     n    = srch->threads_req;
  long                          want = 1;
  if (n != 1 && srch->window_bins > 1)
    {
      want = n > 0 ? n : dp_cpu_count ();
      if (want > (long)srch->window_bins)
        want = (long)srch->window_bins;
    }
  if (want < 1)
    want = 1;
  /* The map's worker slots, one a chunk it may fan into: kept at least the
     count asked for. A failed reserve leaves fewer slots, and the map fans
     over the slots it has: slower, never wrong. */
  if (st->dd_map)
    (void)dp__dd_map_reserve (st->dd_map, (int)want);
  if (want == srch->threads && (want == 1) == (st->pool == NULL))
    return;
  dp_pool_destroy (st->pool);
  st->pool      = NULL;
  srch->threads = 1;
  if (want <= 1)
    return;
  st->pool      = dp_pool_create ((int)want);
  srch->threads = dp_pool_threads (st->pool);
}

static int
dp_correlator_regrid (dp_correlator_state_t *st, size_t new_db, size_t new_nc,
                      size_t new_freq_bins, const float _Complex *replica,
                      double new_carrier)
{
  dp_correlator_search_t *const srch  = st->search;
  const size_t                  cb    = srch->code_bins;
  const size_t                  lb    = srch->lag_bins;
  const size_t                  new_n = new_db * new_freq_bins * cb;
  const int    grid_changed = (new_db != srch->coherent_bins)
                              || (new_n != srch->n)
                              || (new_freq_bins != srch->window_bins);
  const size_t new_frame_n  = (new_freq_bins > 1) ? cb : new_n;
  /* Interpolate each coherent Doppler axis. In wideband mode the window axis
    supplies coarse frequency tiles; each tile can still have a coherent
    slow-time FFT and its own interpolated rows. */
  const size_t new_interp = search_interp_for (new_db, new_freq_bins);
  /* The surface: every Doppler row (D * interp a tile) by lag_bins code
     phases. */
  const size_t new_n_surf = new_db * new_freq_bins * new_interp * lb;
  /* The map follows the grid and, through its ramp, the carrier: a change
     of carrier on an engine that ramps (or starts or stops ramping) moves
     the fine grid and every tile's offsets and weights, and nothing else.
     A carrier-only change rebuilds only the map; the dwell's running state
     (the non-coherent looks, the kept surface, the sidelobe cache) stays. */
  const size_t new_f = search_map_f (new_carrier, new_db, new_freq_bins);
  const int    map_changed
      = grid_changed || !st->dd_map || new_f != srch->map_f
        || (new_f > 1 && new_carrier != srch->carrier_freq_hz);
  const int state_reset = grid_changed || new_nc != srch->n_noncoh;

  float _Complex       *new_ref      = NULL;
  float _Complex       *new_out      = NULL;
  float                *new_mag      = NULL;
  float                *new_scratch  = NULL;
  uint8_t              *new_band     = NULL;
  uint8_t              *new_pmask    = NULL;
  float                *new_ncsurf   = NULL;
  dp_f32_t             *new_ring     = NULL;
  size_t                new_ring_cap = st->ring_cap;
  float _Complex       *new_blk_raw  = NULL;
  dp_correlator_part_t *new_parts    = NULL;
  search_map_t          new_map      = { 0 };
  /* Every buffer below is sized from a factor of the surface (code_bins <=
     lag_bins, so the frame, the map's spectra and the raw block are no
     larger) or of the tiles: check the products once, in bytes, before
     anything is sized from them (#2958). */
  size_t surf_bytes = 0, parts_bytes = 0, work_bytes = 0;
  if (!dp_size_mul (new_db, new_freq_bins, &surf_bytes)
      || !dp_size_mul (surf_bytes, new_interp, &surf_bytes)
      || !dp_size_mul (surf_bytes, lb, &surf_bytes)
      || !dp_size_mul (surf_bytes, sizeof (float _Complex), &surf_bytes)
      || !dp_size_mul (new_freq_bins, sizeof (dp_correlator_part_t),
                       &parts_bytes)
      /* The map's slow-first scratch, f x DI x code_bins at the finest
         f, can be twice the surface: part of the same truth. */
      || !dp_size_mul (new_db, new_interp, &work_bytes)
      || !dp_size_mul (work_bytes, DP__DD_MAP_FINE, &work_bytes)
      || !dp_size_mul (work_bytes, cb, &work_bytes)
      || !dp_size_mul (work_bytes, sizeof (float _Complex), &work_bytes))
    goto fail;
  if (grid_changed)
    {
      /* Single-row reference: row 0 (indices [0, cb)) carries one period
       * of the replica -- the constructor's on the first grid, the one
       * already held on every regrid after it. */
      new_ref = (float _Complex *)calloc (new_n_surf, sizeof (float _Complex));
      if (!new_ref)
        goto fail;
      memcpy (new_ref, replica ? replica : srch->ref,
              cb * sizeof (float _Complex));
      new_out
          = (float _Complex *)dp_mallocn (new_n_surf, sizeof (float _Complex));
      new_mag     = (float *)dp_mallocn (new_n_surf, sizeof (float));
      new_scratch = (float *)dp_mallocn (new_n_surf, sizeof (float));
      /* The masks are surface-sized, like the buffers above, and this
         function has an unwind: a grid too large to allocate is refused
         (DP_ERR_MEMORY to configure_search_raw), never an abort. */
      new_band  = (uint8_t *)dp_mallocn (new_n_surf, 1);
      new_pmask = (uint8_t *)dp_mallocn (new_n_surf, 1);
      if (!new_out || !new_mag || !new_scratch || !new_band || !new_pmask)
        goto fail;
      memset (new_band, 0, new_n_surf);
      /* A tiled block's raw epochs, held until the block's end, when the
         map runs on them (and search_resolve_tile_alias asks about them). */
      if (new_freq_bins > 1 && new_db > 1)
        {
          new_blk_raw = (float _Complex *)dp_mallocn (
              dp_size_mul_sat (new_db, cb), sizeof (float _Complex));
          if (!new_blk_raw)
            goto fail;
        }
      /* The partial slots (one per tile) follow the grid, built here with
         the rest so nothing fails after the commit (#2761). */
      new_parts = (dp_correlator_part_t *)dp_mallocn (new_freq_bins,
                                                      sizeof *new_parts);
      if (!new_parts)
        goto fail;
    }
  if (map_changed
      && search_map_build (st, new_ref ? new_ref : srch->ref, new_db,
                           new_freq_bins, new_interp, new_carrier, &new_map)
             != 0)
    goto fail;

  if (new_frame_n > new_ring_cap)
    {
      new_ring = det_ring_create (new_frame_n > 512 ? new_frame_n : 512);
      if (!new_ring)
        goto fail;
      new_ring_cap = new_ring->capacity;
    }

  /* Non-coherent power accumulator — only on the N_nc > 1 path. */
  if (state_reset && new_nc > 1)
    {
      new_ncsurf = (float *)calloc (new_n_surf, sizeof (float));
      if (!new_ncsurf)
        goto fail;
    }

  /* Every allocation needed succeeded — commit: free the old, adopt the
   * new. */
  if (grid_changed)
    {
      free (srch->ref);
      free (srch->out_buf);
      free (st->mag_buf);
      free (st->noise_scratch);
      free (st->band_mask);
      free (st->peak_mask);
      st->band_mask     = new_band;
      st->peak_mask     = new_pmask;
      srch->ref         = new_ref;
      srch->out_buf     = new_out;
      st->mag_buf       = new_mag;
      st->noise_scratch = new_scratch;
      free (srch->blk_raw);
      srch->blk_raw   = new_blk_raw;
      srch->blk_epoch = 0;
      free (srch->parts);
      srch->parts = new_parts;
    }
  if (map_changed)
    {
      search_map_t old = { st->dd_map,     srch->ref_spec,  srch->map_spec,
                           srch->map_work, srch->map_tiles, srch->map_rows,
                           srch->map_off,  srch->map_wt,    srch->map_f };
      search_map_release (&old);
      st->dd_map      = new_map.map;
      srch->ref_spec  = new_map.ref_spec;
      srch->map_spec  = new_map.spec;
      srch->map_work  = new_map.work;
      srch->map_tiles = new_map.tiles;
      srch->map_rows  = new_map.rows;
      srch->map_off   = new_map.off;
      srch->map_wt    = new_map.wt;
      srch->map_f     = new_map.f;
    }
  srch->carrier_freq_hz = new_carrier;
  if (new_ring)
    {
      if (st->ring)
        dp_f32_destroy (st->ring);
      st->ring     = new_ring;
      st->ring_cap = new_ring_cap;
    }
  srch->coherent_bins = new_db;
  srch->window_bins   = new_freq_bins;
  srch->n_noncoh      = new_nc;
  srch->n             = new_n;
  srch->interp        = new_interp;
  srch->n_surf        = new_n_surf;
  srch->frame_n       = new_frame_n;
  if (state_reset)
    {
      free (srch->nc_surface);
      srch->nc_surface = new_ncsurf;
      /* Every sidelobe slot was read on the old grid: its rows' Doppler, its
         tiles and its resolution all follow D, window_bins and interp, so
         none survives a regrid (configure_search_raw forces one tile, and
         may change D as well). */
      srch->sl_cn = srch->sl_cnext = 0;
      /* A new surface size: the kept surface (search_surface) is re-made
         lazily at the next decided dwell. */
      free (srch->stat_surface);
      srch->stat_surface = NULL;
      srch->surface_at   = 0;
      st->noise_lo       = 0;
      st->noise_hi       = new_n_surf - 1;
    }
  /* The tile count may have moved, so the cap on the caller's request may
     have too: narrowing to one tile runs serially, and widening again comes
     back to the request, not to that 1 (#2746, #2797). */
  search_apply_threads (st);
  return 0;

fail:
  free (new_ref);
  free (new_out);
  free (new_mag);
  free (new_scratch);
  free (new_band);
  free (new_pmask);
  free (new_ncsurf);
  free (new_blk_raw);
  free (new_parts);
  search_map_release (&new_map);
  if (new_ring)
    dp_f32_destroy (new_ring);
  return -1;
}

/* The correlation shape of a PN code held `spc` samples per chip: its
 * autocorrelation is a triangle one chip wide, so peaks within a chip of
 * each other are one emitter, and a peak `delta` samples off the grid keeps
 * `1 - delta/spc` of its amplitude -- `1 - 1/(4*spc)` on average over the
 * half-sample prior. Each node is written in the one form the Pd model has
 * always evaluated it in, so a code engine sizes exactly as before. */
static dp_correlator_shape_t
search_shape_of_chips (size_t spc)
{
  dp_correlator_shape_t sh = { 0 };
  double                u[DP_CORRELATOR_DELAY_LOSS_NODES];
  sh.zone            = spc;
  sh.delay_loss_mean = 1.0 - 1.0 / (4.0 * (double)spc);
  dp_midpoint_nodes (u, DP_CORRELATOR_DELAY_LOSS_NODES);
  for (int k = 0; k < DP_CORRELATOR_DELAY_LOSS_NODES; k++)
    sh.delay_loss[k] = 1.0 - (0.5 / (double)spc) * u[k];
  return sh;
}

/* Amplitude a periodic template's correlation keeps at a fractional lag
 * `delta` (samples), relative to lag 0 -- the autocorrelation evaluated
 * band-limited from the power spectrum P: sum_k P_k e^{j 2 pi f_k delta / n}
 * over the SIGNED bin f_k, with an even n's Nyquist bin taking only its real
 * part, cos(pi delta), because it belongs to both halves. */
static double
search_lag_amplitude (const double *P, size_t n, double delta, double p_sum)
{
  double re = 0.0, im = 0.0;
  for (size_t k = 0; k < n; k++)
    {
      if ((n & 1u) == 0 && k == n / 2)
        {
          re += P[k] * cos (M_PI * delta);
          continue;
        }
      double f = (k <= n / 2) ? (double)k : (double)k - (double)n;
      double a = 2.0 * M_PI * f * delta / (double)n;
      re += P[k] * cos (a);
      im += P[k] * sin (a);
    }
  return hypot (re, im) / p_sum;
}

/* The correlation shape of an arbitrary periodic template (doppler#1470),
 * from one FFT: everything the engine asks of the waveform is a function
 * of its power spectrum P.
 *
 * - The zone is the autocorrelation's first null: the first lag m >= 1
 *   where |R(m)| stops falling -- the next lag is not smaller by more than
 *   1e-6 of R(0). R is the inverse transform of P. For a chip held spc
 *   samples R is a triangle whose floor starts at m = spc, so this lands
 *   where the analytic shape puts it; a perfect sequence (Zadoff-Chu) is
 *   null at 1. The tolerance is what makes both work: a floor is flat, and
 *   a perfect sequence's off-peak lags are rounding noise (~3e-9 of R(0)
 *   for a float 127-sample Zadoff-Chu), so an exact comparison would put
 *   the minimum wherever the FFT's last digit fell.
 *   A short code can reach its floor BEFORE one chip: CODE7's floor is 1/7,
 *   and its sampled triangle is already 1/7 at lag 3 of 4 -- the first null
 *   is then honestly 3, and meets the analytic "one chip" only once the
 *   floor sits below the triangle's last step (a 31-chip code: 0.226, then
 *   0.032).
 * - The delay straddle is search_lag_amplitude() at the Pd model's nodes and
 *   averaged over [0, 1/2] sample (64-interval Simpson, dp_simpson_weights()).
 */
/* The periodic autocorrelation of x, R(m) / n for m = 0 .. n-1, through the
 * correlator's single-row path (x correlated against itself): the one
 * correlation in the library, in cf32 (design correlator.md U4). Returns a
 * fresh buffer the caller frees. */
static float _Complex *
search_autocorr (const float _Complex *x, size_t n)
{
  /* n is the caller's period: a correlator of that length that cannot be
     built is a refusal on the create path (NULL), not an abort. */
  dp_correlator_state_t *c = dp_correlator_create_1d (x, n, 1, 1, 0);
  if (!c)
    return NULL;
  float _Complex *R   = dp_xmallocn (n, sizeof *R);
  const size_t    got = dp_correlator_execute (c, x, n, R, n, NULL);
  (void)got; /* n in, dwell 1: one dump of n, always */
  dp_correlator_destroy (c);
  return R;
}

/* 0, or -1 (nothing left allocated) when a transform of the caller's
   period cannot be built: the create refuses rather than aborts. */
static int
search_shape_of_template (const float _Complex *t, size_t n,
                          dp_correlator_shape_t *sh)
{
  dp_fft_state_t *fwd = dp_fft_create (n, -1, 1);
  if (!fwd)
    return -1;
  float _Complex *R = search_autocorr (t, n);
  if (!R)
    {
      dp_fft_destroy (fwd);
      return -1;
    }
  double _Complex *buf   = dp_xmallocn (n, sizeof *buf);
  double _Complex *spec  = dp_xmallocn (n, sizeof *spec);
  double          *P     = dp_xmallocn (n, sizeof *P);
  double           p_sum = 0.0;

  /* The power spectrum, for the delay-loss model below: a spectrum, not a
     correlation, so it stays a double transform. */
  for (size_t i = 0; i < n; i++)
    buf[i] = (double)crealf (t[i]) + I * (double)cimagf (t[i]);
  dp_fft_execute_cf64 (fwd, buf, n, spec, n);
  for (size_t k = 0; k < n; k++)
    {
      P[k] = creal (spec[k]) * creal (spec[k])
             + cimag (spec[k]) * cimag (spec[k]);
      p_sum += P[k];
    }

  const double r0   = cabsf (R[0]);
  const size_t half = n / 2;
  size_t       zone = half > 0 ? half : 1;
  const double tol  = 1e-6 * r0;
  for (size_t m = 1; m <= half; m++)
    {
      double here = cabsf (R[m]);
      if (m < half && cabsf (R[m + 1]) > here - tol)
        {
          zone = m;
          break;
        }
    }
  sh->zone = zone;
  free (R);

  double u[DP_CORRELATOR_DELAY_LOSS_NODES];
  dp_midpoint_nodes (u, DP_CORRELATOR_DELAY_LOSS_NODES);
  for (int k = 0; k < DP_CORRELATOR_DELAY_LOSS_NODES; k++)
    sh->delay_loss[k] = search_lag_amplitude (P, n, 0.5 * u[k], p_sum);

  double w[65];
  (void)dp_simpson_weights (w, 65);
  double m = 0.0;
  for (int i = 0; i < 65; i++)
    m += w[i] * search_lag_amplitude (P, n, 0.5 * (double)i / 64.0, p_sum);
  sh->delay_loss_mean = m;

  dp_fft_destroy (fwd);
  free (P);
  free (spec);
  free (buf);
  return 0;
}

/* The periodic autocorrelation OFF the peak, relative to it: its energy
 * sum |R(m)|^2 / R(0)^2 and its amplitude sum |R(m)| / R(0) over m != 0,
 * into sh->off_peak and sh->off_peak_amp, and its largest lag OUTSIDE the
 * mainlobe (zone <= m <= n - zone, sh->zone already set) into sh->psl. R
 * is the inverse transform of the replica's power spectrum. The ONE home
 * of all three, for a code and a template alike (doppler#1501,
 * doppler#1470). A perfect sequence's off-peak lags are rounding (~1e-9 of
 * R(0)); below 1e-6 they are counted as nothing. */
static int
search_off_peak (const float _Complex *replica, size_t n,
                 dp_correlator_shape_t *sh)
{
  float _Complex *R = search_autocorr (replica, n); /* R(m) / n */
  if (!R)
    return -1; /* the create refuses: see search_autocorr */
  const double r0 = cabsf (R[0]);
  double       e1 = 0.0, e2 = 0.0, psl = 0.0;
  const size_t z = sh->zone;
  for (size_t m = 1; m < n; m++)
    {
      const double r = r0 > 0.0 ? cabsf (R[m]) / r0 : 0.0;
      if (r < 1e-6)
        continue;
      e1 += r;
      e2 += r * r;
      if (m >= z && m <= n - z && r > psl)
        psl = r;
    }
  sh->off_peak     = e2;
  sh->off_peak_amp = e1;
  sh->psl          = psl;
  free (R);
  return 0;
}

/* Residual points per tile width in the Doppler-offset sidelobe's sweep.
 * The worst sidelobe moves with the residual: on the receivers' Gold-1023
 * search (S = 2, five tiles) a 1/128 grid reads -16.95 dB as a 10 Hz sweep
 * does, 1/32 under-reads it by 0.16 dB and 1/16 by 0.32 dB. */
#define DP_CORRELATOR_DSL_POINTS 128

/* The periodic ambiguity's largest lag OUTSIDE the mainlobe, at a Doppler
 * offset, relative to the emitter's own peak, into sh->dsl (design
 * acq-surface.md §8). An emitter at residual r from its tile's centre reads
 * in tile k (k tiles away) at nu = r + k * f_T, f_T = fs/n the tile width:
 *
 *   |chi(m, nu)| = |sum_n c[n] conj(c[n - m]) e^{j 2 pi nu n / fs}|
 *
 * and its own peak is the best of |chi(0, nu)| over the tiles. dsl is the
 * worst ratio of the off-mainlobe lags (zone <= m <= n - zone, psl's range)
 * to that peak, over r and k. A wideband grid sweeps r across one tile and k
 * over every offset two of its tiles can have, -(W-1) .. W-1 -- a superset
 * of the tiles any one emitter sees, so the value bounds them all; a
 * single-tile grid sweeps r over +-du, its whole band. At du = 0 it is psl.
 *
 * One forward transform per residual: shifting nu by a whole tile shifts
 * the spectrum by one bin, f_T being the transform's own bin spacing, so
 * tile k is the residual's spectrum rolled by k. Defined on the
 * construction grid, like psl: a grid pinned later
 * (dp_correlator_configure_search_raw()) keeps it. Run by
 * dp_correlator_dsl_db() on its first call, not by the constructor. */
static void
search_doppler_sidelobe (const float _Complex *replica, size_t n,
                         size_t window_bins, double du, double fs,
                         dp_correlator_shape_t *sh)
{
  /* One tile and no Doppler uncertainty: the sweep is the single point
     nu = 0, where chi is the autocorrelation and dsl IS psl -- the same
     lags, the same 1e-6 floor. Take the stored value rather than compute
     it a second time (and, since U4, in another precision). */
  if (window_bins <= 1 && !(du > 0.0))
    {
      sh->dsl = sh->psl;
      return;
    }
  /* On the one delay-Doppler map (correlator_map.h), at D = DI = F = 1:
     one forward transform per residual, then one row per tile offset,
     since rolling the residual's spectrum by k bins is the turn by k tiles
     (f_T is the transform's bin spacing). In cf32, the map's precision
     (design correlator.md U4, 3.4b-pre). */
  const dp__dd_geom_t g   = { .n = n, .l = n, .d = 1, .di = 1, .f = 1 };
  dp__dd_map_t       *map = dp_xnn (dp__dd_map_create (&g, 1));
  float _Complex     *wt  = dp_xmallocn (n, sizeof *wt);
  float _Complex     *sig = dp_xmallocn (n, sizeof *sig);
  float _Complex     *X   = dp_xmallocn (n, sizeof *X);
  float _Complex     *chi = dp_xmallocn (n, sizeof *chi);
  dp__dd_map_forward (map, NULL, 0, replica, wt); /* the replica's spectrum */
  for (size_t b = 0; b < n; b++)
    wt[b] = conjf (wt[b]);

  const double f_tile = fs / (double)n;
  const double half = window_bins > 1 ? 0.5 * f_tile : fmin (du, 0.5 * f_tile);
  const long   k_max = (long)window_bins - 1;
  const size_t pts   = half > 0.0 ? DP_CORRELATOR_DSL_POINTS + 1 : 1;
  const size_t z     = sh->zone;
  double       dsl   = 0.0;
  for (size_t p = 0; p < pts; p++)
    {
      const double r
          = pts > 1 ? -half + 2.0 * half * (double)p / (double)(pts - 1) : 0.0;
      for (size_t i = 0; i < n; i++)
        {
          const double ph = 2.0 * M_PI * r * (double)i / fs;
          sig[i]          = (float _Complex) (
              ((double)crealf (replica[i]) + I * (double)cimagf (replica[i]))
              * cexp (I * ph));
        }
      dp__dd_map_forward (map, NULL, 0, sig, X); /* residual's spectrum */
      double own = 0.0, side = 0.0;
      for (long k = -k_max; k <= k_max; k++)
        {
          /* nu = r + k f_T: the spectrum read k bins back, times conj(ref):
             row1 reads bin (b + roll) mod n. */
          const size_t roll = (size_t)(((-k) % (long)n + (long)n) % (long)n);
          dp__dd_map_row1 (map, 0, X, roll, wt, chi); /* n * chi(m, nu) */
          const double peak = cabsf (chi[0]);
          if (peak > own)
            own = peak;
          for (size_t m = z; m <= n - z && m < n; m++)
            if (cabsf (chi[m]) > side)
              side = cabsf (chi[m]);
        }
      const double ratio = own > 0.0 ? side / own : 0.0;
      if (ratio > dsl)
        dsl = ratio;
    }
  sh->dsl = dsl < 1e-6 ? 0.0 : dsl;
  dp__dd_map_destroy (map);
  free (chi);
  free (X);
  free (sig);
  free (wt);
}

/* rate_hypotheses = 0 (auto): the fewest hypotheses the span needs, chosen
 * from the sizer itself rather than a rule of thumb. For each n the drift
 * bound is either binding or not: D*(n) is the depth the sizer picks at n
 * hypotheses with the bound OFF (priced with n looks per cell, which is what
 * moves it), and D_cap(n) the depth the residual rate R/(n - 1) tolerates
 * (n = 1 has no compensation: R itself). The first n with D_cap(n) >= D*(n)
 * is the first at which the hypotheses stop costing depth -- more of them
 * would only multiply the per-dwell work and the false-alarm count. None
 * does within DP_CORRELATOR_RATE_HYPS_MAX: take the maximum, the best this can
 * do. Leaves st->search->rate_hyps at the choice; doppler_rate is restored. */
static void
search_auto_rate_hypotheses (dp_correlator_state_t *st, double pfa, double pd,
                             double snr, double du)
{
  dp_correlator_search_t *const srch    = st->search;
  const double                  f_epoch = srch->chip_rate / (double)srch->sf;
  const double                  span    = srch->doppler_rate;
  size_t                        n;
  for (n = 1; n < DP_CORRELATOR_RATE_HYPS_MAX; n++)
    {
      size_t d_star, nc, wb;
      srch->rate_hyps    = n;
      srch->doppler_rate = 0.0;
      search_auto_config_burst (st, pfa, pd, snr, du, &d_star, &nc, &wb);
      srch->doppler_rate = span;
      /* The residual from its one home: rate_hyps = n and doppler_rate = span
         are both set, which is what search_residual_rate() reads. */
      if (search_drift_depth (f_epoch, search_residual_rate (st), srch->reps)
          >= d_star)
        break;
    }
  srch->rate_hyps = n;
}

/* Shared builder: allocates and configures the engine, dropping straight
 * into whichever auto-sizer `continuous` selects.  Not declared in the
 * header -- dp_correlator_create_burst()/dp_correlator_create_continuous() are
 * the only public entry points, each fixing @p reps/@p
 * symbol_rate/`continuous` to its own mode, never a per-call knob (see the
 * file doc comment).
 *
 * It knows the waveform only as one period of @p replica (@p n_period
 * samples, sf chips) and its correlation @p shape: the unit the rates are
 * counted in is a chip for a code, and the arithmetic is the same whatever
 * that unit is. */
static dp_correlator_state_t *
search_acq_create_impl (const float _Complex *replica, size_t sf,
                        const dp_correlator_shape_t *shape, size_t reps,
                        size_t n_period, double chip_rate, double symbol_rate,
                        double cn0_dbhz, double doppler_uncertainty,
                        double pfa, double pd, int noise_mode, int continuous,
                        size_t code_only_epochs, double doppler_rate,
                        size_t rate_hypotheses, const char **why)
{
  /* Validate: bad arguments yield NULL (each binding raises its declared
   * create_error for it) rather than undefined behaviour downstream.
   * chip_rate > 0
   * acts as the required sentinel (its toml placeholder default is valid,
   * but an explicit 0 is rejected).  cn0_dbhz is the DESIGN C/N0, any
   * finite value (doppler#1484): a burst engine also takes
   * DP_CORRELATOR_CN0_NONE (NaN) as "none given" and sizes for the whole
   * preamble; a continuous engine has no such sizing -- non-coherent looks are
   * its only sensitivity lever and they cannot be chosen without a target --
   * so it requires a finite one. An infinite C/N0 is no design point either.
   */
  const double span = (sf > 0) ? chip_rate / (2.0 * (double)sf) : 0.0;
  /* doppler_uncertainty > span is not rejected: it engages wideband mode
   * (see the file doc comment's "Wideband window-tiling mode" section)
   * instead of being an out-of-range error. It must be finite, though: a
   * NaN or an infinity reached the searched-bin and window counts through
   * a float-to-size conversion (doppler#2103). */
  /* Each refusal names its knob (dp_correlator_create()'s `why`). A
     continuous engine always arrives with rate_hypotheses 1 (its public
     constructor has no such knob, and the dispatcher refuses one). */
  if (!replica || sf < 1 || n_period < sf)
    return NULL; /* the dispatcher refuses an empty ref first */
  if (reps < 1)
    CORR_WHY ("Correlator: reps must be at least 1 (preamble repetitions a "
              "burst)");
  /* A burst's chip rate is its sample rate (one chip = one sample). */
  if (!(chip_rate > 0.0 && isfinite (chip_rate)) && continuous)
    CORR_WHY ("Correlator: chip_rate must be finite and positive");
  if (!(chip_rate > 0.0 && isfinite (chip_rate)))
    CORR_WHY ("Correlator: sample_rate must be finite and positive");
  if (continuous && !isfinite (cn0_dbhz))
    CORR_WHY ("Correlator: the continuous search sizes its dwell from a "
              "design point; it needs a finite cn0_dbhz");
  if (!continuous && !(isfinite (cn0_dbhz) || isnan (cn0_dbhz)))
    CORR_WHY ("Correlator: cn0_dbhz must be finite, or NaN for none");
  if (!(pfa > 0.0 && pfa < 1.0))
    CORR_WHY ("Correlator: pfa must lie strictly between 0 and 1");
  if (!(pd > 0.0 && pd < 1.0))
    CORR_WHY ("Correlator: pd must lie strictly between 0 and 1");
  if (!(doppler_uncertainty >= 0.0 && isfinite (doppler_uncertainty)))
    CORR_WHY ("Correlator: doppler_uncertainty must be finite and >= 0");
  if (code_only_epochs < 1)
    CORR_WHY ("Correlator: code_only_epochs must be at least 1");
  /* A grid whose samples cannot even be counted in a size_t: refused
     before anything is sized from it (a tile is one period of complex
     samples, and a burst holds reps periods). */
  size_t held = 0;
  /* The period's bytes always fit: a burst's period is its ref, already
     in memory, and a continuous one was bounded where it was made
     (search_period_at). */
  const size_t period_bytes = n_period * sizeof (float _Complex);
  if (!dp_size_mul (search_cover_window_bins (doppler_uncertainty, span),
                    period_bytes, &held))
    CORR_WHY ("Correlator: doppler_uncertainty asks for more Doppler tiles "
              "than any search grid can hold");
  if (!dp_size_mul (reps, period_bytes, &held))
    CORR_WHY ("Correlator: reps asks for more periods than any search grid "
              "can hold");
  if (!(doppler_rate >= 0.0) || !isfinite (doppler_rate))
    CORR_WHY ("Correlator: doppler_rate must be finite and >= 0");
  if (rate_hypotheses > DP_CORRELATOR_RATE_HYPS_MAX)
    CORR_WHY ("Correlator: rate_hypotheses must be at most 64 (0 = auto)");
  if (rate_hypotheses != 1 && !(doppler_rate > 0.0))
    CORR_WHY ("Correlator: rate_hypotheses other than 1 needs doppler_rate "
              "> 0, the span the hypotheses cover");

  dp_correlator_state_t *st
      = (dp_correlator_state_t *)calloc (1, sizeof (*st));
  if (!st)
    return NULL;
  st->search = &st->search_rec;
  st->flavor = continuous ? DP_CORRELATOR_SEARCH_CONTINUOUS
                          : DP_CORRELATOR_SEARCH_BURST;

  st->search->sf    = sf;
  st->search->shape = *shape;
  if (search_off_peak (replica, n_period, &st->search->shape))
    goto fail;
  st->search->reps      = reps;
  st->search->code_bins = n_period;
  /* The surface's lag axis (the field's doc): a continuous engine whose
     period is an awkward length pads its inverse transforms to the next
     power of two -- one forward transform an epoch stays at the period,
     the many inverses (one a tile) get fast. A burst engine keeps the
     template's own length. */
  st->search->lag_bins
      = (continuous && dp_fft_smooth_floor (n_period) != n_period)
            ? dp_next_pow_two (n_period)
            : n_period;
  st->search->chip_rate           = chip_rate;
  st->search->fs                  = chip_rate * (double)n_period / (double)sf;
  st->search->cn0_dbhz            = cn0_dbhz;
  st->search->doppler_span_hz     = span;
  st->search->pd                  = pd;
  st->search->pfa                 = pfa;
  st->search->doppler_uncertainty = doppler_uncertainty;
  st->noise_mode                  = (det_noise_mode_t)noise_mode;
  st->search->symbol_rate         = (symbol_rate > 0.0) ? symbol_rate : 0.0;
  st->search->epochs_per_symbol
      = (st->search->symbol_rate > 0.0)
            ? (chip_rate / (double)sf) / st->search->symbol_rate
            : 0.0;

  st->search->code_only_epochs = code_only_epochs;
  st->search->doppler_rate     = doppler_rate;
  st->search->rate_hyps        = rate_hypotheses;
  st->search->burst            = (uint8_t)!continuous;
  st->search->carrier_freq_hz  = 0.0;

  const double snr    = search_design_snr (cn0_dbhz, st->search->fs);
  size_t       best_d = 0, best_nc = 0, best_window_bins = 1;
  if (continuous)
    {
      best_d = search_block_depth (chip_rate / (double)sf, code_only_epochs,
                                   doppler_rate);
      search_auto_config_continuous (st, best_d, pfa, pd, snr,
                                     doppler_uncertainty, &best_nc,
                                     &best_window_bins);
    }
  else
    {
      if (rate_hypotheses == 0)
        search_auto_rate_hypotheses (st, pfa, pd, snr, doppler_uncertainty);
      search_auto_config_burst (st, pfa, pd, snr, doppler_uncertainty, &best_d,
                                &best_nc, &best_window_bins);
    }

  /* The roll per thread (docs/design/acq-surface.md §13): a continuous
     engine asks for the machine's online cores, which dp_correlator_regrid
     caps at one worker a tile, so a single-tile one runs serially; a burst
     engine asks for one. dp_correlator_set_threads() changes the request. */
  st->search->threads     = 1;
  st->search->threads_req = continuous ? 0 : 1;
  if (dp_correlator_regrid (st, best_d, best_nc, best_window_bins, replica,
                            st->search->carrier_freq_hz)
      != 0)
    {
      /* A grid too large to allocate is a value's doing when one of its
         axes was asked for: name the knob that sized the larger one. A
         grid with neither stays the generic out-of-memory refusal. */
      dp__search_destroy (st);
      if (best_window_bins > 1 && best_window_bins >= best_d)
        CORR_WHY ("Correlator: doppler_uncertainty asks for more Doppler "
                  "tiles than the search grid can allocate");
      if (!continuous && best_d > 1)
        CORR_WHY ("Correlator: reps asks for a coherent depth whose search "
                  "grid cannot be allocated");
      return NULL;
    }
  /* dsl is defined on THIS grid; dp_correlator_dsl_db() computes it on first
     read. A sweep of pts x (2W - 1) transforms (0.15 s on the receivers'
     Gold-1023 search) that only a peak-list consumer reads. */
  st->search->dsl_window_bins = st->search->window_bins;
  search_commit_thresholds (st, pfa, pd, snr, doppler_uncertainty);
  if (dp_correlator_set_max_peaks (st, 1, NULL) != 0)
    goto fail;
  st->search->epoch_fan = 1;
  dp__search_reset (st); /* binds the framer at the grid just chosen */
  return st;

fail:
  dp__search_destroy (st);
  return NULL;
}

/* A PN code as the builder's waveform: one period of BPSK (chip 0 -> +1,
 * chip 1 -> -1) over @p n_period samples, and its correlation shape. sf is
 * the code length. The CONTINUOUS engine's only: a burst engine takes its
 * preamble as samples (dp_correlator_create_burst()).
 *
 * A whole number of samples a chip holds each chip that many samples and
 * takes the triangle's analytic shape, exactly as an integer oversample
 * always has. Any other period gives each sample the chip its centre falls
 * in, `floor((k + 1/2) * code_len / n_period)`, which is what sampling the
 * chip waveform at that rate yields; its chips are then not all the same
 * length, so the shape is measured from the replica itself
 * (search_shape_of_template()), as a burst engine's is. */
static dp_correlator_state_t *
search_create_from_chips (const uint8_t *code, size_t code_len, size_t reps,
                          size_t n_period, double chip_rate,
                          double symbol_rate, double cn0_dbhz,
                          double doppler_uncertainty, double pfa, double pd,
                          int noise_mode, int continuous,
                          size_t code_only_epochs, double doppler_rate,
                          const char **why)
{
  if (!code || code_len < 1 || n_period < code_len)
    return NULL; /* the dispatcher refuses an empty ref first */
  /* The period's bytes fit a size_t: search_period_at refused one whose
     do not, where the period is made (#2958). */
  float _Complex *replica
      = (float _Complex *)dp_xmallocn (n_period, sizeof *replica);
  for (size_t k = 0; k < n_period; k++)
    {
      const size_t c
          = (size_t)(((double)k + 0.5) * (double)code_len / (double)n_period);
      replica[k] = (code[c < code_len ? c : code_len - 1] & 1u) ? -1.0f : 1.0f;
    }
  dp_correlator_shape_t shape;
  if (n_period % code_len == 0)
    shape = search_shape_of_chips (n_period / code_len);
  else if (search_shape_of_template (replica, n_period, &shape))
    {
      free (replica);
      return NULL; /* the period's transform could not be built */
    }
  dp_correlator_state_t *st = search_acq_create_impl (
      replica, code_len, &shape, reps, n_period, chip_rate, symbol_rate,
      cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode, continuous,
      code_only_epochs, doppler_rate, /* rate_hypotheses= */ 1, why);
  free (replica);
  return st;
}

size_t
dp_correlator_period_samples (size_t code_len)
{
  size_t four;
  if (code_len == 0 || !dp_size_mul (code_len, 4, &four))
    return 0;
  return dp_next_pow_two (2 * code_len);
}

/* The samples one period of a code_len-chip code spans at sample_rate, or
   0 when that is not a whole number (to a part in 1e9) of at least
   code_len: the search's transforms are that long, and a fractional period
   has no grid to correlate on. */
static size_t
search_period_at (size_t code_len, double sample_rate, double chip_rate,
                  int *too_long)
{
  if (code_len < 1 || !(sample_rate > 0.0) || !isfinite (sample_rate)
      || !(chip_rate > 0.0) || !isfinite (chip_rate))
    return 0;
  const double n = sample_rate * (double)code_len / chip_rate;
  const double r = nearbyint (n);
  if (!(r >= (double)code_len) || fabs (n - r) > 1e-9 * r)
    return 0;
  /* Every caller sizes a period of complex samples from it (the replica,
     the template's transforms), and a continuous search pads its lags to
     the next power of two, up to twice it: a period whose bytes, or whose
     padding, no size_t holds is refused here, where it is made (#2958).
     sample_rate 7.438e22 over a 31-chip code at 1 MHz is ~2^61 samples,
     2^64 bytes. */
  size_t period, bytes;
  if (!dp_size_from_double (r, &period) || !dp_size_mul (period, 2, &bytes)
      || !dp_size_mul (period, sizeof (float _Complex), &bytes))
    {
      if (too_long)
        *too_long = 1;
      return 0;
    }
  return period;
}

size_t
dp_correlator_search_period (size_t code_len, double sample_rate,
                             double chip_rate)
{
  const size_t own = search_period_at (code_len, sample_rate, chip_rate, NULL);
  if (own != 0)
    return own;
  return (sample_rate > 0.0 && isfinite (sample_rate) && chip_rate > 0.0
          && isfinite (chip_rate))
             ? dp_correlator_period_samples (code_len)
             : 0;
}

static dp_correlator_state_t *
search_continuous_create (const uint8_t *code, size_t code_len,
                          double sample_rate, double chip_rate,
                          double symbol_rate, double cn0_dbhz,
                          double doppler_uncertainty, double pfa, double pd,
                          int noise_mode, size_t code_only_epochs,
                          double doppler_rate, const char **why)
{
  /* Each rate on its own first, so the period sentence below is only ever
     about their ratio. */
  if (!(sample_rate > 0.0 && isfinite (sample_rate)))
    CORR_WHY ("Correlator: sample_rate must be finite and positive");
  if (!(chip_rate > 0.0 && isfinite (chip_rate)))
    CORR_WHY ("Correlator: chip_rate must be finite and positive");
  int          too_long = 0;
  const size_t n_period
      = search_period_at (code_len, sample_rate, chip_rate, &too_long);
  if (too_long)
    CORR_WHY ("Correlator: sample_rate asks for a code period no search "
              "grid can hold");
  if (n_period == 0)
    CORR_WHY ("Correlator: the continuous search needs a code period, "
              "sample_rate * len(ref) / chip_rate, that is a whole number "
              "of samples, at least len(ref)");
  return search_create_from_chips (
      code, code_len, /* reps= */ 1, n_period, chip_rate, symbol_rate,
      cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode,
      /* continuous= */ 1, code_only_epochs, doppler_rate, why);
}

dp_correlator_state_t *
dp_correlator_create_continuous (const uint8_t *code, size_t code_len,
                                 double sample_rate, double chip_rate,
                                 double symbol_rate, double cn0_dbhz,
                                 double doppler_uncertainty, double pfa,
                                 double pd, int noise_mode,
                                 size_t code_only_epochs, double doppler_rate)
{
  return search_continuous_create (code, code_len, sample_rate, chip_rate,
                                   symbol_rate, cn0_dbhz, doppler_uncertainty,
                                   pfa, pd, noise_mode, code_only_epochs,
                                   doppler_rate, NULL);
}

static dp_correlator_state_t *
search_burst_create (const float _Complex *tmpl, size_t n, size_t reps,
                     double fs, double cn0_dbhz, double doppler_uncertainty,
                     double pfa, double pd, int noise_mode,
                     double doppler_rate, size_t rate_hypotheses,
                     const char **why)
{
  if (!tmpl || n < 1)
    return NULL; /* the dispatcher refuses an empty ref first */
  /* Unit RMS, the scale a code's +/-1 reference has: `peak_mag` and
     `noise_est` are then in the signal's units whatever the template's
     amplitude. (The thresholds, test_stat and the C/N0 estimate are CFAR
     ratios and would not move either way.) A template with no energy, or a
     non-finite sample, has no shape to search for. */
  double e = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      double re = (double)crealf (tmpl[i]), im = (double)cimagf (tmpl[i]);
      e += re * re + im * im;
    }
  /* One test covers all three: no energy fails `e > 0`, a NaN sample makes
     the sum NaN (which fails it too), an infinite one makes it infinite. */
  if (!(e > 0.0) || !isfinite (e))
    CORR_WHY ("Correlator: the burst search's ref needs finite, non-zero "
              "energy");
  const float     g       = (float)sqrt ((double)n / e);
  float _Complex *replica = dp_xmallocn (n, sizeof *replica);
  for (size_t i = 0; i < n; i++)
    replica[i] = tmpl[i] * g;

  dp_correlator_shape_t shape;
  if (search_shape_of_template (replica, n, &shape))
    {
      free (replica);
      return NULL; /* the template's transform could not be built */
    }
  /* One chip = one sample: sf = n, a period of n samples, chip_rate = fs. */
  dp_correlator_state_t *st = search_acq_create_impl (
      replica, n, &shape, reps, /* n_period= */ n, fs, /* symbol_rate= */ 0.0,
      cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode,
      /* continuous= */ 0, 1, doppler_rate, rate_hypotheses, why);
  free (replica);
  return st;
}

dp_correlator_state_t *
dp_correlator_create_burst (const float _Complex *tmpl, size_t n, size_t reps,
                            double fs, double cn0_dbhz,
                            double doppler_uncertainty, double pfa, double pd,
                            int noise_mode, double doppler_rate,
                            size_t rate_hypotheses)
{
  return search_burst_create (tmpl, n, reps, fs, cn0_dbhz, doppler_uncertainty,
                              pfa, pd, noise_mode, doppler_rate,
                              rate_hypotheses, NULL);
}

/* ── One constructor (docs/design/correlator-python.md sections 3, 17) ──── */

dp_correlator_flavor_t
dp_correlator_flavor (const dp_correlator_state_t *state)
{
  return state->flavor;
}

/* The continuous search's reference as the chips its constructor takes:
   +1 is chip 0 and -1 chip 1 (dp_bin_to_nrz's mapping), and anything else
   is not a chip.  Returns the chips (the caller frees), or NULL when a
   sample is not exactly +1 or -1.  The copy is an eighth of the reference
   the caller already holds, so only a genuine OOM fails it: that aborts
   (clib_common.h). */
static uint8_t *
corr_nrz_chips (const float _Complex *ref, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      const float re = crealf (ref[i]), im = cimagf (ref[i]);
      if (im != 0.0f || (re != 1.0f && re != -1.0f))
        return NULL;
    }
  uint8_t *chips = dp_xmalloc (n);
  for (size_t i = 0; i < n; i++)
    chips[i] = crealf (ref[i]) < 0.0f;
  return chips;
}

dp_correlator_state_t *
dp_correlator_create (const float _Complex *ref, size_t ny, size_t nx,
                      dp_correlator_framing_t framing, size_t doppler_rows,
                      int detect, size_t dwell, size_t ny_out, size_t nx_out,
                      int col_out, float threshold, size_t noise_lo,
                      size_t noise_hi, int noise_mode, double sample_rate,
                      double chip_rate, double symbol_rate, size_t reps,
                      double cn0_dbhz, double doppler_uncertainty, double pfa,
                      double pd, double doppler_rate, size_t rate_hypotheses,
                      size_t code_only_epochs, int nthreads, const char **why)
{
  dp_correlator_state_t *s;
  if (why)
    *why = NULL;
  if (framing != DP_CORRELATOR_FRAMING_NONE
      && framing != DP_CORRELATOR_FRAMING_CONTINUOUS
      && framing != DP_CORRELATOR_FRAMING_BURST)
    CORR_WHY ("Correlator: framing must be 'none', 'continuous' or "
              "'burst'");
  if (!ref || ny == 0 || nx == 0)
    CORR_WHY ("Correlator: ref is empty");
  /* The detectors and the searches take it as given: here is the one
     place an int becomes a mode. */
  if (noise_mode < DET_NOISE_MEAN || noise_mode > DET_NOISE_MAX)
    CORR_WHY ("Correlator: noise_mode must be 'mean', 'median', 'min' "
              "or 'max'");

  /* Each refusal below names ONE knob: the first that does not hold
     its default on a flavor that does not read it. */
#define CORR_SEARCH_ONLY(cond, knob)                                          \
  if (cond)                                                                   \
  CORR_WHY ("Correlator: " knob " is a search's; it needs "                   \
            "framing='continuous' or 'burst'")
#define CORR_CONT_ONLY(cond, knob)                                            \
  if (cond)                                                                   \
  CORR_WHY ("Correlator: " knob " is the continuous search's; it needs "      \
            "framing='continuous'")
#define CORR_BURST_ONLY(cond, knob)                                           \
  if (cond)                                                                   \
  CORR_WHY ("Correlator: " knob " is the burst search's; it needs "           \
            "framing='burst'")
#define CORR_DETECTOR_ONLY(cond, knob)                                        \
  if (cond)                                                                   \
  CORR_WHY ("Correlator: " knob " is a detector's; it needs detect=True")
#define CORR_MAP_ONLY(cond, knob, flavor)                                     \
  if (cond)                                                                   \
  CORR_WHY ("Correlator: " knob " shapes a map; " flavor)

  if (framing == DP_CORRELATOR_FRAMING_NONE)
    {
      /* A search's knobs, which no map or detector reads. */
      CORR_SEARCH_ONLY (sample_rate != 0.0, "sample_rate");
      CORR_SEARCH_ONLY (!isnan (cn0_dbhz), "cn0_dbhz");
      CORR_SEARCH_ONLY (doppler_uncertainty != 0.0, "doppler_uncertainty");
      CORR_SEARCH_ONLY (pfa != 1e-3, "pfa");
      CORR_SEARCH_ONLY (pd != 0.9, "pd");
      CORR_SEARCH_ONLY (doppler_rate != 0.0, "doppler_rate");
      CORR_CONT_ONLY (chip_rate != 0.0, "chip_rate");
      CORR_CONT_ONLY (symbol_rate != 0.0, "symbol_rate");
      CORR_CONT_ONLY (code_only_epochs != 1, "code_only_epochs");
      CORR_BURST_ONLY (reps != 1, "reps");
      CORR_BURST_ONLY (rate_hypotheses != 1, "rate_hypotheses");

      if (doppler_rows > 0)
        {
          if (detect)
            CORR_WHY ("Correlator: doppler_rows builds a delay-Doppler "
                      "map, which does not detect (detect=False)");
          if (ny != 1)
            CORR_WHY ("Correlator: doppler_rows correlates a 1-D code, "
                      "and ref has more than one row");
          CORR_MAP_ONLY (nx_out != 0, "nx_out",
                         "the delay-Doppler map's delay axis is native");
          CORR_MAP_ONLY (col_out != -1, "col_out",
                         "the delay-Doppler map's delay axis is native");
        }
      else if (detect)
        {
          CORR_MAP_ONLY (ny_out != 0, "ny_out",
                         "a detector reports hits (detect=True)");
          CORR_MAP_ONLY (nx_out != 0, "nx_out",
                         "a detector reports hits (detect=True)");
          CORR_MAP_ONLY (col_out != -1, "col_out",
                         "a detector reports hits (detect=True)");
          s = corr_detector_create (ref, ny, nx, dwell, noise_lo, noise_hi,
                                    (det_noise_mode_t)noise_mode, threshold,
                                    nthreads, why);
          if (s && ny == 1)
            s->flavor = DP_CORRELATOR_DETECTOR_1D;
          return s;
        }

      /* A map: the detector's knobs are not read. */
      CORR_DETECTOR_ONLY (threshold != 0.0f, "threshold");
      CORR_DETECTOR_ONLY (noise_lo != 0, "noise_lo");
      CORR_DETECTOR_ONLY (noise_hi != SIZE_MAX, "noise_hi");
      CORR_DETECTOR_ONLY (noise_mode != DET_NOISE_MEAN, "noise_mode");
      if (doppler_rows > 0)
        return corr_dd_create (ref, nx, doppler_rows, dwell, nthreads, ny_out,
                               why);
      if (ny == 1)
        {
          /* The 1-D flavor is the 2-D one at one row: nx_out pads it. */
          if (ny_out != 0)
            CORR_WHY ("Correlator: ny_out pads the rows of a 2-D ref, "
                      "and ref has one row");
          if (col_out != -1)
            CORR_WHY ("Correlator: col_out emits one lag of each row "
                      "of a 2-D ref, and ref has one row");
          return corr1d_create (ref, nx, dwell, nthreads, nx_out, why);
        }
      return corr2d_create (ref, ny, nx, dwell, nthreads, ny_out, nx_out,
                            col_out, why);
    }

  /* A search: the map's and the detector's knobs are not read. */
  if (doppler_rows != 0)
    CORR_WHY ("Correlator: a search sizes its own Doppler grid; "
              "doppler_rows needs framing='none'");
  CORR_MAP_ONLY (dwell != 1, "dwell", "a search sizes its own dwell");
  CORR_MAP_ONLY (ny_out != 0, "ny_out", "a search reports hits");
  CORR_MAP_ONLY (nx_out != 0, "nx_out", "a search reports hits");
  CORR_MAP_ONLY (col_out != -1, "col_out", "a search reports hits");
  /* A search does take a thread count, through set_threads(), and sizes
     its own pool at create (every core across its tiles): nthreads' 1
     here would be a different request, so it is not passed through. */
  if (nthreads != 1)
    CORR_WHY ("Correlator: a search sizes its own thread pool at "
              "create; nthreads applies to framing='none', and a "
              "search's count is set after create with set_threads()");
  if (threshold != 0.0f)
    CORR_WHY ("Correlator: a search sets its threshold from pfa; "
              "threshold needs framing='none' and detect=True");
  if (noise_lo != 0)
    CORR_WHY ("Correlator: a search estimates its own noise; noise_lo "
              "needs framing='none' and detect=True");
  if (noise_hi != SIZE_MAX)
    CORR_WHY ("Correlator: a search estimates its own noise; noise_hi "
              "needs framing='none' and detect=True");
  if (ny != 1)
    CORR_WHY ("Correlator: a search correlates a 1-D ref, and ref has "
              "more than one row");
  if (!(sample_rate > 0.0))
    CORR_WHY ("Correlator: a search needs sample_rate; 0 is not given "
              "(sample_rate=1.0 for normalized units)");
  if (!isfinite (sample_rate))
    CORR_WHY ("Correlator: sample_rate must be finite and positive");

  if (framing == DP_CORRELATOR_FRAMING_CONTINUOUS)
    {
      CORR_BURST_ONLY (reps != 1, "reps");
      CORR_BURST_ONLY (rate_hypotheses != 1, "rate_hypotheses");
      if (!(chip_rate > 0.0))
        CORR_WHY ("Correlator: the continuous search needs chip_rate; 0 "
                  "is not given");
      if (!isfinite (chip_rate))
        CORR_WHY ("Correlator: chip_rate must be finite and positive");
      if (!isfinite (cn0_dbhz))
        CORR_WHY ("Correlator: the continuous search sizes its dwell "
                  "from a design point; it needs a finite cn0_dbhz");
      uint8_t *chips = corr_nrz_chips (ref, nx);
      if (!chips)
        CORR_WHY ("Correlator: the continuous search's ref is NRZ "
                  "chips, each exactly +1 or -1");
      s = search_continuous_create (chips, nx, sample_rate, chip_rate,
                                    symbol_rate, cn0_dbhz, doppler_uncertainty,
                                    pfa, pd, noise_mode, code_only_epochs,
                                    doppler_rate, why);
      free (chips);
      return s;
    }

  CORR_CONT_ONLY (chip_rate != 0.0, "chip_rate");
  CORR_CONT_ONLY (symbol_rate != 0.0, "symbol_rate");
  CORR_CONT_ONLY (code_only_epochs != 1, "code_only_epochs");
#undef CORR_SEARCH_ONLY
#undef CORR_CONT_ONLY
#undef CORR_BURST_ONLY
#undef CORR_DETECTOR_ONLY
#undef CORR_MAP_ONLY
  return search_burst_create (ref, nx, reps, sample_rate, cn0_dbhz,
                              doppler_uncertainty, pfa, pd, noise_mode,
                              doppler_rate, rate_hypotheses, why);
}

#undef CORR_WHY

/* The refusal sentences below state these limits as numbers: a change to
   either macro fails the build here rather than leaving a sentence stale. */
_Static_assert (DP_CORRELATOR_N_NONCOH_SAFETY_CEILING == 256u,
                "configure_search_raw's sentence states 1..256: update it");
_Static_assert (DP_CORRELATOR_MAX_PEAKS == 64u,
                "set_max_peaks' sentence states 1..64: update it");

int
dp_correlator_configure_search_raw (dp_correlator_state_t *st,
                                    size_t doppler_bins, size_t n_noncoh,
                                    const char **why)
{
  dp_correlator_search_t *const srch = st->search;
  if (!srch) /* not a search flavor (§18) */
    return DP_ERR_UNSUPPORTED;
  if (doppler_bins < 1 || doppler_bins > srch->reps)
    {
      if (why)
        *why = "configure_search_raw: doppler_bins must lie in 1..reps";
      return DP_ERR_INVALID;
    }
  if (n_noncoh < 1 || n_noncoh > DP_CORRELATOR_N_NONCOH_SAFETY_CEILING)
    {
      if (why)
        *why = "configure_search_raw: n_noncoh must lie in 1..256 "
               "(DP_CORRELATOR_N_NONCOH_SAFETY_CEILING)";
      return DP_ERR_INVALID;
    }
  /* Non-coherent looks and rate hypotheses do not compose: the search scores
     each hypothesis with the COHERENT statistic, against a gate the
     non-coherent path sets to zero, and sums per-look maxima into a surface
     eta_nc was never derived for. A burst engine buys no looks on its own
     (search_auto_config_burst), so this refuses only a pinned grid. */
  if (n_noncoh > 1 && srch->rate_hyps > 1)
    {
      if (why)
        *why = "configure_search_raw: non-coherent looks (n_noncoh > 1) need "
               "rate_hypotheses 1";
      return DP_ERR_INVALID;
    }

  /* This escape hatch always pins the NATIVE (doppler_bins, n_noncoh) grid
   * -- it exits wideband mode (window_bins back to 1) if that was active,
   * matching its documented contract of pinning the classic grid directly. */
  if (dp_correlator_regrid (st, doppler_bins, n_noncoh, 1, NULL,
                            srch->carrier_freq_hz)
      != 0)
    return DP_ERR_MEMORY;

  const double snr = search_design_snr (srch->cn0_dbhz, srch->fs);
  search_commit_thresholds (st, srch->pfa, srch->pd, snr,
                            srch->doppler_uncertainty);
  dp__search_reset (st);
  return 0;
}

double
dp_correlator_rate_hypothesis (double span, size_t n, size_t h)
{
  return search_rate_of (span, n, h);
}

double _Complex dp_correlator_rate_phasor (double rate, double t)
{
  return cexp (-I * M_PI * rate * t * t);
}

size_t
dp_correlator_rate_hypotheses_for (double f_epoch, double span, size_t depth)
{
  /* NaN or infinity would reach the size_t conversion below: UB. */
  if (!(span > 0.0) || !isfinite (span) || !(f_epoch > 0.0)
      || !isfinite (f_epoch) || depth < 1)
    return 1;
  /* The residual is span / (n - 1): the smallest n leaving it under what a
     block of `depth` epochs tolerates (search_drift_rate_limit). */
  double need = 1.0 + ceil (span / search_drift_rate_limit (f_epoch, depth));
  if (need > (double)DP_CORRELATOR_RATE_HYPS_MAX)
    need = (double)DP_CORRELATOR_RATE_HYPS_MAX;
  return (size_t)need;
}

void
dp__search_destroy (dp_correlator_state_t *st)
{
  if (!st)
    return;
  search_map_t map
      = { st->dd_map,           st->search->ref_spec,  st->search->map_spec,
          st->search->map_work, st->search->map_tiles, st->search->map_rows,
          st->search->map_off,  st->search->map_wt,    st->search->map_f };
  search_map_release (&map);
  free (st->search->prompt_scratch);
  dp__dd_map_destroy (st->search->sl_map);
  dp__dd_map_destroy (st->search->sl_map_cb);
  free (st->search->sl_scratch);
  free (st->search->sl_cmax);
  if (st->ring)
    dp_f32_destroy (st->ring);
  free (st->search->ref);
  free (st->search->blk_raw);
  free (st->search->out_buf);
  free (st->search->rate_chirp);
  free (st->search->rate_best);
  free (st->mag_buf);
  free (st->noise_scratch);
  free (st->search->nc_surface);
  free (st->band_mask);
  free (st->peak_mask);
  free (st->peaks);
  free (st->twin_row);
  free (st->twin_col);
  free (st->search->stat_surface);
  dp_pool_destroy (st->pool);
  free (st->search->parts);
  free (st);
}

int
dp_correlator_set_threads (dp_correlator_state_t *st, int n)
{
  dp_correlator_search_t *const srch = st->search;
  if (!srch) /* not a search flavor (§18) */
    return DP_ERR_UNSUPPORTED;
  srch->threads_req = n;
  search_apply_threads (st);
  return DP_OK;
}

int
dp_correlator_set_max_peaks (dp_correlator_state_t *st, size_t n,
                             const char **why)
{
  /* Only a search lists peaks, and only a search's destroy frees the list:
     off one this would leak it (§18). */
  if (!st->search)
    return DP_ERR_UNSUPPORTED;
  if (n < 1 || n > DP_CORRELATOR_MAX_PEAKS)
    {
      if (why)
        *why = "set_max_peaks: n must lie in 1..64 (DP_CORRELATOR_MAX_PEAKS)";
      return DP_ERR_INVALID;
    }
  /* Fixed sizes from a validated argument: abort-on-OOM helpers, so there
     is no unwind path nothing can reach. */
  free (st->peaks);
  free (st->twin_row);
  free (st->twin_col);
  st->peaks     = (det_peak_t *)dp_xmallocn (n, sizeof (det_peak_t));
  st->twin_row  = (uint32_t *)dp_xmallocn (n, sizeof (uint32_t));
  st->twin_col  = (uint32_t *)dp_xmallocn (n, sizeof (uint32_t));
  st->max_peaks = n;
  st->n_peaks   = 0;
  st->n_twins   = 0;
  return 0;
}

void
dp__search_reset (dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  /* The ONE place the framer is bound: a regrid can change frame_n while
     keeping a ring already big enough for it (dp_correlator_regrid), so the
     framer is bound here, at the current frame_n, after the ring is emptied --
     never inside the regrid. It cannot refuse: the regrid sized the ring for
     frame_n and the ring was just emptied, which is all init checks. */
  dp_f32_reset (st->ring);
  const int bound = dp_f32_framer_init (&st->framer, st->ring, srch->frame_n,
                                        srch->frame_n);
  (void)bound;
  st->consumed = 0;
  if (srch->nc_surface)
    memset (srch->nc_surface, 0, srch->n_surf * sizeof (float));
  srch->nc_count         = 0;
  srch->blk_epoch        = 0;
  srch->samples_consumed = 0;
  st->n_peaks            = 0;
  st->n_twins            = 0;
  st->peak_row = st->peak_col = 0;
  st->peak_mag = st->noise_est = st->test_stat = 0.0f;
  srch->n_held                                 = 0;
  srch->peak_conc                              = 0.0f;
  srch->surface_at                             = 0;
  srch->dwells                                 = 0;
  srch->rate_best_h                            = 0;
}

/* ── Stream push ────────────────────────────────────────────────────────── */

/* out[k] = x[k] * e^{j w k'} for k' = k, or the signed bin (k - n past n/2,
   `signed_bins`, numpy's fftfreq: an odd n's middle bin stays positive):
   a phasor recurrence resynced from an exact cexp() every 256 steps and
   where the signed index jumps. In place is allowed (x == out). The one
   ramp: dp_correlator_block_prompt()'s exact code-rate ramp and the per-cell
   rule-2 model both run it; a private copy in the tile path mis-signed an odd
   period's middle bin (#2815). One cexp() a sample made the rule-2 model a
   third of a pool's run under libm alone. */
static void
search_ramp (const float _Complex *x, size_t n, double w, int signed_bins,
             float _Complex *out)
{
  double _Complex ph = 1.0, step = cexp (I * w);
  for (size_t k = 0; k < n; k++)
    {
      /* The signed index jumps where dp_fftfreq_index turns negative. */
      const int jump = signed_bins && k == (n + 1) / 2;
      if ((k & 255) == 0 || jump)
        {
          const double kk
              = signed_bins ? (double)dp_fftfreq_index (k, n) : (double)k;
          ph = cexp (I * w * kk);
        }
      out[k] = x[k] * (float _Complex)ph;
      ph *= step;
    }
}

/* The slow-time de-chirp phasors, one row of `coherent_bins` per rate
 * hypothesis: exp(-j pi a_h t_i^2), t_i the epoch's time from the block's
 * MIDDLE (so the Doppler an engine reports is the carrier's there). Built
 * lazily and rebuilt when the depth moves, because a regrid can change
 * coherent_bins under a table built for the old one. An epoch is
 * sf / chip_rate seconds on every engine (one chip = one sample on a burst
 * engine, so sf = the preamble length and chip_rate = fs). */
static void
search_rate_table (dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  D = srch->coherent_bins, R = srch->rate_hyps;
  if (search_rate_count (st) < 2
      || (srch->rate_chirp && srch->rate_chirp_d == D))
    return;
  free (srch->rate_chirp);
  srch->rate_chirp
      = dp_xmallocn (dp_size_mul_sat (R, D), sizeof *srch->rate_chirp);
  srch->rate_chirp_d = D;
  const double T     = (double)srch->sf / srch->chip_rate;
  for (size_t h = 0; h < R; h++)
    {
      const double a = search_rate_of (srch->doppler_rate, R, h);
      for (size_t i = 0; i < D; i++)
        {
          const double t = ((double)i - 0.5 * (double)(D - 1)) * T;
          srch->rate_chirp[h * D + i]
              = (float _Complex)dp_correlator_rate_phasor (a, t);
        }
    }
}

/* The rate search as the map's fold (dp__dd_map_run hands it each
 * hypothesis' surface in out_buf, in order): each is tested against its OWN
 * surface (a CFAR reference taken over a max of hypotheses would be biased
 * up, and the gate is derived for one), the strongest statistic wins, and
 * its surface is kept in a spare buffer (a copy, not another pass) unless it
 * was the last one made. The per-cell Pfa was split over the hypotheses at
 * sizing (search_commit_thresholds), so the strongest is held to the one gate.
 * Without hypotheses there is nothing to choose and the fold does nothing. */
typedef struct
{
  dp_correlator_state_t *st;
  size_t                 R;
  size_t                 best_h;
  float                  best;
} search_rates_t;

static void
search_rate_fold (void *ctx, size_t h, const float _Complex *out)
{
  search_rates_t               *c    = (search_rates_t *)ctx;
  dp_correlator_state_t        *st   = c->st;
  dp_correlator_search_t *const srch = st->search;
  (void)out; /* out_buf itself */
  if (c->R < 2)
    return;
  search_compute_stat (st);
  if (search_rate_better (h, st->test_stat, c->best))
    {
      c->best   = st->test_stat;
      c->best_h = h;
      if (h != c->R - 1)
        memcpy (srch->rate_best, srch->out_buf,
                srch->n_surf * sizeof *srch->rate_best);
    }
}

/* The block's decided surface, into out_buf, from its epoch spectra
 * (map_spec): the one delay-Doppler map (docs/design/acq-surface.md §18) in
 * the schedule its rule picks, every tile and every rate hypothesis, at the
 * one scale (1/code_bins). The hypotheses only weight the slow-time axis,
 * so with none (rate_hyps == 1) this is the plain map. Returns the cells
 * the surface holds. */
static size_t
search_make_surface (dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  R    = search_rate_count (st);
  if (R > 1 && srch->rate_best_cap != srch->n_surf)
    {
      free (srch->rate_best);
      srch->rate_best = dp_xmallocn (srch->n_surf, sizeof *srch->rate_best);
      srch->rate_best_cap = srch->n_surf;
    }
  search_rates_t c = { st, R, 0, -1.0f };
  (void)dp__dd_map_run (st->dd_map, search_block_pool (st), -1, srch->map_spec,
                        R > 1 ? srch->rate_chirp : NULL, R, srch->map_tiles,
                        srch->window_bins > 1 ? srch->window_bins : 1,
                        srch->map_work, srch->out_buf, search_rate_fold, &c);
  if (R > 1)
    {
      if (c.best_h != R - 1)
        memcpy (srch->out_buf, srch->rate_best,
                srch->n_surf * sizeof *srch->rate_best);
      srch->rate_best_h = c.best_h;
    }
  return srch->n_surf;
}

/* Would the NEXT frame end a dwell -- report a list? The twin of
   dp__search_push's frame loop, below, and it must stay one: the correlator
   dumps on every frame (its dwell is 1, dp_correlator_regrid) except mid-block
   in wideband mode, where a block of D = coherent_bins epochs dumps on its
   D-th; and a dump decides unless non-coherent looks are still
   accumulating. test_correlator_search's resume checks go red if either clause
   is wrong. */
static int
search_next_frame_decides (const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  const int dumps = srch->window_bins <= 1 || srch->coherent_bins <= 1
                    || srch->blk_epoch + 1 >= srch->coherent_bins;
  return dumps
         && (srch->n_noncoh <= 1 || srch->nc_count + 1 >= srch->n_noncoh);
}

size_t
dp__search_push (dp_correlator_state_t *st, const float _Complex *x,
                 size_t n_in, dp_correlator_hit_t *result, size_t max_results)
{
  dp_correlator_search_t *const srch     = st->search;
  size_t                        ndet     = 0;
  size_t                        off      = 0;
  const size_t                  frame_n  = srch->frame_n;
  const size_t                  nx       = srch->code_bins;
  const size_t                  wideband = srch->window_bins > 1;
  search_rate_table (st); /* no-op without rate hypotheses */

  /* The room a frame that ENDS A DWELL needs: its whole list (max_peaks
     results). A caller's buffer smaller than a list can never offer that,
     so then it is the whole buffer -- the dwell is cut to its strongest
     picks, which the header states -- and never less than one slot, so a
     push with no room decides nothing. */
  const size_t need = st->max_peaks < max_results ? st->max_peaks
                      : max_results               ? max_results
                                                  : 1;

  /* ONE frame at a time, never a batch: whether the next frame ends a dwell
     depends on where the dwell stands, which only the frame before it
     settles, and a dump can report several results -- the detectors'
     batched feed (correlator_private.h) would overfill result here. While
     slots are left, a frame that ends no dwell needs none and is taken; one
     that does is taken only while `need` slots are left. A FULL result takes
     nothing more -- the detectors' rule (#2018), and the contract every
     room-limited caller relies on: a push at room 1 frames nothing past
     its hit. At the first frame it does not take, the push takes the rest
     of its input only if the rest completes no frame (it is then the
     carry) and otherwise stops on the frame boundary: the rest is the
     caller's, at x + dp__search_consumed(). So consumed() is where the hit's
     frame ended unless a carry was taken after it; a hand-off reads the
     hit's own samples_consumed (dp_correlator_position()). */
  for (;;)
    {
      const size_t rest = n_in - off;
      if (ndet == max_results
          || (search_next_frame_decides (st) && max_results - ndet < need))
        {
          off += dp_f32_framer_feed_carry_view (&st->framer, x + off, rest);
          break;
        }
      /* Only what completes THIS frame, never a part of the next: whether
         that one fits is not known until this one is done. The rules are
         the framer's (feed_frames, feed_carry), shared with the detectors'
         drain. */
      off += dp_f32_framer_feed_frames_view (&st->framer, x + off, rest, 1);
      /* Zero-copy: a pointer into the ring, contiguous across its wrap,
         valid until the next framer call. */
      const float _Complex *frame = dp_f32_framer_next_view (&st->framer);
      if (!frame)
        break; /* the input is used up: the rest of a frame is the carry */

      size_t n_out;
      if (wideband && srch->coherent_bins > 1)
        {
          /* A tiled block: its epochs are held raw until its end, where
             the one map runs on them, every stage once a block
             (docs/design/acq-surface.md §18.2). Row r's reported index
             uses the native FFT-bin convention on the combined axis
             (search_block_row), so doppler_bin and doppler_res_hz compose
             as on the narrow axis. */
          memcpy (srch->blk_raw + srch->blk_epoch * nx, frame,
                  nx * sizeof *frame);
          if (++srch->blk_epoch < srch->coherent_bins)
            n_out = 0; /* mid-block: no surface yet */
          else
            {
              srch->blk_epoch = 0;
              dp__dd_map_forward (st->dd_map, search_block_pool (st), -1,
                                  srch->blk_raw, srch->map_spec);
              n_out = search_make_surface (st);
            }
        }
      else
        {
          /* One frame is a whole block: one epoch on a tiled engine at
             D = 1, the D epochs of a narrow one. */
          dp__dd_map_forward (st->dd_map, search_block_pool (st), -1, frame,
                              srch->map_spec);
          n_out = search_make_surface (st);
        }
      srch->samples_consumed += frame_n;

      if (n_out == 0)
        continue; /* mid-dwell — coherent accumulation, no dump yet */

      if (srch->n_noncoh <= 1)
        {
          /* Coherent path: amplitude mean-CFAR per dump; the list of
             every peak above the gate, one result each. */
          search_compute_stat (st);
          const int hit = st->test_stat > dp_correlator_gate_stat (st);
          if (hit)
            ndet += search_report_peaks (st, st->mag_buf, search_stat_coherent,
                                         search_mag_identity, result + ndet,
                                         max_results - ndet);
          search_dwell_decided (st, st->mag_buf, search_stat_coherent, hit);
          continue;
        }

      /* Non-coherent path: magnitude-square accumulate each coherent look
       * (per chunk, on the pool); gate the order-N_nc statistic once
       * n_noncoh looks are in. */
      search_scan_t acc = { st, NULL, 0, 0 };
      dp_pool_run (search_epoch_pool (st), srch->window_bins,
                   search_tile_nc_acc, &acc);
      if (++srch->nc_count < srch->n_noncoh)
        continue; /* still accumulating looks */

      search_compute_stat_nc (st);
      const int hit = st->test_stat > dp_correlator_gate_stat (st);
      if (hit)
        ndet += search_report_peaks (st, srch->nc_surface,
                                     search_stat_noncoherent, search_mag_sqrt,
                                     result + ndet, max_results - ndet);
      search_dwell_decided (st, srch->nc_surface, search_stat_noncoherent,
                            hit);
      memset (srch->nc_surface, 0, srch->n_surf * sizeof (float));
      srch->nc_count = 0;
    }

  st->consumed = off;
  return ndet;
}

size_t
dp__search_consumed (const dp_correlator_state_t *state)
{
  return state->consumed;
}

uint64_t
dp_correlator_position (const dp_correlator_state_t *state)
{
  dp_correlator_search_t *const srch = state->search;
  if (!srch) /* not a search flavor (§18) */
    return 0;
  return srch->samples_consumed + dp_f32_framer_pending (&state->framer);
}

int
dp_correlator_set_carrier_freq_hz (dp_correlator_state_t *state,
                                   double                 carrier_freq_hz)
{
  dp_correlator_search_t *const srch = state->search;
  if (!corr2d_is_continuous (state)) /* §18 */
    return DP_ERR_UNSUPPORTED;
  if (!dp_correlator_carrier_freq_ok (carrier_freq_hz, srch->fs))
    return DP_ERR_INVALID;
  if (carrier_freq_hz == srch->carrier_freq_hz)
    return DP_OK;
  /* The map's fine grid and every tile's ramp follow the carrier, so it is
     set through the regrid, which rebuilds only those (and re-applies the
     requested threads, #2797). It is applied at the block's end, so a
     change mid-block ramps the whole block with the new carrier. */
  return dp_correlator_regrid (state, srch->coherent_bins, srch->n_noncoh,
                               srch->window_bins, NULL, carrier_freq_hz)
                 == 0
             ? DP_OK
             : DP_ERR_MEMORY;
}

int
dp_correlator_carrier_freq_ok (double carrier_freq_hz, double sample_rate_hz)
{
  return carrier_freq_hz == 0.0
         || (isfinite (carrier_freq_hz)
             && carrier_freq_hz > 0.5 * sample_rate_hz);
}

double
dp_correlator_psl_db (const dp_correlator_state_t *state)
{
  dp_correlator_search_t *const srch = state->search;
  if (!srch) /* not a search flavor (§18) */
    return NAN;
  /* log10(0) is -HUGE_VAL, but say it: a perfect sequence has no sidelobe
     to be below, and -inf is that fact rather than a number to compare. */
  return srch->shape.psl > 0.0 ? 20.0 * log10 (srch->shape.psl) : -INFINITY;
}

double
dp_correlator_gate_stat (const dp_correlator_state_t *state)
{
  dp_correlator_search_t *const srch = state->search;
  if (!srch) /* not a search flavor (§18) */
    return NAN;
  /* The two dwell paths' gates, in the one statistic both report. */
  return srch->n_noncoh <= 1 ? (double)state->threshold : (double)srch->eta_nc;
}

double
dp_correlator_dsl_db (dp_correlator_state_t *state)
{
  dp_correlator_search_t *const srch = state->search;
  if (!srch) /* not a search flavor (§18) */
    return NAN;
  if (!srch->dsl_ready)
    {
      search_doppler_sidelobe (
          srch->ref, srch->code_bins, srch->dsl_window_bins,
          srch->doppler_uncertainty, srch->fs, &srch->shape);
      srch->dsl_ready = 1;
    }
  return srch->shape.dsl > 0.0 ? 20.0 * log10 (srch->shape.dsl) : -INFINITY;
}

/* dp_correlator_sidelobe_ratio()'s sweep points are search_core.h's: they size
   the cache it keeps on the state. */
/* A frequency touches at most two tiles' bands (an edge), and its folds add
   none past the grid's own: room for any grid. */
#define DP_CORRELATOR_SIDELOBE_TILES_MAX 4

/* The fast-time mixing frequencies (tile centres) a cell at Doppler `f` can
   have been read in: every tile of the grid whose band, widened by `tol`,
   holds f or one of its folds onto the circular combined axis (W tiles of
   width fe). A cell on a tile edge, or on the axis's wrap, belongs to both
   tiles it touches. A single-tile engine mixes at DC. Returns the count. */
static size_t
search_tiles_of (const dp_correlator_state_t *st, double f, double tol,
                 double *out)
{
  dp_correlator_search_t *const srch = st->search;
  const double                  fe   = srch->fs / (double)srch->code_bins;
  const long                    W    = (long)srch->window_bins;
  if (W <= 1)
    {
      out[0] = 0.0;
      return 1;
    }
  size_t n = 0;
  for (long k = -(W - 1) / 2; k <= W / 2; k++)
    {
      const double t = fe * (double)k;
      for (long q = -1; q <= 1; q++)
        if (fabs (f + (double)q * (double)W * fe - t) <= 0.5 * fe + tol)
          {
            out[n++] = t;
            break;
          }
    }
  return n;
}

/* The cell value a replica delayed to `refd` (time domain, code_bins) puts
   on the surface when mixed by `nu` Hz relative to the mixing tile: the
   Doppler is applied after the delay, continuous across the code's wrap as
   a channel applies it; the spectrum times conj(FFT(replica)), zero-padded
   to lag_bins and inverted. Into `out` (lag_bins), unnormalised like the
   surface's own inverse. */
static void
search_sidelobe_row (dp_correlator_state_t *st, const float _Complex *refd,
                     double nu, float _Complex *out)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  cb   = srch->code_bins;
  float _Complex               *C    = srch->sl_scratch; /* FFT(replica) */
  float _Complex               *sig  = C + cb;
  float _Complex               *spec = sig + cb;
  float _Complex               *Cc   = C + 4 * cb; /* conj(C) */
  const double                  w    = 2.0 * M_PI * nu / srch->fs;
  search_ramp (refd, cb, w, 0, sig);
  /* On the one delay-Doppler map, at one epoch: the forward, then one row
     of spec * conj(C) padded to lag_bins and inverted -- the same
     transforms, pad and product this ran on its own plans. */
  dp__dd_map_forward (srch->sl_map, NULL, 0, sig, spec);
  dp__dd_map_row1 (srch->sl_map, 0, spec, 0, Cc, out);
}

/* One slot of the cache: hit `from`'s worst ratio at every column, over the
   lag and Doppler points (dp_correlator_sidelobe_ratio()), as the rows read it
   at `tile` Hz. The same values the uncached sweep took the maximum of, so a
   lookup is exactly what it would have returned. */
static void
search_sidelobe_fill (dp_correlator_state_t     *st,
                      const dp_correlator_hit_t *from, double tile,
                      double *cmax)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  cb = srch->code_bins, lb = srch->lag_bins;
  float _Complex               *C = srch->sl_scratch;
  float _Complex *spec   = C + 2 * cb; /* scratch for the delayed replica */
  float _Complex *refd   = C + 3 * cb;
  float _Complex *ones   = C + 5 * cb;
  float _Complex *row    = C + 6 * cb;
  const double    s      = (double)cb / (double)lb; /* samples a column */
  const double    res    = srch->doppler_res_hz;
  const double    f_from = dp_correlator_bin_doppler_hz (st, from->row);
  /* its own peak on its own lag, per Doppler point: |sum_n |c[n]|^2
     e^{j w n}| in the best tile it can have been read in */
  double own[DP_CORRELATOR_SIDELOBE_DOPPLER_PTS],
      fpt[DP_CORRELATOR_SIDELOBE_DOPPLER_PTS];
  double t_from[DP_CORRELATOR_SIDELOBE_TILES_MAX];
  for (int p = 0; p < DP_CORRELATOR_SIDELOBE_DOPPLER_PTS; p++)
    {
      fpt[p] = f_from
               + res
                     * ((double)p
                            / (double)(DP_CORRELATOR_SIDELOBE_DOPPLER_PTS - 1)
                        - 0.5);
      const size_t n_from
          = search_tiles_of (st, fpt[p], 1e-9 * srch->fs, t_from);
      own[p] = 0.0;
      for (size_t a = 0; a < n_from; a++)
        {
          const double w    = 2.0 * M_PI * (fpt[p] - t_from[a]) / srch->fs;
          double _Complex z = 0.0, ph = 1.0, step = cexp (I * w);
          for (size_t k = 0; k < cb; k++)
            {
              if ((k & 255) == 0) /* resync, as search_ramp() */
                ph = cexp (I * w * (double)k);
              z += (double)(crealf (srch->ref[k]) * crealf (srch->ref[k])
                            + cimagf (srch->ref[k]) * cimagf (srch->ref[k]))
                   * ph;
              ph *= step;
            }
          own[p] = fmax (own[p], cabs (z));
        }
    }
  for (size_t c = 0; c < lb; c++)
    cmax[c] = 0.0;
  for (int q = 0; q < DP_CORRELATOR_SIDELOBE_LAG_PTS; q++)
    {
      /* the emitter's lag, samples, across its column (a delay of tau puts
         the correlation's peak at column tau / s): the replica delayed by
         tau, a phase ramp over the signed bin of its spectrum */
      const double tau
          = ((double)from->col
             + ((double)q / (double)(DP_CORRELATOR_SIDELOBE_LAG_PTS - 1)
                - 0.5))
            * s;
      const double w = -2.0 * M_PI * tau / (double)cb;
      search_ramp (C, cb, w, 1, spec);
      /* the plain inverse, as a row of spec times ones (exact) */
      dp__dd_map_row1 (srch->sl_map_cb, 0, spec, 0, ones, refd);
      for (size_t k = 0; k < cb; k++)
        refd[k] /= (float)cb; /* the unnormalised inverse */
      for (int p = 0; p < DP_CORRELATOR_SIDELOBE_DOPPLER_PTS; p++)
        {
          if (!(own[p] > 0.0))
            continue;
          search_sidelobe_row (st, refd, fpt[p] - tile, row);
          /* the unnormalised inverse is code_bins times the time-domain
             correlation own[] is (Parseval) */
          const double norm = (double)cb * own[p];
          for (size_t c = 0; c < lb; c++)
            cmax[c] = fmax (cmax[c], cabsf (row[c]) / norm);
        }
    }
}

double
dp_correlator_sidelobe_ratio (dp_correlator_state_t     *state,
                              const dp_correlator_hit_t *from,
                              const dp_correlator_hit_t *to)
{
  dp_correlator_state_t        *st   = state;
  dp_correlator_search_t *const srch = st->search;
  const size_t                  cb = srch->code_bins, lb = srch->lag_bins;
  if (!srch->sl_scratch)
    {
      const dp__dd_geom_t g  = { .n = cb, .l = lb, .d = 1, .di = 1, .f = 1 };
      const dp__dd_geom_t gc = { .n = cb, .l = cb, .d = 1, .di = 1, .f = 1 };
      srch->sl_map           = dp_xnn (dp__dd_map_create (&g, 1));
      srch->sl_map_cb        = dp_xnn (dp__dd_map_create (&gc, 1));
      srch->sl_scratch = dp_xmallocn ((6 * cb + lb), sizeof *srch->sl_scratch);
      srch->sl_cmax
          = dp_xmallocn (dp_size_mul_sat (DP_CORRELATOR_SIDELOBE_SLOTS, lb),
                         sizeof *srch->sl_cmax);
      srch->sl_cn = srch->sl_cnext = 0;
      float _Complex *C            = srch->sl_scratch;
      dp__dd_map_forward (srch->sl_map, NULL, 0, srch->ref, C);
      for (size_t b = 0; b < cb; b++)
        {
          C[4 * cb + b] = conjf (C[b]); /* the row's weights */
          C[5 * cb + b] = 1.0f;         /* the plain inverse's */
        }
    }
  /* A pick reads many cells against its few stronger hits, dwell after
     dwell, and each read costs 45 to 90 row transforms uncached; a slot
     keeps one hit's whole row as read in one tile. dp_correlator_regrid()
     empties the cache, so a slot is keyed on the hit and the tile alone. */
  const double f_to = dp_correlator_bin_doppler_hz (st, to->row);
  double       t_to[DP_CORRELATOR_SIDELOBE_TILES_MAX];
  const size_t n_to  = search_tiles_of (st, f_to, 1e-9 * srch->fs, t_to);
  double       worst = 0.0;
  for (size_t t = 0; t < n_to; t++)
    {
      size_t slot = srch->sl_cn;
      for (size_t i = 0; i < srch->sl_cn; i++)
        if (srch->sl_crow[i] == from->row && srch->sl_ccol[i] == from->col
            && srch->sl_ctile[i] == t_to[t])
          {
            slot = i;
            break;
          }
      if (slot == srch->sl_cn)
        {
          if (srch->sl_cn < DP_CORRELATOR_SIDELOBE_SLOTS)
            slot = srch->sl_cn++;
          else
            {
              slot = srch->sl_cnext;
              srch->sl_cnext
                  = (srch->sl_cnext + 1) % DP_CORRELATOR_SIDELOBE_SLOTS;
            }
          srch->sl_crow[slot]  = from->row;
          srch->sl_ccol[slot]  = from->col;
          srch->sl_ctile[slot] = t_to[t];
          search_sidelobe_fill (st, from, t_to[t], srch->sl_cmax + slot * lb);
        }
      worst = fmax (worst, srch->sl_cmax[slot * lb + to->col % lb]);
    }
  return worst > 1.0 ? 1.0 : worst;
}

void
dp_correlator_build_handoff (const dp_correlator_state_t *state,
                             const dp_correlator_hit_t *hit, size_t code_len,
                             dp_correlator_handoff_t *out)
{
  dp_correlator_search_t *const srch = state->search;
  /* Shared with the wideband search's own row->roll mapping — see
     dp_fftfreq_index()'s doc comment for the sign inversion that a second,
     drifted copy of this formula used to cause here. */
  double doppler_hz = dp_correlator_bin_doppler_hz (state, hit->row);
  double phase
      = search_handoff_chip_phase (state, hit->row, hit->col, code_len);

  *out = (dp_correlator_handoff_t){
    .samples_consumed = hit->samples_consumed,
    .chip_phase       = phase,
    .doppler_hz_est   = doppler_hz,
    .doppler_res_hz   = srch->doppler_res_hz,
    .cn0_dbhz_est     = (double)hit->cn0_dbhz_est,
    .peak_mag         = hit->peak_mag,
    .noise_est        = hit->noise_est,
    .test_stat        = hit->test_stat,
  };
}

/* ── Observability (docs/design/async-dsss-receiver.md §2.4) ─────────── */
int
dp_correlator_set_telemetry (dp_correlator_state_t *state, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim,
                             const char **why)
{
  dp_correlator_search_t *const srch = state->search;
  if (!corr2d_is_continuous (state)) /* §18 */
    return DP_ERR_UNSUPPORTED;
  if (!tlm) /* detach: the probe site reverts to the single-branch cost */
    {
      srch->tlm.ctx = NULL;
      return DP_OK;
    }
  const char              *p = prefix ? prefix : "acq";
  static const char *const suffix[10]
      = { "stat", "gate",    "noise",  "peak", "row",
          "col",  "n_peaks", "n_held", "conc", "hit" };
  int32_t *ids[10]
      = { &srch->tlm.id_stat,    &srch->tlm.id_gate,   &srch->tlm.id_noise,
          &srch->tlm.id_peak,    &srch->tlm.id_row,    &srch->tlm.id_col,
          &srch->tlm.id_n_peaks, &srch->tlm.id_n_held, &srch->tlm.id_conc,
          &srch->tlm.id_hit };
  char name[10][DP_TLM_NAME_MAX];
  for (size_t i = 0; i < 10; i++) /* all names before any registration */
    if (dp_tlm_name_join (name[i], p, suffix[i]) != DP_OK)
      {
        if (why) /* overlong prefix: nothing registered */
          *why = "set_telemetry: the prefix is too long: prefix + \".\" + "
                 "each probe name must fit DP_TLM_NAME_MAX bytes";
        return DP_ERR_INVALID;
      }
  /* decim apart from room, so each refusal can name its argument once
     methods take a `why` (just-makeit#2574); the dry run below then
     refuses only a context with no room. */
  if (decim < 1)
    {
      if (why)
        *why = "set_telemetry: decim must be at least 1";
      return DP_ERR_INVALID;
    }
  /* Whole or nothing (#2647). The dry run registers every name on a
     scratch context (decim is already known good), and then asks whether
     the caller's table has room for the names it lacks; only then does
     anything register there. The ids land in a temporary and commit together
     with ctx, so a refusal leaves the previous attachment exactly as it was --
     not ctx on one context with some ids from another. */
  dp_tlm_t *scratch = dp_tlm_create (1);
  if (!scratch)
    return DP_ERR_MEMORY;
  int fits = 1;
  for (size_t i = 0; i < 10 && fits; i++)
    fits = dp_tlm_probe (scratch, name[i], decim) >= 0;
  fits = fits && dp_tlm_has_room_for (tlm, scratch);
  dp_tlm_destroy (scratch);
  if (!fits)
    {
      if (why) /* decim is known good: the table is full */
        *why = "set_telemetry: the context has no room for this search's ten "
               "probes";
      return DP_ERR_INVALID;
    }
  int32_t got[10];
  for (size_t i = 0; i < 10; i++)
    {
      int id = dp_tlm_probe (tlm, name[i], decim);
      if (id < 0) /* cannot happen after the dry run; refuse, never half */
        return DP_ERR_INVALID;
      got[i] = id;
    }
  for (size_t i = 0; i < 10; i++)
    *ids[i] = got[i];
  srch->tlm.ctx = tlm; /* set last: the emit site gates on ctx */
  return DP_OK;
}
size_t
dp_correlator_surface (dp_correlator_state_t *state, float *out, size_t n_out)
{
  dp_correlator_search_t *const srch = state->search;
  if (!corr2d_is_continuous (state)) /* §18: refused, never "none yet" */
    return SIZE_MAX;
  if (!srch->stat_surface || srch->surface_at == 0 || n_out < srch->n_surf)
    return 0;
  memcpy (out, srch->stat_surface, srch->n_surf * sizeof (float));
  return srch->n_surf;
}
size_t
dp_correlator_surface_doppler_hz (dp_correlator_state_t *state, double *out,
                                  size_t n_out)
{
  dp_correlator_search_t *const srch = state->search;
  if (!corr2d_is_continuous (state)) /* §18: refused, never "none yet" */
    return SIZE_MAX;
  const size_t rows = srch->n_surf / srch->lag_bins;
  if (n_out < rows)
    return 0;
  /* Row r is bin r/interp of the native axis (window_bins tiles, or the
     coherent_bins slow-time rows), folded the way every hit is. */
  for (size_t r = 0; r < rows; r++)
    out[r] = (double)dp_fftfreq_index (r, rows) / (double)srch->interp
             * srch->doppler_res_hz;
  return rows;
}
size_t
dp_correlator_surface_chip_phase (dp_correlator_state_t *state, double *out,
                                  size_t n_out)
{
  dp_correlator_search_t *const srch = state->search;
  if (!corr2d_is_continuous (state)) /* §18: refused, never "none yet" */
    return SIZE_MAX;
  const size_t cols = srch->lag_bins;
  if (n_out < cols)
    return 0;
  for (size_t c = 0; c < cols; c++)
    out[c] = search_chip_phase_of_col (c, srch->sf, cols);
  return cols;
}
size_t
dp_correlator_surface_complex (dp_correlator_state_t *state,
                               float _Complex *out, size_t n_out)
{
  dp_correlator_search_t *const srch = state->search;
  /* out_buf is the coherent dump the last dwell was decided on; a
     non-coherent dwell accumulates power elsewhere and out_buf is only
     its last look, so it is not the dwell's. */
  /* Both searches: the map-equivalence test reads the burst search's
     surface against the reference map (test_correlator_search eq_score). */
  if (!srch) /* §18: refused, never "none yet" */
    return SIZE_MAX;
  if (srch->dwells == 0 || srch->n_noncoh > 1 || n_out < srch->n_surf)
    return 0;
  memcpy (out, srch->out_buf, srch->n_surf * sizeof *out);
  return srch->n_surf;
}
/* A whole block sits in blk exactly when the engine gathers blocks, at
   least one epoch has been consumed since create/reset, and the epoch
   counter has wrapped to 0 -- the next push overwrites row 0. */
static int
search_block_whole (const dp_correlator_state_t *state)
{
  dp_correlator_search_t *const srch = state->search;
  return srch->blk_raw != NULL && srch->samples_consumed > 0
         && srch->blk_epoch == 0;
}
size_t
dp_correlator_block_prompt (dp_correlator_state_t *state, size_t tile,
                            size_t col, float _Complex *out, size_t n_out)
{
  dp_correlator_search_t *const srch = state->search;
  if (!corr2d_is_continuous (state)) /* §18: refused, never "none yet" */
    return SIZE_MAX;
  const size_t D = srch->coherent_bins, cb = srch->code_bins;
  const size_t lb = srch->lag_bins;
  if (!search_block_whole (state) || tile >= srch->window_bins || col >= lb
      || n_out < D)
    return 0;
  if (!srch->prompt_scratch)
    {
      srch->prompt_scratch
          = dp_mallocn ((2 * cb + lb), sizeof (float _Complex));
      if (!srch->prompt_scratch)
        return 0;
    }
  float _Complex *wt = srch->prompt_scratch, *row = wt + cb;
  /* The block's epoch spectra, from its raw epochs: the same transform
     the block's surface ran on, so this also holds after a restore. */
  dp__dd_map_forward (state->dd_map, NULL, 0, srch->blk_raw, srch->map_spec);
  const dp__dd_tile_t *t  = &srch->map_tiles[tile];
  const long           sr = dp_fftfreq_index (tile, srch->window_bins);
  const double         d  = srch->carrier_freq_hz > 0.0 && sr != 0
                                ? (double)sr * srch->fs / srch->carrier_freq_hz
                                : 0.0;
  for (size_t k = 0; k < D; k++)
    {
      /* The template at the one scale, ramped EXACTLY to epoch k about the
         block's middle (the map's own row stage, at one epoch: the
         surface reads the same ramp rounded to its fine grid). */
      for (size_t j = 0; j < cb; j++)
        wt[j] = srch->ref_spec[j] / (float)cb;
      if (d != 0.0)
        {
          const double s = ((double)k - 0.5 * (double)(D - 1)) * d;
          search_ramp (wt, cb, -2.0 * M_PI * s / (double)cb, 1, wt);
        }
      dp__dd_map_row1 (state->dd_map, 0, srch->map_spec + k * cb, t->roll, wt,
                       row);
      out[k] = row[col];
    }
  return D;
}
size_t
dp_correlator_block_raw (dp_correlator_state_t *state, float _Complex *out,
                         size_t n_out)
{
  dp_correlator_search_t *const srch = state->search;
  if (!corr2d_is_continuous (state)) /* §18: refused, never "none yet" */
    return SIZE_MAX;
  const size_t n = srch->coherent_bins * srch->code_bins;
  if (!search_block_whole (state) || n_out < n)
    return 0;
  memcpy (out, srch->blk_raw, n * sizeof *out);
  return n;
}
int
dp_correlator_set_surface_sink (dp_correlator_state_t        *state,
                                dp_correlator_surface_sink_fn fn, void *ctx,
                                uint32_t decim)
{
  dp_correlator_search_t *const srch = state->search;
  /* Both searches decide dwells (test_search_equiv records every burst
     dwell through it); a map or detector has no dwell to hand over. */
  if (!srch)
    return DP_ERR_UNSUPPORTED;
  srch->sink       = fn;
  srch->sink_ctx   = ctx;
  srch->sink_decim = decim ? decim : 1u;
  if (fn)
    srch->keep_surface = 1;
  return DP_OK;
}

int
dp_correlator_set_keep_surface (dp_correlator_state_t *state, int keep)
{
  if (!corr2d_is_continuous (state)) /* §18 */
    return DP_ERR_UNSUPPORTED;
  /* A flag: what is kept is what the blob carries (0 or 1, version 2). */
  state->search->keep_surface = keep != 0;
  return DP_OK;
}
/* ── Serializable state — the pure-transducer face ─────────────────────────
 *
 * Fixed flat layout (offsets depend only on configuration), so the state
 * blob is portable POD:
 *   [hdr][extra][ the framer's snapshot ][ float nc_surface[n_surf] ]...
 * The framer's snapshot is its own self-validating child blob: the carry,
 * fewer than frame_n samples, since a push always leaves the framer drained
 * -- and so a fixed size for a given frame_n.
 */

/* Offset to the framer's snapshot: standard envelope + acq's extra header.
 */
/* The blob: the CORL header, the flavor (the reject key every flavor's
   blob opens with), then the search payload. */
#define SEARCH_BODY_OFF                                                       \
  (DP_CORRELATOR_SEARCH_EXTRA_OFF + sizeof (search_extra_t))

static void *
search_state_carry (void *blob)
{
  return (char *)blob + SEARCH_BODY_OFF;
}

static float *
search_state_nc (void *blob, const dp_correlator_state_t *st)
{
  return (float *)((char *)blob + SEARCH_BODY_OFF
                   + dp_f32_framer_state_bytes (&st->framer));
}

static uint32_t *
search_state_twins (void *blob, const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  return (
      uint32_t *)((char *)search_state_nc (blob, st)
                  + (srch->n_noncoh > 1 ? srch->n_surf * sizeof (float) : 0));
}

/* The block accumulator's cells, when the engine has one. */
/* The block's raw epochs (search_resolve_tile_alias), after the block: a
   mid-block split must resume with the epochs the pick will be asked
   about, or the resumed engine decides a tile edge differently. */
static size_t
search_blk_raw_cells (const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  return srch->blk_raw ? srch->coherent_bins * srch->code_bins : 0;
}

static float _Complex *
search_state_blk_raw (void *blob, const dp_correlator_state_t *st)
{
  return (float _Complex *)((char *)search_state_twins (blob, st)
                            + 2 * st->max_peaks * sizeof (uint32_t));
}

size_t
dp__search_state_bytes (const dp_correlator_state_t *st)
{
  dp_correlator_search_t *const srch = st->search;
  size_t b = SEARCH_BODY_OFF + dp_f32_framer_state_bytes (&st->framer);
  if (srch->n_noncoh > 1)
    b += srch->n_surf * sizeof (float);
  b += 2 * st->max_peaks * sizeof (uint32_t);
  b += search_blk_raw_cells (st) * sizeof (float _Complex);
  return b;
}

void
dp__search_get_state (const dp_correlator_state_t *st, void *blob)
{
  dp_correlator_search_t *const srch = st->search;
  const size_t                  n    = srch->n;

  dp_writer_t w = dp_writer_init (blob, dp__search_state_bytes (st));
  dp_w_hdr (&w, CORRELATOR_STATE_MAGIC, CORRELATOR_STATE_VERSION,
            dp__search_state_bytes (st));
  dp_w_u64 (&w, (uint64_t)st->flavor);
  /* Zeroed first: the blob is compared byte for byte by round-trip tests
     and composed into other objects' blobs, so the struct's padding must be
     deterministic, and a designated initialiser leaves it unspecified. */
  search_extra_t ex;
  memset (&ex, 0, sizeof ex);
  ex.has_nc           = (uint16_t)(srch->n_noncoh > 1);
  ex.keep_surface     = (uint16_t)(srch->keep_surface != 0);
  ex.n_noncoh         = (uint32_t)srch->n_noncoh;
  ex.n                = (uint64_t)n;
  ex.samples_consumed = srch->samples_consumed;
  ex.nc_count         = (uint32_t)srch->nc_count;
  ex.max_peaks        = (uint32_t)st->max_peaks;
  ex.n_twins          = (uint32_t)st->n_twins;
  ex.blk_epoch        = (uint32_t)srch->blk_epoch;
  ex.rate_hyps        = (uint32_t)srch->rate_hyps;
  ex.carrier_freq_hz  = srch->carrier_freq_hz;
  dp_w_bytes (&w, &ex, sizeof ex);

  /* The carry: the framer's own snapshot, zero-padded to a fixed size. */
  dp_f32_framer_get_state (&st->framer, search_state_carry (blob));
  if (ex.has_nc)
    memcpy (search_state_nc (blob, st), srch->nc_surface,
            srch->n_surf * sizeof (float));
  /* The twin pairs, then zeros to max_peaks: each region has a writer of
     its own, so its pad is dp_w_zeros like every other blob's. */
  dp_writer_t tw = dp_writer_init (search_state_twins (blob, st),
                                   2 * st->max_peaks * sizeof (uint32_t));
  for (size_t k = 0; k < st->n_twins; k++)
    {
      dp_w_u32 (&tw, (uint32_t)st->twin_row[k]);
      dp_w_u32 (&tw, (uint32_t)st->twin_col[k]);
    }
  dp_w_zeros (&tw, 2 * (st->max_peaks - st->n_twins) * sizeof (uint32_t));
  /* A tiled block is written one epoch at a time, so only its first
     blk_epoch epochs are this block's -- or all D once a block has completed
     and blk_epoch has wrapped to 0: that whole block is what
     dp_correlator_block_prompt and dp_correlator_block_raw hand out. The rest
     are the previous block's, or were never written (dp_xmalloc), and are
     zeros here: a blob is a function of the stream, not of the heap (#2052,
     the block's share of #1471). Nothing reads them before they are rewritten,
     so a restored engine's zeros change no output. */
  const size_t D = srch->coherent_bins, nx = srch->code_bins;
  const size_t live = srch->blk_epoch          ? srch->blk_epoch
                      : srch->samples_consumed ? D
                                               : 0;
  if (srch->blk_raw)
    {
      dp_writer_t rw = dp_writer_init (search_state_blk_raw (blob, st),
                                       search_blk_raw_cells (st)
                                           * sizeof (float _Complex));
      dp_w_cf32 (&rw, srch->blk_raw, live * nx);
      dp_w_zeros (&rw, (D - live) * nx * sizeof (float _Complex));
    }
}

/* Every check set_state makes before it writes anything, and the one
   dp_correlator_state_position() makes: a blob is accepted by both or by
   neither. Reads, never writes; @p ex is the blob's extra block on DP_OK. */
static int
search_state_check (const dp_correlator_state_t *st, const void *blob,
                    search_extra_t *out)
{
  dp_correlator_search_t *const srch = st->search;
  int                           rc
      = dp_state_validate (blob, dp__search_state_bytes (st),
                           CORRELATOR_STATE_MAGIC, CORRELATOR_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  uint64_t flavor;
  memcpy (&flavor, (const char *)blob + sizeof (dp_state_hdr_t),
          sizeof flavor);
  if (flavor != (uint64_t)st->flavor)
    return DP_ERR_INVALID;
  search_extra_t ex;
  memcpy (&ex, (const char *)blob + DP_CORRELATOR_SEARCH_EXTRA_OFF, sizeof ex);
  if (ex.n != (uint64_t)srch->n || ex.n_noncoh != (uint32_t)srch->n_noncoh
      || ex.max_peaks != (uint32_t)st->max_peaks || ex.n_twins > ex.max_peaks
      || ex.blk_epoch >= (uint32_t)srch->coherent_bins
      || ex.rate_hyps != (uint32_t)srch->rate_hyps || ex.keep_surface > 1
      || !dp_correlator_carrier_freq_ok (ex.carrier_freq_hz, srch->fs))
    return DP_ERR_INVALID;

  /* The carry and the engine's position must name ONE stream point: the
     engine counts frame_n per frame it framed, and the carry's snapshot
     records the frames its framer handed out. A blob in which the two
     disagree would shift every later hit's epoch by the difference, without
     a word (#2015's class), so it is refused -- and, like every check here,
     before anything is written. */
  uint64_t frames;
  if (dp_f32_framer_state_frames (&st->framer,
                                  search_state_carry ((void *)blob), &frames)
          != DP_OK
      || frames > UINT64_MAX / srch->frame_n
      || frames * srch->frame_n != ex.samples_consumed)
    return DP_ERR_INVALID;

  /* The dwell's place follows from the same count, so it is held to it
     too: a tiled engine dumps once per block of D frames and every other
     once per frame, the block's epoch is the frames into the block, and
     the non-coherent looks are the dumps into the dwell. */
  const uint64_t D     = srch->window_bins > 1 && srch->coherent_bins > 1
                             ? (uint64_t)srch->coherent_bins
                             : 1;
  const uint64_t dumps = frames / D;
  if (ex.blk_epoch != frames % D
      || ex.nc_count != (srch->n_noncoh > 1 ? dumps % srch->n_noncoh : 0))
    return DP_ERR_INVALID;
  *out = ex;
  return DP_OK;
}

int
dp_correlator_state_position (const dp_correlator_state_t *st,
                              const void *blob, uint64_t *position)
{
  search_extra_t ex;
  uint64_t       pending;
  if (search_state_check (st, blob, &ex) != DP_OK
      || dp_f32_framer_state_pending (
             &st->framer, search_state_carry ((void *)blob), &pending)
             != DP_OK)
    return DP_ERR_INVALID;
  /* dp_correlator_position()'s sum, read from the blob. */
  *position = ex.samples_consumed + pending;
  return DP_OK;
}

int
dp__search_state_check (const dp_correlator_state_t *st, const void *blob)
{
  search_extra_t ex;
  return search_state_check (st, blob, &ex);
}

int
dp__search_set_state (dp_correlator_state_t *st, const void *blob)
{
  dp_correlator_search_t *const srch = st->search;
  search_extra_t                ex;
  int                           rc = search_state_check (st, blob, &ex);
  if (rc != DP_OK)
    return rc;

  /* The carrier is a mutator's value (#2022): restored through its
     setter's rebuild (dp_correlator_regrid), never by writing the field, so a
     blob taken at another carrier gets the fine grid and the tables it needs.
     It is the last thing that can fail, and a failed regrid commits
     nothing, so the engine is untouched either way. */
  if (ex.carrier_freq_hz != srch->carrier_freq_hz
      && dp_correlator_regrid (st, srch->coherent_bins, srch->n_noncoh,
                               srch->window_bins, NULL, ex.carrier_freq_hz)
             != 0)
    return DP_ERR_MEMORY;
  /* Every check has passed; from here the blob is applied. The framer
     cannot refuse it now: state_frames ran set_state's own check. */
  (void)dp_f32_framer_set_state (&st->framer,
                                 search_state_carry ((void *)blob));

  /* Then the rest of the live state. */
  srch->samples_consumed = ex.samples_consumed;
  srch->nc_count         = ex.nc_count;
  srch->keep_surface     = ex.keep_surface;
  st->consumed           = 0;

  if (srch->nc_surface)
    {
      if (ex.has_nc)
        memcpy (srch->nc_surface, search_state_nc ((void *)blob, st),
                srch->n_surf * sizeof (float));
      else
        memset (srch->nc_surface, 0, srch->n_surf * sizeof (float));
    }
  const uint32_t *tw = search_state_twins ((void *)blob, st);
  st->n_twins        = ex.n_twins;
  for (size_t k = 0; k < st->n_twins; k++)
    {
      st->twin_row[k] = tw[2 * k];
      st->twin_col[k] = tw[2 * k + 1];
    }
  srch->blk_epoch = ex.blk_epoch;
  if (srch->blk_raw)
    memcpy (srch->blk_raw, search_state_blk_raw ((void *)blob, st),
            search_blk_raw_cells (st) * sizeof (float _Complex));
  st->n_peaks      = 0;
  srch->n_held     = 0;
  srch->peak_conc  = 0.0f;
  srch->surface_at = 0; /* nothing decided since the restore */
  srch->dwells     = 0;
  return DP_OK;
}

size_t
dp_correlator_run (dp_correlator_state_t *st, const void *state_in,
                   void *state_out, const float _Complex *in, size_t n_in,
                   dp_correlator_hit_t *result, size_t max_results)
{
  if (state_in)
    {
      /* Refused: nothing ran, so nothing was taken -- a resume loop adds
         dp__search_consumed() to its offset, and the previous call's count
         would skip input. */
      if (dp__search_set_state (st, state_in) != 0)
        {
          st->consumed = 0;
          return 0;
        }
    }
  else
    dp__search_reset (st);

  size_t ndet = dp__search_push (st, in, n_in, result, max_results);

  if (state_out)
    dp__search_get_state (st, state_out);
  return ndet;
}

int
dp_correlator_get_keep_surface (const dp_correlator_state_t *state, int *out)
{
  if (!corr2d_is_continuous (state)) /* §18: the continuous search only */
    return DP_ERR_UNSUPPORTED;
  *out = state->search->keep_surface;
  return DP_OK;
}

int
dp_correlator_last_corr_status (const dp_correlator_state_t *state)
{
  const dp_correlator_flavor_t f = dp_correlator_flavor (state);
  return f == DP_CORRELATOR_DETECTOR_1D || f == DP_CORRELATOR_DETECTOR_2D
             ? DP_OK
             : DP_ERR_UNSUPPORTED; /* §18: both detectors only */
}

int
dp_correlator_get_ny (const dp_correlator_state_t *state, size_t *out)
{
  if (corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->ny;
  return DP_OK;
}

int
dp_correlator_get_nx (const dp_correlator_state_t *state, size_t *out)
{
  if (corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->nx;
  return DP_OK;
}

int
dp_correlator_get_ny_out (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_map (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->ny_out;
  return DP_OK;
}

int
dp_correlator_get_nx_out (const dp_correlator_state_t *state, size_t *out)
{
  if (!(state->flavor == DP_CORRELATOR_1D
        || state->flavor == DP_CORRELATOR_2D))
    return DP_ERR_UNSUPPORTED;
  *out = state->nx_out;
  return DP_OK;
}

int
dp_correlator_get_col_out (const dp_correlator_state_t *state, int *out)
{
  if (!(state->flavor == DP_CORRELATOR_1D
        || state->flavor == DP_CORRELATOR_2D))
    return DP_ERR_UNSUPPORTED;
  *out = state->col_out;
  return DP_OK;
}

int
dp_correlator_get_doppler_step (const dp_correlator_state_t *state,
                                double                      *out)
{
  if (!corr2d_is_dd (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->doppler_step;
  return DP_OK;
}

int
dp_correlator_get_n (const dp_correlator_state_t *state, size_t *out)
{
  if (!(state->flavor == DP_CORRELATOR_1D || corr2d_is_detector (state)))
    return DP_ERR_UNSUPPORTED;
  *out = state->n;
  return DP_OK;
}

int
dp_correlator_get_dwell (const dp_correlator_state_t *state, size_t *out)
{
  if (!!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->dwell;
  return DP_OK;
}

int
dp_correlator_get_count (const dp_correlator_state_t *state, size_t *out)
{
  if (!!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->count;
  return DP_OK;
}

int
dp_correlator_get_ring_cap (const dp_correlator_state_t *state, size_t *out)
{
  if (!!corr2d_is_map (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->ring_cap;
  return DP_OK;
}

int
dp_correlator_get_noise_lo (const dp_correlator_state_t *state, size_t *out)
{
  if (!!corr2d_is_map (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->noise_lo;
  return DP_OK;
}

int
dp_correlator_get_noise_hi (const dp_correlator_state_t *state, size_t *out)
{
  if (!!corr2d_is_map (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->noise_hi;
  return DP_OK;
}

int
dp_correlator_get_threshold (const dp_correlator_state_t *state, float *out)
{
  if (!!corr2d_is_map (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->threshold;
  return DP_OK;
}

int
dp_correlator_get_max_peaks (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->max_peaks;
  return DP_OK;
}

int
dp_correlator_get_code_bins (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->code_bins;
  return DP_OK;
}

int
dp_correlator_get_doppler_bins (const dp_correlator_state_t *state,
                                size_t                      *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = dp_correlator_grid_bins (state);
  return DP_OK;
}

int
dp_correlator_get_n_noncoh (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->n_noncoh;
  return DP_OK;
}

int
dp_correlator_get_eta (const dp_correlator_state_t *state, float *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->eta;
  return DP_OK;
}

int
dp_correlator_get_eta_nc (const dp_correlator_state_t *state, float *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->eta_nc;
  return DP_OK;
}

int
dp_correlator_get_pfa_cell (const dp_correlator_state_t *state, double *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->pfa_cell;
  return DP_OK;
}

int
dp_correlator_get_pd_predicted (const dp_correlator_state_t *state,
                                double                      *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->pd_predicted;
  return DP_OK;
}

int
dp_correlator_get_straddle_loss (const dp_correlator_state_t *state,
                                 double                      *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->straddle_loss;
  return DP_OK;
}

int
dp_correlator_get_sample_rate (const dp_correlator_state_t *state, double *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->fs;
  return DP_OK;
}

int
dp_correlator_get_cn0_dbhz (const dp_correlator_state_t *state, double *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->cn0_dbhz;
  return DP_OK;
}

int
dp_correlator_get_doppler_span_hz (const dp_correlator_state_t *state,
                                   double                      *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->doppler_span_hz;
  return DP_OK;
}

int
dp_correlator_get_doppler_res_hz (const dp_correlator_state_t *state,
                                  double                      *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->doppler_res_hz;
  return DP_OK;
}

int
dp_correlator_get_pd (const dp_correlator_state_t *state, double *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->pd;
  return DP_OK;
}

int
dp_correlator_get_underpowered (const dp_correlator_state_t *state, bool *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->underpowered != 0;
  return DP_OK;
}

int
dp_correlator_get_threads (const dp_correlator_state_t *state, int *out)
{
  if (!corr2d_is_search (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->threads;
  return DP_OK;
}

int
dp_correlator_get_lag_bins (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->lag_bins;
  return DP_OK;
}

int
dp_correlator_get_coherent_bins (const dp_correlator_state_t *state,
                                 size_t                      *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->coherent_bins;
  return DP_OK;
}

int
dp_correlator_get_sf (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->sf;
  return DP_OK;
}

int
dp_correlator_get_chip_rate (const dp_correlator_state_t *state, double *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->chip_rate;
  return DP_OK;
}

int
dp_correlator_get_symbol_rate (const dp_correlator_state_t *state, double *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->symbol_rate;
  return DP_OK;
}

int
dp_correlator_get_epochs_per_symbol (const dp_correlator_state_t *state,
                                     double                      *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->epochs_per_symbol;
  return DP_OK;
}

int
dp_correlator_get_n_peaks (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->n_peaks;
  return DP_OK;
}

int
dp_correlator_get_n_held (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->n_held;
  return DP_OK;
}

int
dp_correlator_get_peak_conc (const dp_correlator_state_t *state, float *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->peak_conc;
  return DP_OK;
}

int
dp_correlator_get_carrier_freq_hz (const dp_correlator_state_t *state,
                                   double                      *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->carrier_freq_hz;
  return DP_OK;
}

int
dp_correlator_get_surface_rows (const dp_correlator_state_t *state,
                                size_t                      *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->n_surf / state->search->lag_bins;
  return DP_OK;
}

int
dp_correlator_get_surface_at (const dp_correlator_state_t *state, size_t *out)
{
  if (!corr2d_is_continuous (state))
    return DP_ERR_UNSUPPORTED;
  *out = (size_t)state->search->surface_at;
  return DP_OK;
}

int
dp_correlator_get_reps (const dp_correlator_state_t *state, size_t *out)
{
  if (!(state->flavor == DP_CORRELATOR_SEARCH_BURST))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->reps;
  return DP_OK;
}

int
dp_correlator_get_pd_burst (const dp_correlator_state_t *state, double *out)
{
  if (!(state->flavor == DP_CORRELATOR_SEARCH_BURST))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->pd_burst;
  return DP_OK;
}

int
dp_correlator_get_psl_db (const dp_correlator_state_t *state, double *out)
{
  if (!(state->flavor == DP_CORRELATOR_SEARCH_BURST))
    return DP_ERR_UNSUPPORTED;
  *out = dp_correlator_psl_db (state);
  return DP_OK;
}

int
dp_correlator_get_doppler_rate (const dp_correlator_state_t *state,
                                double                      *out)
{
  if (!(state->flavor == DP_CORRELATOR_SEARCH_BURST))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->doppler_rate;
  return DP_OK;
}

int
dp_correlator_get_rate_hypotheses (const dp_correlator_state_t *state,
                                   size_t                      *out)
{
  if (!(state->flavor == DP_CORRELATOR_SEARCH_BURST))
    return DP_ERR_UNSUPPORTED;
  *out = state->search->rate_hyps;
  return DP_OK;
}
