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
    /* Refine searches whole code periods either side of the anchor, because
       acquisition's code phase already fixes the alignment WITHIN a period
       -- the only open question is which repetition. k_lo is generous: the
       anchor is the earliest code epoch of whichever frame detected, and
       with a coherent depth up to `reps` that frame can end a whole preamble
       past the true start. */
    s->k_lo        = 3u * reps + 2u;
    s->k_hi        = 2u;
    s->refine_span = (s->k_lo + s->k_hi + reps) * s->code_period;
    s->retain_span = s->refine_span + s->burst_len;
    /* Twice the retained span, so `chunk_max` below is never zero: a push
       larger than the ring is processed in slices rather than refused,
       which is what "accepts any block size" costs. */
    s->hist = dp_f32_create (dsss_br_pow2_ceil (2u * s->retain_span));
    if (!s->hist)
      goto fail;
    s->chunk_max = s->hist->capacity - s->retain_span;
  }

  /* Refine scratch. The chip signs are real, so a per-period correlation is
     a signed sum over the window rather than a complex multiply -- and
     expanding them to samples once here keeps the divide out of the inner
     loop. */
  s->ref_sign = malloc (s->code_period * sizeof *s->ref_sign);
  if (!s->ref_sign)
    goto fail;
  for (size_t i = 0; i < s->code_period; i++)
    s->ref_sign[i]
        = (s->acq_code[(i / spc) % acq_code_len] & 1u) ? -1.0f : 1.0f;

  /* One correlation per candidate preamble POSITION, not per sample: the
     candidates are anchor + k*P, so the whole search is (k_lo+k_hi+reps)
     code-period correlations rather than a dense sweep. */
  s->corr_len = s->k_lo + s->k_hi + reps + 1u;
  s->corr_buf = malloc (s->corr_len * sizeof *s->corr_buf);
  if (!s->corr_buf)
    goto fail;

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

  /* acq_state_bytes() is ALREADY a pure function of configuration -- it
     sizes its sample region from `ring_cap`, the capacity, not from whatever
     happens to be unconsumed. So this is simply its current value; an
     earlier version added a whole ring on top, on the assumption that it
     tracked the live count. It is re-read in configure_search_raw(), the one
     call that can legitimately change the grid underneath it. */
  s->acq_blob_max = acq_state_bytes (s->acq->engine);

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
  free (state->ref_sign);
  free (state->corr_buf);
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
  state->q_head      = 0;
  state->q_len       = 0;
  memset (state->q, 0, sizeof state->q);
  state->suppress_until = 0;

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

/** @brief Contiguous view of the history ring at a stream position. */
static const float _Complex *
dsss_br_at (const dsss_burst_receiver_state_t *s, uint64_t pos)
{
  /* The ring is double-mapped, so this pointer stays contiguous across the
     wrap -- which is what lets a burst window reach burst_demod uncopied. */
  return (const float _Complex *)&s->hist
      ->data[((size_t)pos & s->hist->mask) * 2];
}

/** @brief Samples currently reachable at or after @p pos. */
static int
dsss_br_have (const dsss_burst_receiver_state_t *s, uint64_t pos, size_t n)
{
  return pos >= s->hist->tail && pos + n <= s->hist->head;
}

/** @brief |correlation| of one code period of preamble at @p pos. */
static double
dsss_br_period_mag (const dsss_burst_receiver_state_t *s, uint64_t pos)
{
  const float _Complex *w  = dsss_br_at (s, pos);
  float                 re = 0.0f, im = 0.0f;
  for (size_t j = 0; j < s->code_period; j++)
    {
      float g = s->ref_sign[j];
      re += g * crealf (w[j]);
      im += g * cimagf (w[j]);
    }
  return sqrt ((double)re * re + (double)im * im);
}

/**
 * @brief Refine a coarse anchor to the exact preamble start.
 *
 * The stage acquisition cannot do. Its code phase is a lag MODULO one code
 * period, so it fixes the alignment WITHIN a period exactly and says nothing
 * about WHICH repetition -- and the anchor derived from it can sit a whole
 * preamble past the true start, because the frame that detected may be the
 * last one overlapping. The candidates are therefore `anchor + k*P` and
 * nothing between: the sub-period question is already answered.
 *
 * Score each candidate by correlating one code period at every position the
 * preamble would occupy and summing the MAGNITUDES. Only `reps - abs(k)` of
 * those positions still land on preamble when a candidate is `k` periods
 * off, so the score follows a triangular envelope peaking at the truth.
 *
 * **Non-coherent across the repetitions, deliberately.** Correlating all
 * `reps * P` samples as one reference is the obvious form and does not
 * survive the residual acquisition leaves: measured, a quarter of a Doppler
 * bin put the coherent peak two whole periods off, with the true position
 * 639x below it. One code period is short enough that a half-bin residual
 * cannot rotate through it. Same coherent-then-non-coherent split `acq`
 * itself uses, asked a finer question.
 *
 * @param s      Receiver.
 * @param anchor Coarse code epoch from the hit.
 * @param start  Written with the refined stream-absolute preamble start.
 * @param margin Written with the runner-up ratio: the best rival period over
 *               the winner. Near 1 means the period was NOT resolved, which
 *               nothing else in the chain can see.
 * @return Non-zero on success; 0 if the search window is not yet reachable,
 *         in which case the caller must try again rather than drop the hit.
 */
