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

/* Frame trailer: the CRC-16 burst_demod checks. Named rather than spelled
 * 16 at each use -- the burst length below is derived from it, and a bare
 * 16 beside a payload length reads like a block size. */
#define DSSS_BR_CRC_BITS 16u

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
                            size_t spc, double chip_rate, size_t payload_len,
                            double cn0_dbhz, double doppler_uncertainty,
                            double pfa, double pd, double carrier_hz,
                            double max_rate, size_t est_segments)
{
  /* Every one of these is an ARGUMENT error, and the manifest's
   * create_error/create_error_message turn a NULL return into a ValueError
   * naming the constraint -- not the blanket MemoryError this would
   * otherwise surface as (the gh-782 shape, see objects/*.toml). */
  if (!acq_code || acq_code_len == 0 || !data_code || data_code_len == 0
      || !sync || sync_len == 0 || reps < 1 || spc < 1 || chip_rate <= 0.0
      || payload_len < 1 || cn0_dbhz <= 0.0 || pfa <= 0.0 || pfa >= 1.0
      || pd <= 0.0 || pd >= 1.0)
    return NULL;

  dsss_burst_receiver_state_t *s = calloc (1, sizeof *s);
  if (!s)
    return NULL;

  s->reps          = reps;
  s->spc           = spc;
  s->chip_rate     = chip_rate;
  s->payload_len   = payload_len;
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

  /* ── Derived geometry ───────────────────────────────────────────────
   * code_period: one acquisition code repetition, in samples. This is the
   * modulus every epoch ambiguity in the design doc is stated against --
   * acq's code_phase is exactly `burst_start mod code_period` (§3.1).
   * burst_len: preamble + the spread sync|payload|CRC frame. */
  s->code_period = acq_code_len * spc;
  {
    size_t frame_syms = sync_len + payload_len + DSSS_BR_CRC_BITS;
    s->burst_len = (reps * acq_code_len + frame_syms * data_code_len) * spc;
  }

  /* ── The history ring (§7.1) ────────────────────────────────────────
   * NOT a caller knob. A detection can fire on the LAST frame inside the
   * preamble, so the burst start is up to reps*code_period behind it; the
   * refine stage searches about that; and the burst itself still has to
   * arrive. Sized from the geometry, which is entirely known here -- a
   * caller asked to size a look-back buffer is a caller handed a way to
   * lose bursts silently.
   *
   * Reusing the double-mapped ring rather than growing a new type: acq
   * already composes it (acq_state_t's `ring`), and the mirror is what
   * lets a window spanning the wrap come back as ONE contiguous
   * float complex * for burst_demod, with no copy and no seam. */
  {
    size_t span = 2u * reps * s->code_period + s->burst_len;
    s->hist     = dp_f32_create (dsss_br_pow2_ceil (span));
    if (!s->hist)
      goto fail;
  }

  /* ── The composed children ──────────────────────────────────────────
   * Certified individually; this object owns only the seam between them.
   * noise_mode 0 = mean, matching burst_acq's own default. */
  s->acq = burst_acq_create (s->acq_code, acq_code_len, reps, spc, chip_rate,
                             cn0_dbhz, doppler_uncertainty, pfa, pd, 0);
  if (!s->acq)
    goto fail;

  s->demod
      = burst_demod_create (s->data_code, data_code_len, spc, chip_rate,
                            carrier_hz, max_rate, payload_len, est_segments);
  if (!s->demod)
    goto fail;
  burst_demod_set_preamble (s->demod, s->acq_code, acq_code_len, reps);
  burst_demod_set_sync (s->demod, s->sync, sync_len);

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
  if (state->acq)
    burst_acq_destroy (state->acq);
  if (state->hist)
    dp_f32_destroy (state->hist);
  free (state->acq_code);
  free (state->data_code);
  free (state->sync);
  free (state);
}

