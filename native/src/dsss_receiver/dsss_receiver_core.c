#include "dsss_receiver/dsss_receiver_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Largest divisor of sps in {4, 2, 1} -- MpskReceiver's own carrier-arm
 * count, mirroring the Python story's `next(c for c in (4,2,1) if sps % c
 * == 0)`. */
static int
_derive_n (size_t sps)
{
  if (sps % 4 == 0)
    return 4;
  if (sps % 2 == 0)
    return 2;
  return 1;
}

/* Acquisition's code_phase is a correlation LAG; Dll's init_chip wants the
 * code's own instantaneous phase -- the inversion this project's Stage 2
 * validated (doppler.dsss.handoff.dll_init_chip_from_acq), ported to C. */
static double
_chip_phase_from_hit (size_t code_phase, size_t spc, size_t code_len)
{
  double cl    = (double)code_len;
  double phase = fmod (cl - (double)code_phase / (double)spc, cl);
  if (phase < 0.0)
    phase += cl;
  return phase;
}

/* Allocate a fresh Dll/RateConverter/MpskReceiver triple from the given
 * hand-off phase/frequency and grid, without touching `state`'s existing
 * children -- the fail-safe half of the "allocate everything first" regrid
 * discipline (_regrid() in acq_core.c is the precedent). On any failure,
 * frees whatever it already allocated and returns -1; `*dll_out` etc. are
 * only ever written on success. */
static int
_build_chain (double chip_rate, double symbol_rate, const uint8_t *code,
              size_t code_len, size_t spc, int m, int differential,
              double chip_phase, double doppler_hz_est, size_t segments,
              size_t sps, int n, dll_state_t **dll_out,
              RateConverter_state_t **rc_out, mpsk_receiver_state_t **rx_out)
{
  double partial_rate = chip_rate * (double)segments / (double)code_len;
  double target_rate  = (double)sps * symbol_rate;
  if (partial_rate <= 0.0 || target_rate <= 0.0)
    return -1;

  dll_state_t *dll = dll_create (code, code_len, spc, chip_phase, 0.002, 0.707,
                                 0.5, segments);
  if (!dll)
    return -1;

  RateConverter_state_t *rc
      = RateConverter_create (target_rate / partial_rate, 0);
  if (!rc)
    {
      dll_destroy (dll);
      return -1;
    }

  mpsk_receiver_state_t *rx = mpsk_receiver_create (
      m, sps, n, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707, 0.01, 1, 0.3,
      doppler_hz_est / target_rate, 30, differential);
  if (!rx)
    {
      RateConverter_destroy (rc);
      dll_destroy (dll);
      return -1;
    }

  *dll_out = dll;
  *rc_out  = rc;
  *rx_out  = rx;
  return 0;
}

static void
_free_chain (dsss_receiver_state_t *s)
{
  mpsk_receiver_destroy (s->rx);
  RateConverter_destroy (s->rc);
  dll_destroy (s->dll);
  s->rx  = NULL;
  s->rc  = NULL;
  s->dll = NULL;
}

/* Build a fresh chain and, only on success, swap it in for state's current
 * one (freeing the old triple) -- the "allocate everything first" half of
 * the regrid discipline applied at the call site, not just inside
 * _build_chain. Returns 0/-1 like _build_chain. */
static int
_rebuild_chain (dsss_receiver_state_t *s, double chip_phase,
                double doppler_hz_est, size_t segments, size_t sps, int n)
{
  dll_state_t           *dll = NULL;
  RateConverter_state_t *rc  = NULL;
  mpsk_receiver_state_t *rx  = NULL;
  if (_build_chain (s->chip_rate, s->symbol_rate, s->code, s->code_len, s->spc,
                    s->m, s->differential, chip_phase, doppler_hz_est,
                    segments, sps, n, &dll, &rc, &rx)
      != 0)
    return -1;
  _free_chain (s);
  s->dll      = dll;
  s->rc       = rc;
  s->rx       = rx;
  s->segments = segments;
  s->sps      = sps;
  s->n        = n;
  return 0;
}

/* Despread -> resample -> demod a block through the already-built chain.
 * Scratch buffers sized generously (dll's own "emitted <= x_len" bound,
 * then the RateConverter's own configured ratio) rather than relying on
 * a *_max_out() call that would need to know x_len in advance. */
static size_t
_track_chain (dsss_receiver_state_t *s, const float complex *x, size_t x_len,
              float complex *out, size_t max_out)
{
  if (x_len == 0)
    return 0;

  float complex *dll_out = malloc (x_len * sizeof *dll_out);
  if (!dll_out)
    return 0;
  size_t n_dll = dll_steps (s->dll, x, x_len, dll_out, x_len);

  size_t         rc_cap = (size_t)((double)n_dll * s->rc->rate) + 64;
  float complex *rc_out = malloc (rc_cap * sizeof *rc_out);
  if (!rc_out)
    {
      free (dll_out);
      return 0;
    }
  size_t n_rc = RateConverter_execute (s->rc, dll_out, n_dll, rc_out, rc_cap);
  free (dll_out);

  size_t n_out = mpsk_receiver_steps (s->rx, rc_out, n_rc, out, max_out);
  free (rc_out);
  return n_out;
}

dsss_receiver_state_t *
dsss_receiver_create (const uint8_t *code, size_t code_len, double chip_rate,
                      double symbol_rate, size_t spc, int m, double cn0_dbhz,
                      double pfa, double pd, double doppler_uncertainty,
                      size_t reps, size_t max_noncoh, size_t segments,
                      size_t sps, int differential)
{
  if (!code || code_len < 1 || chip_rate <= 0.0 || symbol_rate <= 0.0
      || spc < 1 || (m != 2 && m != 4 && m != 8) || segments < 1 || sps < 1)
    return NULL;

  dsss_receiver_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;

  obj->code = malloc (code_len);
  if (!obj->code)
    {
      free (obj);
      return NULL;
    }
  memcpy (obj->code, code, code_len);
  obj->code_len = code_len;

  obj->acq = acq_create (obj->code, code_len, reps, spc, chip_rate, cn0_dbhz,
                         doppler_uncertainty, pfa, pd, 0 /* noise_mode=mean */,
                         max_noncoh, symbol_rate);
  if (!obj->acq)
    {
      free (obj->code);
      free (obj);
      return NULL;
    }

  obj->spc          = spc;
  obj->m            = m;
  obj->differential = differential;
  obj->chip_rate    = chip_rate;
  obj->symbol_rate  = symbol_rate;
  obj->tracking     = 0;

  /* Placeholder chain (phase 0, no Doppler) -- always allocated, seeded
   * for real the moment a hit fires (see the state struct's own doc
   * comment for why). */
  if (_build_chain (chip_rate, symbol_rate, obj->code, code_len, spc, m,
                    differential, 0.0, 0.0, segments, sps, _derive_n (sps),
                    &obj->dll, &obj->rc, &obj->rx)
      != 0)
    {
      acq_destroy (obj->acq);
      free (obj->code);
      free (obj);
      return NULL;
    }
  obj->segments = segments;
  obj->sps      = sps;
  obj->n        = _derive_n (sps);
  return obj;
}

void
dsss_receiver_destroy (dsss_receiver_state_t *state)
{
  if (!state)
    return;
  _free_chain (state);
  acq_destroy (state->acq);
  free (state->code);
  free (state);
}

void
dsss_receiver_reset (dsss_receiver_state_t *state)
{
  acq_reset (state->acq);
  /* Best-effort: on OOM, leave the current chain in place rather than
   * signal a failure this void-returning lifecycle function can't report
   * (matches dll_reset()/mpsk_receiver_reset()'s own void contract). */
  _rebuild_chain (state, 0.0, 0.0, state->segments, state->sps, state->n);
  state->tracking       = 0;
  state->doppler_hz_est = 0.0;
  state->cn0_dbhz_est   = 0.0;
  state->samples_fed    = 0;
}

size_t
dsss_receiver_steps_max_out (dsss_receiver_state_t *state)
{
  (void)state;
  return 0; /* caller falls back to allocating x_len -- always sufficient,
               symbols emitted <= raw samples in. */
}

size_t
dsss_receiver_steps (dsss_receiver_state_t *state, const float complex *x,
                     size_t x_len, float complex *out, size_t max_out)
{
  if (x_len == 0)
    return 0;

  if (!state->tracking)
    {
      uint64_t     before = state->samples_fed;
      acq_result_t hit;
      state->samples_fed += x_len;
      size_t n_hits = acq_push (state->acq, x, x_len, &hit, 1);
      if (n_hits == 0)
        return 0;

      /* Everything acq_push has framed so far (state->acq->samples_consumed)
       * minus everything DsssReceiver has ever fed it (before + x_len) is
       * still ring-resident, unprocessed -- and, since every PRIOR call
       * always drained the ring below one frame before returning (it never
       * short-circuits on max_results until this hit), that remainder is
       * guaranteed to be a suffix of THIS call's own x, never a previous
       * call's (already-freed) buffer. */
      uint64_t total_after = before + (uint64_t)x_len;
      uint64_t consumed    = state->acq->samples_consumed;
      uint64_t tail64
          = (total_after > consumed) ? (total_after - consumed) : 0;
      size_t tail_len = (tail64 > (uint64_t)x_len) ? x_len : (size_t)tail64;
      const float complex *tail = x + (x_len - tail_len);

      double chip_phase
          = _chip_phase_from_hit (hit.code_phase, state->spc, state->code_len);
      size_t dop_bins       = state->acq->doppler_bins;
      size_t half           = dop_bins / 2;
      size_t folded         = (hit.doppler_bin + half) % dop_bins;
      long   k_fold         = (long)folded - (long)half;
      double doppler_hz_est = (double)k_fold * state->acq->doppler_res_hz;

      if (_rebuild_chain (state, chip_phase, doppler_hz_est, state->segments,
                          state->sps, state->n)
          != 0)
        return 0; /* stay searching; try again on the next hit */

      state->tracking       = 1;
      state->doppler_hz_est = doppler_hz_est;
      state->cn0_dbhz_est   = (double)hit.cn0_dbhz_est;

      return _track_chain (state, tail, tail_len, out, max_out);
    }

  return _track_chain (state, x, x_len, out, max_out);
}

int
dsss_receiver_configure_search_raw (dsss_receiver_state_t *state,
                                    size_t doppler_bins, size_t n_noncoh)
{
  return acq_configure_search_raw (state->acq, doppler_bins, n_noncoh);
}

void
dsss_receiver_configure_lock_raw (dsss_receiver_state_t *state,
                                  double up_thresh, double down_thresh,
                                  size_t n_looks, double alpha, uint32_t n_up,
                                  uint32_t n_down)
{
  dll_configure_lock_raw (state->dll, up_thresh, down_thresh, n_looks, alpha,
                          n_up, n_down);
}

int
dsss_receiver_configure_chain_raw (dsss_receiver_state_t *state,
                                   size_t segments, size_t sps, int n)
{
  if (segments < 1 || sps < 1 || n < 1 || (int)(sps % (size_t)n) != 0)
    return -1;

  double chip_phase      = dll_get_code_phase (state->dll);
  double old_target_rate = (double)state->sps * state->symbol_rate;
  double doppler_hz_now
      = mpsk_receiver_get_norm_freq (state->rx) * old_target_rate;

  return _rebuild_chain (state, chip_phase, doppler_hz_now, segments, sps, n);
}

int
dsss_receiver_get_tracking (const dsss_receiver_state_t *state)
{
  return state->tracking;
}
double
dsss_receiver_get_doppler_hz (const dsss_receiver_state_t *state)
{
  return state->doppler_hz_est;
}
double
dsss_receiver_get_cn0_dbhz_est (const dsss_receiver_state_t *state)
{
  return state->cn0_dbhz_est;
}
size_t
dsss_receiver_get_segments (const dsss_receiver_state_t *state)
{
  return state->segments;
}
size_t
dsss_receiver_get_sps (const dsss_receiver_state_t *state)
{
  return state->sps;
}
int
dsss_receiver_get_n (const dsss_receiver_state_t *state)
{
  return state->n;
}
double
dsss_receiver_get_chip_phase (const dsss_receiver_state_t *state)
{
  return dll_get_code_phase (state->dll);
}
double
dsss_receiver_get_code_rate (const dsss_receiver_state_t *state)
{
  return dll_get_code_rate (state->dll);
}
double
dsss_receiver_get_lock (const dsss_receiver_state_t *state)
{
  return mpsk_receiver_get_lock (state->rx);
}
double
dsss_receiver_get_norm_freq (const dsss_receiver_state_t *state)
{
  return mpsk_receiver_get_norm_freq (state->rx);
}

size_t
dsss_receiver_state_bytes (const dsss_receiver_state_t *s)
{
  /* A fixed shape (all four children always present -- see the state
   * struct's own doc comment) so this never depends on `tracking`. */
  return sizeof (dp_state_hdr_t) + sizeof (dsss_receiver_extra_t)
         + acq_state_bytes (s->acq) + dll_state_bytes (s->dll)
         + RateConverter_state_bytes (s->rc)
         + mpsk_receiver_state_bytes (s->rx);
}

void
dsss_receiver_get_state (const dsss_receiver_state_t *s, void *blob)
{
  DP_GET_OPEN (DSSS_RECEIVER_STATE_MAGIC, DSSS_RECEIVER_STATE_VERSION,
               dsss_receiver_state_bytes (s));
  dsss_receiver_extra_t extra = {
    .tracking       = (uint8_t)s->tracking,
    .doppler_hz_est = s->doppler_hz_est,
    .cn0_dbhz_est   = s->cn0_dbhz_est,
    .segments       = (uint64_t)s->segments,
    .sps            = (uint64_t)s->sps,
    .n              = (uint64_t)s->n,
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  DP_W_CHILD (&_w, acq, s->acq);
  DP_W_CHILD (&_w, dll, s->dll);
  DP_W_CHILD (&_w, RateConverter, s->rc);
  DP_W_CHILD (&_w, mpsk_receiver, s->rx);
}

int
dsss_receiver_set_state (dsss_receiver_state_t *s, const void *blob)
{
  DP_SET_OPEN (DSSS_RECEIVER_STATE_MAGIC, DSSS_RECEIVER_STATE_VERSION,
               dsss_receiver_state_bytes (s));
  dsss_receiver_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);
  /* segments/sps/n are the layout key -- the blob's grid must match the
   * live engine's (rebuild via configure_chain_raw() first if not).
   * `tracking` is an ordinary restorable value, not a precondition: a
   * freshly-created (searching) engine can restore a tracking blob and
   * vice versa, since all four children always exist either way. */
  if (extra.segments != (uint64_t)s->segments || extra.sps != (uint64_t)s->sps
      || extra.n != (uint64_t)s->n)
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, acq, s->acq);
  DP_R_CHILD (&_r, dll, s->dll);
  DP_R_CHILD (&_r, RateConverter, s->rc);
  DP_R_CHILD (&_r, mpsk_receiver, s->rx);
  s->tracking       = extra.tracking;
  s->doppler_hz_est = extra.doppler_hz_est;
  s->cn0_dbhz_est   = extra.cn0_dbhz_est;
  return DP_OK;
}