static int
dsss_br_refine (dsss_burst_receiver_state_t *s, uint64_t anchor,
                uint64_t *start, double *margin)
{
  size_t P    = s->code_period;
  size_t reps = s->reps;
  /* Back off a WHOLE NUMBER OF CODE PERIODS, always. The candidates are
     `anchor + k*P` precisely because acquisition's code phase already fixes
     the alignment within a period, so a clamp that backed off to sample 0
     would put the whole grid on multiples of P instead -- losing the very
     phase the anchor carries. Measured: with a 127-chip code the sizer picks
     a coherent depth of 1, k_lo*P then exceeds an early anchor, and refine
     returned 11*P exactly while the burst sat 588 samples off it. */
  uint64_t max_back = (anchor / (uint64_t)P) * (uint64_t)P;
  uint64_t back     = (uint64_t)(s->k_lo * P);
  if (back > max_back)
    back = max_back;
  uint64_t lo = anchor - back;

  size_t n_cand = s->k_lo + s->k_hi + 1u;
  if (anchor - lo < (uint64_t)(s->k_lo * P))
    n_cand = (size_t)((anchor - lo) / P) + s->k_hi + 1u;

  /* The last candidate's last repetition has to be reachable too. */
  size_t need = (n_cand - 1u) * P + reps * P;
  if (!dsss_br_have (s, lo, need))
    return 0;

  /* One correlation per preamble POSITION over the whole candidate range,
     computed once; each candidate's score is a sum of `reps` of them. */
  size_t n_pos = n_cand + reps - 1u;
  if (n_pos > s->corr_len)
    n_pos = s->corr_len;
  for (size_t i = 0; i < n_pos; i++)
    s->corr_buf[i]
        = (float)dsss_br_period_mag (s, lo + (uint64_t)(i * P)) + 0.0f * I;

  double best = -1.0, runner = 0.0;
  size_t best_k = 0;
  for (size_t k = 0; k + reps <= n_pos; k++)
    {
      double sum = 0.0;
      for (size_t r = 0; r < reps; r++)
        sum += (double)crealf (s->corr_buf[k + r]);
      if (sum > best)
        {
          best   = sum;
          best_k = k;
        }
    }
  for (size_t k = 0; k + reps <= n_pos; k++)
    {
      if (k == best_k)
        continue;
      double sum = 0.0;
      for (size_t r = 0; r < reps; r++)
        sum += (double)crealf (s->corr_buf[k + r]);
      if (sum > runner)
        runner = sum;
    }

  *start  = lo + (uint64_t)(best_k * P);
  *margin = best > 0.0 ? runner / best : 1.0;
  return 1;
}

/** @brief Release history no stage can still need. */
static void
dsss_br_trim (dsss_burst_receiver_state_t *s)
{
  uint64_t head = s->hist->head;
  uint64_t keep
      = head > (uint64_t)s->retain_span ? head - (uint64_t)s->retain_span : 0;
  if (s->q_len)
    {
      const dsss_br_pending_t *o    = &s->q[s->q_head];
      uint64_t                 base = o->refined ? o->start : o->anchor;
      uint64_t                 back = (uint64_t)(s->k_lo * s->code_period);
      uint64_t                 need = base > back ? base - back : 0;
      if (need < keep)
        keep = need;
    }
  if (keep > s->hist->tail)
    dp_f32_consume (s->hist, (size_t)(keep - s->hist->tail));
}

/**
 * @brief Advance the oldest detection: refine it, then demodulate it.
 *
 * Both steps are retried rather than abandoned. A detection dropped because
 * its window had not fully arrived is a LOST BURST, and it is the exact bug
 * this shape exists to prevent -- the first version refined inline and
 * discarded on a short window, which silently handed the burst to whatever
 * spurious hit came next.
 */
