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

/* Allocate a fresh Dll/RateConverter/MpskReceiver triple (+ seed the
 * pre-despread carrier loop, by value into *car_out -- costas_init never
 * allocates, so it can't fail) from the given hand-off phase/frequency and
 * grid, without touching `state`'s existing children -- the fail-safe half
 * of the "allocate everything first" regrid discipline (_regrid() in
 * acq_core.c is the precedent). On any failure, frees whatever it already
 * allocated and returns -1; `*dll_out` etc. are only ever written on
 * success. */
static int
_build_chain (double chip_rate, double symbol_rate, const uint8_t *code,
              size_t code_len, size_t spc, int m, int differential,
              double chip_phase, double doppler_hz_est, size_t segments,
              size_t sps, int n, costas_state_t *car_out,
              dll_state_t **dll_out, RateConverter_state_t **rc_out,
              mpsk_receiver_state_t **rx_out)
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

  /* MpskReceiver's own carrier loop is seeded at 0, NOT doppler_hz_est --
   * the pre-despread Costas loop below (FLL-assisted, seeded correctly at
   * the front-end rate) is the stage actually responsible for removing
   * the FULL physical Doppler; by the time a wiped, despread, resampled
   * sample reaches MpskReceiver, only a small residual (that loop's own
   * steady-state/settling error) should remain. Seeding MpskReceiver's
   * NCO with the SAME full doppler_hz_est again (an earlier version of
   * this code did, re-evaluated at target_rate=sps*symbol_rate) is a
   * double count that is merely negligible at small Doppler
   * (doppler_hz_est/target_rate ~0) but catastrophic at large offsets:
   * at 15000 Hz / 21600 Hz target_rate = 0.694 cycles/sample, past
   * Nyquist -- the NCO can only represent frequency mod 1 cycle/sample,
   * so this aliases to a real, wrong seed of ~-0.306 cycles/sample, far
   * outside MpskReceiver's own carrier_nda loop's validated pull-in
   * range (~0.01 cycles/sample, carrier_nda_pullin.c) -- confirmed as
   * the direct cause of total lock failure on a real static-offset case
   * (task #148), fixed here. */
  mpsk_receiver_state_t *rx = mpsk_receiver_create (
      m, sps, n, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707, 0.01, 1, 0.3,
      0.0, 30, differential);
  if (!rx)
    {
      RateConverter_destroy (rc);
      dll_destroy (dll);
      return -1;
    }

  /* Pre-despread carrier loop: seeded in cycles/sample at the FRONT-END
   * rate (chip_rate*spc) -- the stage that actually owns removing the
   * full Doppler (see the comment above MpskReceiver's own seed). One
   * costas_update() per code PERIOD (code_len*spc samples) -- `bn`/
   * `bn_fll` are normalized to this update rate (loop_filter_init's own
   * per-update convention, costas_core.c), so `tsamps` here must match
   * the actual call cadence in _carrier_update_from_partials, not a
   * finer per-segment interval (an earlier draft divided this by
   * `segments` to match a since-reverted per-partial update cadence --
   * that quadrupled the loop's real-time bandwidth at fixed `bn`, which
   * measurably made tracking WORSE, not better; see FINISHING_PLAN.md). */
  double front_end_rate = chip_rate * (double)spc;
  costas_init (car_out, DSSS_RX_BN_CARRIER, 0.707,
               doppler_hz_est / front_end_rate, code_len * spc,
               DSSS_RX_BN_FLL);

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
  costas_state_t         car;
  dll_state_t           *dll = NULL;
  RateConverter_state_t *rc  = NULL;
  mpsk_receiver_state_t *rx  = NULL;
  if (_build_chain (s->chip_rate, s->symbol_rate, s->code, s->code_len, s->spc,
                    s->m, s->differential, chip_phase, doppler_hz_est,
                    segments, sps, n, &car, &dll, &rc, &rx)
      != 0)
    return -1;
  _free_chain (s);
  s->car           = car;
  s->dll           = dll;
  s->rc            = rc;
  s->rx            = rx;
  s->segments      = segments;
  s->sps           = sps;
  s->n             = n;
  s->car_carry_len = 0; /* fresh chain: no leftover partial-period tail */
  return 0;
}

