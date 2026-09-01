/**
 * @file dsss_burst_receiver_core.c
 * @brief DsssBurstReceiver — the burst chain, composed in C.
 *
 * Phase 3 of docs/design/dsss-burst-receiver.md is the algorithm; what is
 * implemented here is the lifecycle it hangs on: argument validation, the
 * composed children, and the history ring whose span is DERIVED from the
 * geometry rather than asked of the caller.
 *
 * `push()` is deliberately still a stub — see its comment. Everything it
 * needs to exist is built here, so the remaining work is the three stages
 * and nothing else.
 */
#include "dsss_burst_receiver/dsss_burst_receiver_core.h"

#include <math.h>

/* Frame trailer: the CRC-16 burst_demod checks. Named rather than spelled
 * 16 at each use -- the burst length below is derived from it, and a bare
 * 16 beside a payload length reads like a block size. */

/** @brief Smallest power of two >= n (the ring's capacity contract). */
static size_t
dsss_br_pow2_ceil (size_t n)
{
  size_t p = 1u;
  while (p < n)
    p <<= 1;
  return p;
}

dsss_burst_receiver_state_t *
dsss_burst_receiver_create (const uint8_t *acq_code, size_t acq_code_len,
                            const uint8_t *data_code, size_t data_code_len,
                            const uint8_t *sync, size_t sync_len, size_t reps,
                            size_t spc, double chip_rate, size_t frame_syms,
                            double cn0_dbhz, double doppler_uncertainty,
                            double pfa, double pd, double carrier_hz,
                            double max_rate, size_t est_segments)
{
  /* Every one of these is an ARGUMENT error, and the manifest's
   * create_error/create_error_message turn a NULL return into a ValueError
   * naming the constraint -- not the blanket MemoryError this would
   * otherwise surface as (the gh-782 shape, declared per object under
   * objects/). */
  if (!acq_code || acq_code_len == 0 || !data_code || data_code_len == 0
      || !sync || sync_len == 0 || reps < 1 || spc < 1 || chip_rate <= 0.0
      || frame_syms < 1 || cn0_dbhz <= 0.0 || pfa <= 0.0 || pfa >= 1.0
      || pd <= 0.0 || pd >= 1.0)
    return NULL;

  dsss_burst_receiver_state_t *s = calloc (1, sizeof *s);
  if (!s)
    return NULL;

  s->reps          = reps;
  s->spc           = spc;
  s->chip_rate     = chip_rate;
  s->frame_syms    = frame_syms;
  s->acq_code_len  = acq_code_len;
  s->data_code_len = data_code_len;
  s->sync_len      = sync_len;

  /* The codes outlive the caller's buffers: this object is fed across many
   * push() calls and rebuilds its demodulator per burst, so borrowing would
   * be a use-after-free the first time a caller freed its own array. */
  s->acq_code  = malloc (acq_code_len);
  s->data_code = malloc (data_code_len);
  s->sync      = malloc (sync_len);
  if (!s->acq_code || !s->data_code || !s->sync)
    goto fail;
  memcpy (s->acq_code, acq_code, acq_code_len);
  memcpy (s->data_code, data_code, data_code_len);
  memcpy (s->sync, sync, sync_len);

  /* ── The demodulator, built FIRST, because it owns the frame ────────
   * Its description says how long the frame is, and everything below is
   * measured in bursts. That used to be `sync + payload + CRC-16` spelled
   * out here -- a fourth copy of one frame shape, and the one that decides
   * how much history the ring keeps, so a frame the transmitter actually
   * sent could not fit in the window this object reserved for it. */
  s->demod
      = burst_demod_create (s->data_code, data_code_len, spc, chip_rate,
                            carrier_hz, max_rate, frame_syms, est_segments);
  if (!s->demod)
    goto fail;
  burst_demod_set_preamble (s->demod, s->acq_code, acq_code_len, reps);
  burst_demod_set_sync (s->demod, s->sync, sync_len);

  /* ── Derived geometry ───────────────────────────────────────────────
   * code_period: one acquisition code repetition, in samples. This is the
   * modulus every epoch ambiguity in the design doc is stated against --
   * acq's code_phase is exactly `burst_start mod code_period` (§3.1).
   * burst_len: preamble + the spread frame, whatever the frame is. */
  s->frame_bits  = frame_syms; /* the row stride of push() and llrs() */
  s->code_period = acq_code_len * spc;
  s->burst_len   = (reps * acq_code_len + frame_syms * data_code_len) * spc;

  /* ── The composed children ──────────────────────────────────────────
   * Certified individually; this object owns only the seam between them.
   *
   * The capture takes `burst_len` because that is the one thing acquisition
   * has no notion of -- and this object is where the frame is known, so it
   * is where the burst length comes from. noise_mode 0 = mean, matching
   * burst_acq's own default. */
  s->cap = burst_capture_create (s->acq_code, acq_code_len, s->burst_len, reps,
                                 spc, chip_rate, cn0_dbhz, doppler_uncertainty,
                                 pfa, pd, 0);
  if (!s->cap)
    goto fail;

  return s;

fail:
  dsss_burst_receiver_destroy (s);
  return NULL;
}