static size_t
dsss_br_emit (dsss_burst_receiver_state_t *s, uint8_t *out, size_t max_out)
{
  if (!s->q_len)
    return 0;
  dsss_br_pending_t *e = &s->q[s->q_head];

  if (!e->refined)
    {
      if (!dsss_br_refine (s, e->anchor, &e->start, &e->margin))
        return 0;
      e->refined = 1;
    }
  if (!dsss_br_have (s, e->start, s->burst_len))
    return 0;

  const acq_state_t *eng = s->acq->engine;
  double             f0  = e->doppler_hz / (s->chip_rate * (double)s->spc);
  burst_demod_set_prior (s->demod, f0, 0);
  size_t n = burst_demod_demod (s->demod, dsss_br_at (s, e->start),
                                s->burst_len, out, max_out);

  /* Publish the event. These fields ARE the record a consumer receives, so
     they are written together, from one burst, and never left half-updated
     from a previous one. */
  s->preamble_start = e->start;
  s->doppler_hz_est = e->doppler_hz;
  s->cn0_dbhz_est   = e->cn0_dbhz;
  s->refine_margin  = e->margin;
  s->frame_valid    = s->demod->frame_valid;
  s->doppler_res_hz = eng->doppler_res_hz;
  s->est_freq_hz    = s->demod->est_freq_hz;
  s->est_rate_hz    = s->demod->est_rate_hz;
  s->est_snr_db     = s->demod->est_snr_db;
  s->n_bursts++;

  s->q_head = (s->q_head + 1u) % DSSS_BR_QCAP;
  s->q_len--;
  s->pending = s->q_len;
  return n;
}

size_t
dsss_burst_receiver_push (dsss_burst_receiver_state_t *state,
                          const float complex *x, size_t x_len, uint8_t *out,
                          size_t max_out)
{
  /* A burst already ready outranks new input: at most one is handed back per
     call, so the read-backs always describe the bits the caller just got. */
  size_t produced = dsss_br_emit (state, out, max_out);
  if (produced)
    return produced;

  const acq_state_t *e   = state->acq->engine;
  size_t             off = 0;
  while (off < x_len)
    {
      size_t chunk = x_len - off;
      if (chunk > state->chunk_max)
        chunk = state->chunk_max;

      dsss_br_trim (state);
      if (!dp_f32_write (state->hist, (const float *)(x + off), chunk))
        {
          /* A dropped sample is a LOST BURST, not a statistic. Counted so a
             caller can size or throttle rather than silently decode fewer
             bursts than arrived. */
          state->dropped += chunk;
          off += chunk;
          continue;
        }
      state->samples_fed += chunk;

      /* SEARCH. */
      acq_result_t hits[16];
      size_t       nh = burst_acq_push (state->acq, x + off, chunk, hits, 16);

      for (size_t i = 0; i < nh; i++)
        {
          /* The hit's own END anchor, made stream-absolute: samples_consumed
             is where this detection's epoch ENDED, so backing off one frame
             and adding the code phase names a code epoch rather than a
             position. Which epoch of the preamble it is, refine decides. */
          uint64_t epoch = hits[i].samples_consumed - (uint64_t)e->n
                           + (uint64_t)hits[i].code_phase;
          if (epoch < state->suppress_until)
            continue; /* same burst, a later frame of its preamble */
          if (state->q_len >= DSSS_BR_QCAP)
            break;

          dsss_br_pending_t *q
              = &state->q[(state->q_head + state->q_len) % DSSS_BR_QCAP];
          q->anchor  = epoch;
          q->start   = 0;
          q->margin  = 1.0;
          q->refined = 0;
          q->doppler_hz
              = dp_fftfreq (hits[i].doppler_bin, e->coherent_bins,
                            e->doppler_res_hz * (double)e->coherent_bins);
          q->cn0_dbhz = hits[i].cn0_dbhz_est;
          state->q_len++;
          state->pending = state->q_len;

          /* Suppress from the anchor forward by a whole burst: every later
             frame of this preamble reports an epoch inside that span. */
          state->suppress_until = epoch + (uint64_t)state->burst_len;
        }

      off += chunk;

      /* Emit as soon as one is ready, so a caller streaming small blocks
         sees a burst when its last sample lands rather than at the end of an
         arbitrarily long push. */
      produced = dsss_br_emit (state, out, max_out);
      if (produced)
        break;
    }
  return produced;
}