/* Sum whatever dll_steps() just emitted for one wiped period into a single
 * DATA-WIPED pseudo-coherent prompt and steer the carrier loop from it,
 * once per period (matching costas_init's own tsamps=one-period
 * calibration, see _build_chain). dll_steps() emits `segments`-many
 * PARTIAL prompts per period, and a data-bit transition can land inside
 * the period -- at SPEC's own async ratio (periods/symbol ~= 1.111) this
 * happens in the large majority of periods, not as a rare edge case. A
 * bare coherent SUM of the raw partials (an earlier draft of this
 * function did exactly that) flips sign between partials straddling the
 * transition and self-cancels. The fix reuses the SAME technique
 * costas_update()'s own bn_fll cross-product term already applies
 * between consecutive symbols ("both prompts are data-wiped [by their
 * own Re sign] so a BPSK bit flip ... does not corrupt the cross
 * product", costas_core.h): sign-align each partial by its OWN real-part
 * sign before adding, so a bit flip between partials reinforces the sum
 * instead of cancelling it. Costas' phase/frequency discriminator only
 * cares about the sum's relative alignment to the carrier, not an
 * absolute data-bit reference, so this data-wiping is exactly as safe
 * here as it already is inside costas_update() itself (a per-partial
 * version of a trick this codebase already trusts, not a new one).
 *
 * Confirmed load-bearing a THIRD time (task #151's investigation, after
 * two earlier re-confirmations recorded in project memory CHECKPOINT
 * 37): dll_steps()'s own segments>1 emit block was tried carrying the
 * winning lookback candidate's own coherent sign through to its OUTPUT
 * (chunk_p[]) instead of leaving this per-partial guess as the only sign
 * source. That output is not internal scratch -- it is the actual
 * despread symbol stream this object hands straight to RateConverter/
 * mpsk_receiver for demodulation (`_track_carrier_dll`'s own dll_out
 * buffer, read again here AND passed on to RateConverter_execute
 * unmodified). Applying one epoch-wide sign there directly corrupted
 * real data: `test_dsss_receiver.py::test_acquires_and_decodes` dropped
 * from a clean decode to ber=0.465, because a natural epoch straddling a
 * transition genuinely carries two different bits in its two halves --
 * exactly the case a shifted lookback candidate winning is meant to
 * detect -- and a single epoch sign silently overwrites one of them.
 * Reverted at the primitive; this per-partial decision-directed guess
 * remains the only sign source for THIS internal carrier-tracking sum, a
 * value that (unlike dll_steps()'s own output) is never handed onward as
 * data. A single (or all-zero-magnitude) partial degenerates harmlessly
 * to a plain pass-through / no-op sum. */
static void
_carrier_update_from_partials (costas_state_t      *car,
                               const float complex *partials, size_t n)
{
  if (n == 0)
    return;
  float complex sum = 0.0f;
  for (size_t k = 0; k < n; k++)
    {
      float sign = (crealf (partials[k]) >= 0.0f) ? 1.0f : -1.0f;
      sum += partials[k] * sign;
    }
  costas_update (car, sum);
}

/* Pre-despread carrier stage: wipe raw samples with the carrier loop's
 * CURRENT held frequency, exactly one code period (`s->tsamps` samples) at
 * a time, run the UNMODIFIED dll_steps() on each wiped period (segments>1
 * lookback fully intact -- dll_steps() may emit up to `segments` partial
 * prompts for that one period, all written straight into `dll_out`, the
 * same shape RateConverter downstream already expects), and
 * costas_update() the carrier from the period's summed partials BEFORE
 * wiping the next period -- that ordering is what gives the loop its
 * per-period responsiveness (see the header's own file doc, and
 * ~/.claude/plans/crystalline-knitting-hopper.md). Any samples that don't
 * fill a whole period are buffered in `car_carry_buf` for the next call,
 * preserving this object's own "any block size" contract regardless of
 * how a caller chunks its steps() calls. Returns the number of despread
 * prompts written to `dll_out` (capped at max_out, a generous
 * caller-supplied bound). */