void
dsss_burst_receiver_destroy (dsss_burst_receiver_state_t *state)
{
  if (!state)
    return;
  if (state->demod)
    burst_demod_destroy (state->demod);
  if (state->cap)
    burst_capture_destroy (state->cap);
  free (state->ev);
  free (state->llr);
  free (state->acq_code);
  free (state->data_code);
  free (state->sync);
  free (state);
}

void
dsss_burst_receiver_reset (dsss_burst_receiver_state_t *state)
{
  /* The capture resets the search, rewinds the history ring and clears the
     detection queue. That rewind is what this function got WRONG while the
     ring lived here: `head`/`tail` are monotonic ABSOLUTE counters, so
     consuming everything available left them at the stream's last position
     while `samples_fed` restarted at 0 -- nothing was reachable afterwards,
     refine never ran, and a second pass over the same capture returned
     nothing with dropped=67992 (doppler#1169). Composing the capture fixes
     it by construction: there is one ring, and one place that rewinds it. */
  burst_capture_reset (state->cap);

  state->ev_len  = 0;
  state->llr_len = 0;

  /* Every read-back together. These ARE the event (§4), and a stale
   * doppler_hz_est beside a cleared preamble_start describes a burst that
   * was never demodulated -- the silent failure burst_demod's own report
   * (F4) found in exactly this shape. */
  state->preamble_start = 0;
  state->doppler_hz_est = 0.0;
  state->doppler_res_hz = 0.0;
  state->cn0_dbhz_est   = 0.0;
  state->est_freq_hz    = 0.0;
  state->est_rate_hz    = 0.0;
  state->est_snr_db     = 0.0;
  state->refine_margin  = 0.0;

  /* n_bursts and dropped are LIFETIME counters and deliberately survive:
   * they answer "did this receiver ever lose samples", which a reset that
   * zeroed them could hide. */
}

size_t
dsss_burst_receiver_push_max_out (dsss_burst_receiver_state_t *state,
                                  size_t                       x_len)
{
  /* One frame per window the capture can complete, and the capture already
     answers how many that is -- in samples, over the same input. Deriving it
     from the same numbers a second time is how the two come to disagree. */
  size_t windows
      = burst_capture_push_max_out (state->cap, x_len) / state->burst_len;
  return windows * state->frame_syms;
}

/**
 * @brief Demodulate one window the capture has handed back.
 *
 * The whole of what this object adds to a capture: seed the demodulator from
 * the coarse Doppler the search found, run it over the window, and publish
 * the record. Everything before this -- searching, resolving WHICH preamble
 * repetition a detection landed in, holding history to reach back into, and
 * deciding when two detections name one burst -- is the capture's.
 *
 * The window is BORROWED out of the capture's scratch: contiguous,
 * `burst_len` samples, valid until the next push(). It is not copied here,
 * which is why the composition costs one memcpy per burst rather than two.
 */