int
dsss_burst_receiver_configure_search_raw (dsss_burst_receiver_state_t *state,
                                          size_t doppler_bins, size_t n_noncoh)
{
  int rc = burst_acq_configure_search_raw (state->acq, doppler_bins, n_noncoh);
  /* Re-pinning the grid resizes the acquisition child's blob, so the region
     reserved for it moves with it. state_bytes() stays a pure function of
     CONFIGURATION -- this call IS configuration -- and a blob taken before
     the change correctly stops matching. */
  if (rc == 0)
    state->acq_blob_max = acq_state_bytes (state->acq->engine);
  return rc;
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
  /* A pure function of CONFIGURATION, deliberately: jm's binding compares an
     incoming blob's length against this before calling set_state, so a size
     that moved with the stream would make a receiver restorable only into an
     instance holding exactly as much history -- which is not resume, it is
     coincidence. Both variable regions are fixed-size with a length prefix. */
  return sizeof (dp_state_hdr_t)
         + sizeof (uint64_t) * 4u /* samples_fed, n_bursts, dropped, start  */
         + sizeof (uint32_t) * 2u /* frame_valid, pending                   */
         + sizeof (double) * 7u   /* the event's doubles                    */
         + sizeof (uint64_t)      /* suppress_until                         */
         + sizeof (uint32_t) * 2u /* q_len, q_head                          */
         + sizeof (dsss_br_pending_t) * DSSS_BR_QCAP
         + sizeof (uint32_t) /* retained sample count                       */
         + s->retain_span * sizeof (float _Complex)
         + sizeof (uint32_t) /* acquisition child length                    */
         + s->acq_blob_max;
}

void
dsss_burst_receiver_get_state (const dsss_burst_receiver_state_t *s,
                               void                              *blob)
{
  DP_GET_OPEN (DSSS_BURST_RECEIVER_STATE_MAGIC,
               DSSS_BURST_RECEIVER_STATE_VERSION,
               dsss_burst_receiver_state_bytes (s));

  dp_w_u64 (&_w, s->samples_fed);
  dp_w_u64 (&_w, s->n_bursts);
  dp_w_u64 (&_w, s->dropped);
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
  dp_w_u64 (&_w, s->suppress_until);

  /* The detections in flight. Omitting these would resume a receiver that
     had forgotten a burst it had already found but not yet returned -- a
     silently lost burst, which is the failure this object exists to avoid. */
  dp_w_u32 (&_w, (uint32_t)s->q_len);
  dp_w_u32 (&_w, (uint32_t)s->q_head);
  dp_w_bytes (&_w, s->q, sizeof s->q);

  /* The retained look-back, into a fixed region: the next burst's window may
     begin inside it, so a resume without it cannot reach back. */
  size_t n = dsss_br_retained (s);
  if (n > s->retain_span)
    n = s->retain_span;
  dp_w_u32 (&_w, (uint32_t)n);
  {
    void *region
        = dp_w_reserve (&_w, s->retain_span * sizeof (float _Complex));
    if (region)
      {
        memset (region, 0, s->retain_span * sizeof (float _Complex));
        uint64_t from = s->hist->head - (uint64_t)n;
        memcpy (region, dsss_br_at (s, from), n * sizeof (float _Complex));
      }
  }

  size_t an = acq_state_bytes (s->acq->engine);
  dp_w_u32 (&_w, (uint32_t)an);
  {
    void *region = dp_w_reserve (&_w, s->acq_blob_max);
    if (region && an <= s->acq_blob_max)
      {
        memset (region, 0, s->acq_blob_max);
        acq_get_state (s->acq->engine, region);
      }
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

  s->samples_fed    = dp_r_u64 (&_r);
  s->n_bursts       = dp_r_u64 (&_r);
  s->dropped        = dp_r_u64 (&_r);
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
  s->suppress_until = dp_r_u64 (&_r);

  uint32_t q_len  = dp_r_u32 (&_r);
  uint32_t q_head = dp_r_u32 (&_r);
  if (q_len > DSSS_BR_QCAP || q_head >= DSSS_BR_QCAP)
    return DP_ERR_INVALID;
  s->q_len  = q_len;
  s->q_head = q_head;
  dp_r_bytes (&_r, s->q, sizeof s->q);

  uint32_t n = dp_r_u32 (&_r);
  if ((size_t)n > s->retain_span)
    return DP_ERR_INVALID;
  {
    const void *region
        = dp_r_reserve (&_r, s->retain_span * sizeof (float _Complex));
    if (!region)
      return DP_ERR_INVALID;
    /* Rewind the ring to the saved stream position, so a look-back read at
       an absolute sample index lands where the saving receiver had it. */
    uint64_t head = s->samples_fed;
    DP_STORE_REL (&s->hist->head, (size_t)(head - (uint64_t)n));
    DP_STORE_REL (&s->hist->tail, (size_t)(head - (uint64_t)n));
    if (n && !dp_f32_write (s->hist, (const float *)region, n))
      return DP_ERR_INVALID;
  }

  uint32_t an = dp_r_u32 (&_r);
  {
    const void *region = dp_r_reserve (&_r, s->acq_blob_max);
    if (!region || (size_t)an > s->acq_blob_max)
      return DP_ERR_INVALID;
    if (acq_set_state (s->acq->engine, region) != DP_OK)
      return DP_ERR_INVALID;
  }
  return DP_OK;
}