static size_t
_track_carrier_dll (dsss_receiver_state_t *s, const float complex *x,
                    size_t x_len, float complex *dll_out, size_t max_out)
{
  size_t emitted = 0;
  size_t pos     = 0; /* index into x of the next unconsumed raw sample */

  if (s->car_carry_len > 0)
    {
      size_t need = s->tsamps - s->car_carry_len;
      size_t take = (need <= x_len) ? need : x_len;
      memcpy (s->car_carry_buf + s->car_carry_len, x, take * sizeof (*x));
      s->car_carry_len += take;
      pos = take;
      if (s->car_carry_len < s->tsamps)
        return 0; /* still not a full period; stay buffered */

      for (size_t i = 0; i < s->tsamps; i++)
        s->car_wiped_buf[i] = costas_wipeoff (&s->car, s->car_carry_buf[i]);
      size_t n_out = dll_steps (s->dll, s->car_wiped_buf, s->tsamps,
                                dll_out + emitted, max_out - emitted);
      _carrier_update_from_partials (&s->car, dll_out + emitted, n_out);
      emitted += n_out;
      s->car_carry_len = 0;
    }

  while (pos + s->tsamps <= x_len)
    {
      const float complex *chunk = x + pos;
      for (size_t i = 0; i < s->tsamps; i++)
        s->car_wiped_buf[i] = costas_wipeoff (&s->car, chunk[i]);
      size_t n_out = dll_steps (s->dll, s->car_wiped_buf, s->tsamps,
                                dll_out + emitted, max_out - emitted);
      _carrier_update_from_partials (&s->car, dll_out + emitted, n_out);
      emitted += n_out;
      pos += s->tsamps;
    }

  size_t leftover = x_len - pos;
  if (leftover > 0)
    memcpy (s->car_carry_buf, x + pos, leftover * sizeof (*x));
  s->car_carry_len = leftover;

  return emitted;
}

/* Carrier-wipe -> despread -> resample -> demod a block through the
 * already-built chain. Scratch buffers sized generously (dll's own
 * "emitted <= x_len" bound, then the RateConverter's own configured
 * ratio) rather than relying on a *_max_out() call that would need to
 * know x_len in advance. */