static size_t
dsss_br_demod_one (dsss_burst_receiver_state_t *s, size_t i, uint8_t *out,
                   size_t max_out)
{
  const float complex         *w  = burst_capture_window (s->cap, i);
  const burst_capture_event_t *ce = burst_capture_event_at (s->cap, i);
  if (!w || !ce)
    return 0;

  double f0 = ce->doppler_hz_est / (s->chip_rate * (double)s->spc);
  burst_demod_set_prior (s->demod, f0, 0);
  size_t n = burst_demod_demod (s->demod, w, s->burst_len, out, max_out);

  /* Publish the event. These fields ARE the record a consumer receives, so
     they are written together, from one burst, and never left half-updated
     from a previous one. The first five are the capture's -- copied rather
     than re-derived, because the capture measured them; the last three are
     the demodulator's own estimates, which is the half this object adds. */
  s->preamble_start = ce->preamble_start;
  s->doppler_hz_est = ce->doppler_hz_est;
  s->doppler_res_hz = ce->doppler_res_hz;
  s->cn0_dbhz_est   = ce->cn0_dbhz_est;
  s->refine_margin  = ce->refine_margin;
  s->est_freq_hz    = s->demod->est_freq_hz;
  s->est_rate_hz    = s->demod->est_rate_hz;
  s->est_snr_db     = s->demod->est_snr_db;
  s->n_bursts++;

  /* The burst's SOFT bits, alongside its record and for the same reason: one
     push can complete several, and a decoder handed the payload needs the
     LLRs of THAT burst (doppler#1018). */
  {
    const size_t want = s->llr_len + s->frame_bits;
    if (want > s->llr_cap)
      {
        size_t cap = s->llr_cap ? s->llr_cap * 2u : (s->frame_bits * 4u);
        while (cap < want)
          cap *= 2u;
        s->llr     = dp_xrealloc (s->llr, cap * sizeof *s->llr);
        s->llr_cap = cap;
      }
    if (s->llr_len + s->frame_bits <= s->llr_cap)
      s->llr_len += burst_demod_llrs (s->demod, 1, s->llr + s->llr_len,
                                      s->frame_bits);
  }

  /* ...and the same event into this burst's OWN row. One push can complete
     several, and the scalars above can only ever describe the last of them. */
  if (s->ev_len == s->ev_cap)
    {
      size_t cap = s->ev_cap ? s->ev_cap * 2u : 8u;
      s->ev      = dp_xrealloc (s->ev, cap * sizeof *s->ev);
      s->ev_cap  = cap;
    }
  {
    dsss_br_event_t *r = &s->ev[s->ev_len++];
    r->preamble_start  = s->preamble_start;
    r->doppler_hz_est  = s->doppler_hz_est;
    r->doppler_res_hz  = s->doppler_res_hz;
    r->cn0_dbhz_est    = s->cn0_dbhz_est;
    r->est_freq_hz     = s->est_freq_hz;
    r->est_rate_hz     = s->est_rate_hz;
    r->est_snr_db      = s->est_snr_db;
    r->refine_margin   = s->refine_margin;
  }
  return n;
}

size_t
dsss_burst_receiver_push (dsss_burst_receiver_state_t *state,
                          const float complex *x, size_t x_len, uint8_t *out,
                          size_t max_out)
{
  /* Every call starts a fresh event list: `events()` describes THIS push. */
  state->ev_len  = 0;
  state->llr_len = 0;

  /* Find the bursts. Everything the old version of this function did before
     demodulating -- writing the history ring, slicing an oversized push,
     re-feeding acquisition until it had absorbed the chunk, claiming
     detections into bursts, refining each to an exact preamble start, and
     holding the ones whose windows had not arrived -- is the capture's now,
     and is certified there (18 limits).

     `out = NULL` because the windows are not wanted as an array: the C
     consumer face hands them back as borrows, which is what keeps this
     composition at one memcpy per burst instead of two. */
  burst_capture_push (state->cap, x, x_len, NULL, 0);

  /* Demodulate every window it completed -- all of them, not one. A call can
     complete several bursts, and returning one would mean storing the
     surplus, capacity for the store, a drain protocol and back-pressure:
     four ways for a caller to lose a burst by not following a protocol
     (§8.2). Collection is forced by the return value instead. */
  size_t produced = 0;
  size_t ready    = burst_capture_ready (state->cap);
  for (size_t i = 0; i < ready; i++)
    {
      size_t room = max_out > produced ? max_out - produced : 0;
      produced
          += dsss_br_demod_one (state, i, out ? out + produced : NULL, room);
    }
  return produced;
}

size_t
dsss_burst_receiver_llrs_max_out (dsss_burst_receiver_state_t *state, size_t n)
{
  (void)n; /* as events_max_out: the count is the last push's, not a request */
  return state->llr_len;
}