void
dsss_burst_receiver_reset (dsss_burst_receiver_state_t *state)
{
  burst_acq_reset (state->acq);

  /* Drop the ring's contents rather than reallocating: consuming everything
   * readable is what "a fresh stream cannot inherit the previous burst"
   * means for a look-back buffer. */
  dp_f32_consume (state->hist, dp_f32_available (state->hist));

  state->samples_fed = 0;
  state->pending     = 0;

  /* Every read-back, not just the verdict. These ARE the event (§4), and a
   * stale doppler_hz_est beside a cleared frame_valid describes a burst
   * that was never demodulated -- the silent failure burst_demod's own
   * report (F4) found in exactly this shape. */
  state->preamble_start = 0;
  state->frame_valid    = 0;
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
  /* At most one completed burst per call (see the manifest's push doc), so
   * the payload length bounds the output regardless of how many samples
   * arrive. */
  (void)x_len;
  return state->payload_len;
}

size_t
dsss_burst_receiver_push (dsss_burst_receiver_state_t *state,
                          const float complex *x, size_t x_len, uint8_t *out,
                          size_t max_out)
{
  /* <<IMPLEMENT phase 3: search -> refine -> demod.
   *
   * The lifecycle above builds everything this needs. What is left is the
   * three stages of docs/design/dsss-burst-receiver.md §7:
   *
   *   SEARCH  retain x in `hist`, feed burst_acq, collect hits and anchor
   *           each on samples_fed + samples_consumed (an END anchor).
   *   REFINE  correlate the WHOLE preamble over +/- reps*code_period to
   *           recover the exact preamble_start -- the quantity acq
   *           structurally cannot report (§3.1) -- and record
   *           refine_margin, the winner over its nearest whole-period
   *           competitor.
   *   DEMOD   once preamble_start + burst_len has arrived, hand the window
   *           to burst_demod seeded from the EVENT alone, and publish the
   *           read-backs.
   *
   * Until then this reports "no burst completed", which is a legal answer
   * for any input and is what the phase-2 test pins. >> */
  (void)x;
  (void)x_len;
  (void)out;
  (void)max_out;
  state->samples_fed += x_len;
  return 0;
}

int
dsss_burst_receiver_configure_search_raw (dsss_burst_receiver_state_t *state,
                                          size_t doppler_bins, size_t n_noncoh)
{
  return burst_acq_configure_search_raw (state->acq, doppler_bins, n_noncoh);
}

/* ── Read-backs: the DetectionEvent, per burst (§4) ───────────────────── */

uint64_t
dsss_burst_receiver_get_preamble_start (
    const dsss_burst_receiver_state_t *state)
{
  return state->preamble_start;
}

int
dsss_burst_receiver_get_frame_valid (const dsss_burst_receiver_state_t *state)
{
  return state->frame_valid;
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
  return state->pending;
}

uint64_t
dsss_burst_receiver_get_dropped (const dsss_burst_receiver_state_t *state)
{
  return state->dropped;
}