static size_t
_track_chain (dsss_receiver_state_t *s, const float complex *x, size_t x_len,
              float complex *out, size_t max_out)
{
  if (x_len == 0)
    return 0;

  float complex *dll_out = malloc (x_len * sizeof *dll_out);
  if (!dll_out)
    return 0;
  size_t n_dll = _track_carrier_dll (s, x, x_len, dll_out, x_len);

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
                      size_t segments, size_t sps, int differential)
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

  obj->acq = acq_create_continuous (obj->code, code_len, spc, chip_rate,
                                    symbol_rate, cn0_dbhz, doppler_uncertainty,
                                    pfa, pd, 0 /* noise_mode=mean */);
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

  /* tsamps (one code period, samples) is fixed for this object's entire
   * lifetime -- code_len/spc are construction-time invariants, never
   * touched by configure_chain_raw() (segments/sps/n only) -- so the
   * carrier scratch/carry buffers are allocated ONCE here, never
   * per-rebuild, and freed once in destroy(). */
  obj->tsamps        = code_len * spc;
  obj->car_wiped_buf = malloc (obj->tsamps * sizeof (*obj->car_wiped_buf));
  obj->car_carry_buf = malloc (obj->tsamps * sizeof (*obj->car_carry_buf));
  if (!obj->car_wiped_buf || !obj->car_carry_buf)
    {
      free (obj->car_wiped_buf);
      free (obj->car_carry_buf);
      acq_destroy (obj->acq);
      free (obj->code);
      free (obj);
      return NULL;
    }
  obj->car_carry_len = 0;

  /* Placeholder chain (phase 0, no Doppler) -- always allocated, seeded
   * for real the moment a hit fires (see the state struct's own doc
   * comment for why). */
  if (_build_chain (chip_rate, symbol_rate, obj->code, code_len, spc, m,
                    differential, 0.0, 0.0, segments, sps, _derive_n (sps),
                    &obj->car, &obj->dll, &obj->rc, &obj->rx)
      != 0)
    {
      free (obj->car_wiped_buf);
      free (obj->car_carry_buf);
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
  free (state->car_wiped_buf);
  free (state->car_carry_buf);
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

      /* This receiver's embedded engine is always built via
       * acq_create_continuous() -- coherent_bins is pinned at 1, window_bins
       * is the active mechanism, always -- acq_build_handoff()'s only
       * supported mode. */
      acq_handoff_t ho;
      acq_build_handoff (state->acq, &hit, state->code_len, state->spc, &ho);

      if (_rebuild_chain (state, ho.chip_phase, ho.doppler_hz_est,
                          state->segments, state->sps, state->n)
          != 0)
        return 0; /* stay searching; try again on the next hit */

      state->tracking       = 1;
      state->doppler_hz_est = ho.doppler_hz_est;
      state->cn0_dbhz_est   = ho.cn0_dbhz_est;

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
  /* A fixed shape (all five children always present -- see the state
   * struct's own doc comment) so this never depends on `tracking`. The
   * carry buffer's OWN byte count must depend only on `tsamps` (fixed at
   * construction, like Dll's own segments>1 chunk buffers keyed on the
   * construction-fixed `segments`), NOT on the currently-buffered
   * `car_carry_len` (0..tsamps-1, a running value that differs from call
   * to call and instance to instance) -- set_state's envelope check
   * validates the blob's declared size against THIS instance's own
   * state_bytes() before the blob's own car_carry_len is even read, so
   * sizing on a per-call-varying count would spuriously reject valid
   * blobs saved at a different carry depth than the live instance's
   * current one. Always pack the full tsamps-capacity buffer; only the
   * first car_carry_len samples are meaningful (see the struct doc). */
  return sizeof (dp_state_hdr_t) + sizeof (dsss_receiver_extra_t)
         + acq_state_bytes (s->acq) + costas_state_bytes (&s->car)
         + dll_state_bytes (s->dll) + RateConverter_state_bytes (s->rc)
         + mpsk_receiver_state_bytes (s->rx)
         + s->tsamps * sizeof (float _Complex);
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
    .car_carry_len  = (uint64_t)s->car_carry_len,
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  DP_W_CHILD (&_w, acq, s->acq);
  DP_W_CHILD (&_w, costas, &s->car);
  DP_W_CHILD (&_w, dll, s->dll);
  DP_W_CHILD (&_w, RateConverter, s->rc);
  DP_W_CHILD (&_w, mpsk_receiver, s->rx);
  dp_w_cf32 (&_w, s->car_carry_buf, s->tsamps);
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
   * vice versa, since all five children always exist either way.
   * car_carry_len is bounds-checked against this instance's own tsamps
   * capacity before it's used to size the cf32 read below -- a corrupt
   * or hostile blob claiming an oversized carry must be rejected, not
   * overrun the fixed-capacity car_carry_buf. */
  if (extra.segments != (uint64_t)s->segments || extra.sps != (uint64_t)s->sps
      || extra.n != (uint64_t)s->n
      || extra.car_carry_len > (uint64_t)s->tsamps)
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, acq, s->acq);
  DP_R_CHILD (&_r, costas, &s->car);
  DP_R_CHILD (&_r, dll, s->dll);
  DP_R_CHILD (&_r, RateConverter, s->rc);
  DP_R_CHILD (&_r, mpsk_receiver, s->rx);
  dp_r_cf32 (&_r, s->car_carry_buf, s->tsamps);
  s->tracking       = extra.tracking;
  s->doppler_hz_est = extra.doppler_hz_est;
  s->cn0_dbhz_est   = extra.cn0_dbhz_est;
  s->car_carry_len  = (size_t)extra.car_carry_len;
  return DP_OK;
}