size_t
dsss_burst_receiver_llrs (dsss_burst_receiver_state_t *state, size_t n,
                          float *out, size_t max_out)
{
  (void)n; /* as events(): the count is the last push's, not a request */
  const size_t rows = state->llr_len < max_out ? state->llr_len : max_out;
  if (out && rows)
    memcpy (out, state->llr, rows * sizeof *out);
  return rows;
}

size_t
dsss_burst_receiver_events_max_out (dsss_burst_receiver_state_t *state)
{
  return state->ev_len;
}

size_t
dsss_burst_receiver_events (dsss_burst_receiver_state_t *state, size_t n,
                            dsss_br_event_t *out, size_t max_out)
{
  (void)n; /* see the header: the count is the last push's, not a request */
  size_t rows = state->ev_len < max_out ? state->ev_len : max_out;
  if (out && rows)
    memcpy (out, state->ev, rows * sizeof *out);
  return rows;
}

int
dsss_burst_receiver_configure_search_raw (dsss_burst_receiver_state_t *state,
                                          size_t doppler_bins, size_t n_noncoh)
{
  /* Straight through to the capture, which owns the engine and re-reads the
     blob bound the new grid invalidates. */
  return burst_capture_configure_search_raw (state->cap, doppler_bins,
                                             n_noncoh);
}

/* ── Read-backs: the DetectionEvent, per burst (§4) ───────────────────── */

uint64_t
dsss_burst_receiver_get_preamble_start (
    const dsss_burst_receiver_state_t *state)
{
  return state->preamble_start;
}

double
dsss_burst_receiver_get_doppler_hz_est (
    const dsss_burst_receiver_state_t *state)
{
  return state->doppler_hz_est;
}

double
dsss_burst_receiver_get_doppler_res_hz (
    const dsss_burst_receiver_state_t *state)
{
  return state->doppler_res_hz;
}

double
dsss_burst_receiver_get_cn0_dbhz_est (const dsss_burst_receiver_state_t *state)
{
  return state->cn0_dbhz_est;
}

double
dsss_burst_receiver_get_est_freq_hz (const dsss_burst_receiver_state_t *state)
{
  return state->est_freq_hz;
}

double
dsss_burst_receiver_get_est_rate_hz (const dsss_burst_receiver_state_t *state)
{
  return state->est_rate_hz;
}

double
dsss_burst_receiver_get_est_snr_db (const dsss_burst_receiver_state_t *state)
{
  return state->est_snr_db;
}

double
dsss_burst_receiver_get_refine_margin (
    const dsss_burst_receiver_state_t *state)
{
  return state->refine_margin;
}

size_t
dsss_burst_receiver_get_pending (const dsss_burst_receiver_state_t *state)
{
  /* The capture holds the queue, so it holds the answer. */
  return burst_capture_get_pending (state->cap);
}

uint64_t
dsss_burst_receiver_get_dropped (const dsss_burst_receiver_state_t *state)
{
  /* The ring is the capture\'s, and so is what it refused. */
  return burst_capture_get_dropped (state->cap);
}

uint64_t
dsss_burst_receiver_get_n_bursts (const dsss_burst_receiver_state_t *state)
{
  return state->n_bursts;
}

/* ── The spans a caller must respect ─────────────────────────────────────
 *
 * Forwarded from the capture rather than mirrored into a field here: the
 * capture derives them, so it owns them, and a mirror is a second value to
 * keep in step. Both are still on THIS object's face because a caller
 * placing bursts holds a receiver, not a capture. */

size_t
dsss_burst_receiver_get_refine_span (const dsss_burst_receiver_state_t *state)
{
  return state->cap->refine_span;
}

size_t
dsss_burst_receiver_get_retain_span (const dsss_burst_receiver_state_t *state)
{
  return state->cap->retain_span;
}

/* ── Serializable state ────────────────────────────────────────────────────
 *
 * What travels: this object's own stream bookkeeping and event read-backs,
 * the RETAINED look-back (the next burst's window may begin inside it --
 * dropping it would resume a receiver that cannot reach back, which is the
 * whole point of §7.1), and the acquisition engine's state, delegated to
 * its own triplet rather than re-packed here.
 *
 * What does not: the codes and geometry, which create() restores; and
 * anything of burst_demod's, which has none by design.
 */