uint64_t
dsss_burst_receiver_get_n_bursts (const dsss_burst_receiver_state_t *state)
{
  return state->n_bursts;
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

/** @brief Samples currently retained in the history ring. */
static size_t
dsss_br_retained (const dsss_burst_receiver_state_t *s)
{
  return dp_f32_available (s->hist);
}

size_t
dsss_burst_receiver_state_bytes (const dsss_burst_receiver_state_t *s)
{
  return sizeof (dp_state_hdr_t)
         + sizeof (uint64_t)
               * 4u          /* samples_fed, n_bursts, dropped, retained */
         + sizeof (uint64_t) /* preamble_start                           */
         + sizeof (uint32_t) * 2u /* frame_valid, pending */
         + sizeof (double) * 7u /* the event's doubles                      */
         + dsss_br_retained (s) * sizeof (float _Complex)
         + acq_state_bytes (s->acq->engine);
}

void
dsss_burst_receiver_get_state (const dsss_burst_receiver_state_t *s,
                               void                              *blob)
{
  DP_GET_OPEN (DSSS_BURST_RECEIVER_STATE_MAGIC,
               DSSS_BURST_RECEIVER_STATE_VERSION,
               dsss_burst_receiver_state_bytes (s));

  size_t retained = dsss_br_retained (s);
  dp_w_u64 (&_w, s->samples_fed);
  dp_w_u64 (&_w, s->n_bursts);
  dp_w_u64 (&_w, s->dropped);
  dp_w_u64 (&_w, (uint64_t)retained);
  dp_w_u64 (&_w, s->preamble_start);
  dp_w_u32 (&_w, (uint32_t)s->frame_valid);
  dp_w_u32 (&_w, (uint32_t)s->pending);
  dp_w_f64 (&_w, s->doppler_hz_est);
  dp_w_f64 (&_w, s->doppler_res_hz);
  dp_w_f64 (&_w, s->cn0_dbhz_est);
  dp_w_f64 (&_w, s->est_freq_hz);
  dp_w_f64 (&_w, s->est_rate_hz);
  dp_w_f64 (&_w, s->est_snr_db);
  dp_w_f64 (&_w, s->refine_margin);

  /* The retained window, oldest first. Read through the ring's own tail so
   * the double mapping hands back one contiguous run across the wrap --
   * the same property that lets a burst window reach burst_demod uncopied. */
  if (retained)
    {
      const float _Complex *tail
          = (const float _Complex *)&s->hist
                ->data[(s->hist->tail & s->hist->mask) * 2];
      dp_w_cf32 (&_w, tail, retained);
    }

  DP_W_CHILD (&_w, acq, s->acq->engine);
}

int
dsss_burst_receiver_set_state (dsss_burst_receiver_state_t *s,
                               const void                  *blob)
{
  /* Opens with dp_state_validate, so a wrong-object, wrong-version,
   * wrong-size or foreign-endian blob is REJECTED rather than
   * reinterpreted. */
  DP_SET_OPEN (DSSS_BURST_RECEIVER_STATE_MAGIC,
               DSSS_BURST_RECEIVER_STATE_VERSION,
               dsss_burst_receiver_state_bytes (s));

  s->samples_fed    = dp_r_u64 (&_r);
  s->n_bursts       = dp_r_u64 (&_r);
  s->dropped        = dp_r_u64 (&_r);
  size_t retained   = (size_t)dp_r_u64 (&_r);
  s->preamble_start = dp_r_u64 (&_r);
  s->frame_valid    = (int)dp_r_u32 (&_r);
  s->pending        = (size_t)dp_r_u32 (&_r);
  s->doppler_hz_est = dp_r_f64 (&_r);
  s->doppler_res_hz = dp_r_f64 (&_r);
  s->cn0_dbhz_est   = dp_r_f64 (&_r);
  s->est_freq_hz    = dp_r_f64 (&_r);
  s->est_rate_hz    = dp_r_f64 (&_r);
  s->est_snr_db     = dp_r_f64 (&_r);
  s->refine_margin  = dp_r_f64 (&_r);

  /* A blob whose retained span cannot fit this receiver's ring is a
   * CONFIG mismatch, not a corrupt blob -- rebuild from the matching
   * geometry rather than truncating and resuming a receiver that silently
   * cannot look as far back as the one that was saved. */
  if (retained > s->hist->capacity)
    return DP_ERR_INVALID;

  dp_f32_consume (s->hist, dp_f32_available (s->hist));
  if (retained)
    {
      const void *src = dp_r_reserve (&_r, retained * sizeof (float _Complex));
      if (!src)
        return DP_ERR_INVALID;
      if (!dp_f32_write (s->hist, (const float *)src, retained))
        return DP_ERR_INVALID;
    }

  DP_R_CHILD (&_r, acq, s->acq->engine);
  return DP_OK;
}