/* ── Serializable state ──────────────────────────────────────────────────
 *
 * The composition's blob is this object's own scalars plus the CAPTURE's,
 * nested. That is the whole of the change: the look-back, the detection
 * queue and the acquisition child used to be written out here, field by
 * field, and every one of them now travels inside `burst_capture_get_state`
 * -- one owner, one layout, one place that can get it wrong.
 *
 * Nothing of the demodulator is serialized, deliberately and unchanged: a
 * burst completes inside one demod() call or is lost, so there is no
 * mid-demod position to save (§6.2). The asymmetry the design doc worried
 * about costs nothing, because the seam lands exactly where it already was.
 */

size_t
dsss_burst_receiver_state_bytes (const dsss_burst_receiver_state_t *s)
{
  /* A pure function of CONFIGURATION, deliberately: jm's binding compares an
     incoming blob's length against this before calling set_state, so a size
     that moved with the stream would make a receiver restorable only into an
     instance holding exactly as much history -- which is not resume, it is
     coincidence. The capture's own state_bytes() carries that property for
     the half it owns. */
  return sizeof (dp_state_hdr_t)
         + sizeof (uint64_t) * 2u /* n_bursts, preamble_start              */
         + sizeof (double) * 7u   /* the event's doubles                   */
         + burst_capture_state_bytes (s->cap);
}

void
dsss_burst_receiver_get_state (const dsss_burst_receiver_state_t *s,
                               void                              *blob)
{
  DP_GET_OPEN (DSSS_BURST_RECEIVER_STATE_MAGIC,
               DSSS_BURST_RECEIVER_STATE_VERSION,
               dsss_burst_receiver_state_bytes (s));

  dp_w_u64 (&_w, s->n_bursts);
  dp_w_u64 (&_w, s->preamble_start);
  dp_w_f64 (&_w, s->doppler_hz_est);
  dp_w_f64 (&_w, s->doppler_res_hz);
  dp_w_f64 (&_w, s->cn0_dbhz_est);
  dp_w_f64 (&_w, s->est_freq_hz);
  dp_w_f64 (&_w, s->est_rate_hz);
  dp_w_f64 (&_w, s->est_snr_db);
  dp_w_f64 (&_w, s->refine_margin);

  /* The capture's sub-blob, self-validating: it opens with its own envelope,
     so a corrupted or foreign child is rejected by the child rather than
     reinterpreted by this one. */
  {
    void *region = dp_w_reserve (&_w, burst_capture_state_bytes (s->cap));
    if (region)
      burst_capture_get_state (s->cap, region);
  }
}

int
dsss_burst_receiver_set_state (dsss_burst_receiver_state_t *s,
                               const void                  *blob)
{
  /* Opens with dp_state_validate, so a wrong-object, wrong-version,
     wrong-size or foreign-endian blob is REJECTED rather than
     reinterpreted. */
  DP_SET_OPEN (DSSS_BURST_RECEIVER_STATE_MAGIC,
               DSSS_BURST_RECEIVER_STATE_VERSION,
               dsss_burst_receiver_state_bytes (s));

  s->n_bursts       = dp_r_u64 (&_r);
  s->preamble_start = dp_r_u64 (&_r);
  s->doppler_hz_est = dp_r_f64 (&_r);
  s->doppler_res_hz = dp_r_f64 (&_r);
  s->cn0_dbhz_est   = dp_r_f64 (&_r);
  s->est_freq_hz    = dp_r_f64 (&_r);
  s->est_rate_hz    = dp_r_f64 (&_r);
  s->est_snr_db     = dp_r_f64 (&_r);
  s->refine_margin  = dp_r_f64 (&_r);

  {
    const void *region
        = dp_r_reserve (&_r, burst_capture_state_bytes (s->cap));
    if (!region)
      return DP_ERR_INVALID;
    if (burst_capture_set_state (s->cap, region) != DP_OK)
      return DP_ERR_INVALID;
  }

  /* The last push's rows describe a call that did not happen on this
     instance. Clearing them is what keeps events() honest after a resume. */
  s->ev_len  = 0;
  s->llr_len = 0;
  return DP_OK;
}
