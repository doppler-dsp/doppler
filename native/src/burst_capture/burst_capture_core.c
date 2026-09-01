/**
 * @file burst_capture_core.c
 * @brief BurstCapture — search, refine, retain, emit.
 *
 * The search/refine/retain half of what `dsss_burst_receiver_core.c` used to
 * hold alone. Everything here was MOVED rather than rewritten, so the
 * measurements behind each constant (docs/design/dsss-burst-receiver.md §3,
 * §6.1, §7.1) still describe this code.
 */
#include "burst_capture/burst_capture_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/** @brief Smallest power of two >= n (the ring's capacity contract). */
static size_t
burst_capture_pow2_ceil (size_t n)
{
  size_t p = 1u;
  while (p < n)
    p <<= 1;
  return p;
}

/**
 * @brief Both constructors, differing only in where the ring's pages live.
 *
 * @param path          NULL for an anonymous ring; a file to back it with
 *                      otherwise.
 * @param acq_code      Preamble PN chips (0/1), length @p acq_code_len.
 * @param acq_code_len  Preamble code length, chips.
 * @param burst_len     Samples in one burst -- what gets captured.
 * @param reps          Preamble code repetitions.
 * @param spc           Samples per chip.
 * @param chip_rate     Chip rate, Hz.
 * @param cn0_dbhz      C/N0 the search is sized for, dB-Hz.
 * @param doppler_uncertainty  Doppler search half-range, Hz (0 = native).
 * @param pfa           Target false-alarm probability, in (0, 1).
 * @param pd            Target detection probability, in (0, 1).
 * @param noise_mode    CFAR reference: 0=mean, 1=median, 2=min, 3=max.
 * @return Heap state, or NULL on an out-of-range parameter or a file the
 *         ring could not be backed with.
 */
static burst_capture_state_t *
burst_capture_create_impl (const char *path, const uint8_t *acq_code,
                           size_t acq_code_len, size_t burst_len, size_t reps,
                           size_t spc, double chip_rate, double cn0_dbhz,
                           double doppler_uncertainty, double pfa, double pd,
                           int noise_mode)
{
  /* Every one of these is an ARGUMENT error, and the manifest's
   * create_error/create_error_message turn a NULL return into a ValueError
   * naming the constraint -- not the blanket MemoryError this would
   * otherwise surface as. */
  if (!acq_code || acq_code_len == 0 || burst_len == 0 || reps < 1 || spc < 1
      || chip_rate <= 0.0 || cn0_dbhz <= 0.0 || pfa <= 0.0 || pfa >= 1.0
      || pd <= 0.0 || pd >= 1.0)
    return NULL;

  /* dp_xcalloc and friends abort on OOM rather than threading an unwind path
     no test can reach: arguments are validated above, so the only remaining
     failure is genuine exhaustion. The one create() still checked below is
     the acquisition child's, which can refuse a geometry rather than a
     size. */
  burst_capture_state_t *s = dp_xcalloc (1, sizeof *s);

  s->reps         = reps;
  s->spc          = spc;
  s->chip_rate    = chip_rate;
  s->acq_code_len = acq_code_len;
  s->burst_len    = burst_len;

  /* The code outlives the caller's buffer: this object is fed across many
   * push() calls, so borrowing would be a use-after-free the first time a
   * caller freed its own array. */
  s->acq_code = dp_xmalloc (acq_code_len);
  memcpy (s->acq_code, acq_code, acq_code_len);

  /* code_period: one acquisition code repetition, in samples. This is the
   * modulus every epoch ambiguity in the design doc is stated against --
   * acq's code_phase is exactly `burst_start mod code_period` (§3.1). */
  s->code_period = acq_code_len * spc;

  /* ── The history ring (§7.1) ────────────────────────────────────────
   * NOT a caller knob. A detection can fire on the LAST frame inside the
   * preamble, so the burst start is up to reps*code_period behind it; the
   * refine stage searches about that; and the burst itself still has to
   * arrive. Sized from the geometry, which is entirely known here.
   *
   * Reusing the double-mapped ring rather than growing a new type: acq
   * already composes it (acq_state_t's `ring`), and the mirror is what lets
   * a window spanning the wrap be copied out as ONE contiguous run. */
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
    /* The dead air a caller must leave. Derived from the claim rule and the
       detection lag, not fitted: the first burst can be detected on a frame
       up to reps*P past its start and the second on its first frame, so the
       two anchors close by that much before CLAIM ever sees them. Verified
       on four geometries -- at 0.6x this value a pair is 92-98% captured, at
       this value it is 100% (doppler#1172). */
    {
      size_t lag = reps * s->code_period;
      s->min_gap = s->refine_span + lag > s->burst_len
                       ? s->refine_span + lag - s->burst_len
                       : 0u;
    }
    /* Twice the retained span, so `chunk_max` below is never zero: a push
       larger than the ring is processed in slices rather than refused,
       which is what "accepts any block size" costs. */
    size_t cap = burst_capture_pow2_ceil (2u * s->retain_span);
    if (path)
      {
        /* A file failure is the CALLER's -- a bad path, a full disk, a
           read-only mount -- so it returns NULL like any other argument
           error, rather than aborting the way an OOM does. */
        s->hist = dp_f32_create_backed (cap, path, &s->recovered);
        if (!s->hist)
          goto fail;
        s->backed = 1;
      }
    else
      {
        s->hist = dp_xnn (dp_f32_create (cap));
      }
    /* One code period of headroom. The retention bound below lands EXACTLY
       on retain_span, and a merge can move an anchor forward inside the
       window, so leave a period rather than sit on the equality. */
    s->chunk_max = s->hist->capacity - s->retain_span - s->code_period;
  }

  /* ── The detection queue, sized from the geometry ────────────────────
   * Every live entry has a burst window that has NOT arrived, so its base
   * lies within `retain_span` of the head; the CLAIM rule merges anchors
   * closer than `refine_span`, so distinct entries are at least that far
   * apart. The count is therefore retain_span/refine_span, and it is NOT a
   * constant: about 1 at a short-burst test geometry and 5.5x that at a real
   * link. A fixed cap silently dropped the hit and the rest of its batch on
   * any geometry but the one the tests happened to use. */
  {
    size_t per = s->burst_len / s->refine_span + 1u;
    s->q_cap   = 2u * (3u + per);
    if (s->q_cap < 8u)
      s->q_cap = 8u;
    s->q = dp_xcalloc (s->q_cap, sizeof *s->q);
  }

  /* Refine scratch. The chip signs are real, so a per-period correlation is
     a signed sum over the window rather than a complex multiply -- and
     expanding them to samples once here keeps the divide out of the inner
     loop. */
  s->ref_sign = dp_xmalloc (s->code_period * sizeof *s->ref_sign);
  for (size_t i = 0; i < s->code_period; i++)
    s->ref_sign[i]
        = (s->acq_code[(i / spc) % acq_code_len] & 1u) ? -1.0f : 1.0f;

  /* One correlation per candidate preamble POSITION, not per sample: the
     candidates are anchor + k*P, so the whole search is (k_lo+k_hi+reps)
     code-period correlations rather than a dense sweep. */
  s->corr_len = s->k_lo + s->k_hi + reps + 1u;
  s->corr_buf = dp_xmalloc (s->corr_len * sizeof *s->corr_buf);

  /* ── The composed child ─────────────────────────────────────────────
   * Certified individually; this object owns only the seam around it.
   * noise_mode 0 = mean, matching burst_acq's own default. */
  s->acq
      = burst_acq_create (s->acq_code, acq_code_len, reps, spc, chip_rate,
                          cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode);
  if (!s->acq)
    goto fail;

  /* acq_state_bytes() is ALREADY a pure function of configuration -- it
     sizes its sample region from `ring_cap`, the capacity, not from whatever
     happens to be unconsumed. It is re-read in configure_search_raw(), the
     one call that can legitimately change the grid underneath it. */
  s->acq_blob_max = acq_state_bytes (s->acq->engine);
  /* Mirrored once here: the declared warning needs it as a field, and a
     value that cannot change after create() has no reason to be re-read. */
  s->underpowered = s->acq->engine->underpowered ? 1 : 0;

  return s;

fail:
  burst_capture_destroy (s);
  return NULL;
}

burst_capture_state_t *
burst_capture_create (const uint8_t *acq_code, size_t acq_code_len,
                      size_t burst_len, size_t reps, size_t spc,
                      double chip_rate, double cn0_dbhz,
                      double doppler_uncertainty, double pfa, double pd,
                      int noise_mode)
{
  return burst_capture_create_impl (NULL, acq_code, acq_code_len, burst_len,
                                    reps, spc, chip_rate, cn0_dbhz,
                                    doppler_uncertainty, pfa, pd, noise_mode);
}

burst_capture_state_t *
burst_capture_create_backed (const char *path, const uint8_t *acq_code,
                             size_t acq_code_len, size_t burst_len,
                             size_t reps, size_t spc, double chip_rate,
                             double cn0_dbhz, double doppler_uncertainty,
                             double pfa, double pd, int noise_mode)
{
  if (!path || !*path)
    return NULL;
  return burst_capture_create_impl (path, acq_code, acq_code_len, burst_len,
                                    reps, spc, chip_rate, cn0_dbhz,
                                    doppler_uncertainty, pfa, pd, noise_mode);
}

void
burst_capture_destroy (burst_capture_state_t *state)
{
  if (!state)
    return;
  if (state->acq)
    burst_acq_destroy (state->acq);
  if (state->hist)
    dp_f32_destroy (state->hist);
  free (state->acq_code);
  free (state->ref_sign);
  free (state->corr_buf);
  free (state->q);
  free (state->win);
  free (state->ev);
  free (state->det);
  free (state);
}

void
burst_capture_reset (burst_capture_state_t *state)
{
  if (!state)
    return;
  burst_acq_reset (state->acq);
  /* Rewind the ring to zero, not merely empty it. `head`/`tail` are
     MONOTONIC ABSOLUTE counters, so consuming everything available leaves
     them at whatever position the stream had reached while `samples_fed`
     below restarts at 0 -- and every position this object then computes is
     0-based while burst_capture_have() tests against a ring that is not.
     Nothing is reachable, refine never runs, and dp_f32_write refuses, so
     the samples are counted as dropped and no burst is ever emitted again.
     Measured on the receiver this was moved from: after a reset the second
     pass over the SAME capture returned nothing, with dropped=67992
     (doppler#1169). set_state() already rewinds for exactly this reason;
     reset() has to agree with it. */
  DP_STORE_REL (&state->hist->head, 0u);
  DP_STORE_REL (&state->hist->tail, 0u);
  state->samples_fed    = 0;
  state->pending        = 0;
  state->q_head         = 0;
  state->ev_len         = 0;
  state->det_len        = 0;
  state->suppress_until = 0;
  state->preamble_start = 0;
  state->doppler_hz_est = 0.0;
  state->doppler_res_hz = 0.0;
  state->cn0_dbhz_est   = 0.0;
  state->refine_margin  = 0.0;
  /* `dropped` and `n_bursts` deliberately survive: a lost burst stays lost,
     and a lifetime count that reset() zeroed would report a clean stream. */
}

/** @brief Contiguous view of the history ring at a stream position. */
static const float _Complex *
burst_capture_at (const burst_capture_state_t *s, uint64_t pos)
{
  /* The ring is double-mapped, so this pointer stays contiguous across the
     wrap -- which is what lets a burst window be read as one run. */
  return (const float _Complex *)&s->hist
      ->data[((size_t)pos & s->hist->mask) * 2];
}

/** @brief Samples currently reachable at or after @p pos. */
static int
burst_capture_have (const burst_capture_state_t *s, uint64_t pos, size_t n)
{
  return pos >= s->hist->tail && pos + n <= s->hist->head;
}

/** @brief |correlation| of one code period of preamble at @p pos. */
static double
burst_capture_period_mag (const burst_capture_state_t *s, uint64_t pos)
{
  const float _Complex *w  = burst_capture_at (s, pos);
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
 * 639x below it. Same coherent-then-non-coherent split `acq` itself uses,
 * asked a finer question.
 *
 * **How short is "short enough" -- the number, which this comment used to
 * assert without.** Acquisition leaves a residual of at most half a bin, and
 * a bin is `chip_rate / (sf * coherent_bins)`; one code period lasts
 * `sf / chip_rate` seconds. The product is
 *
 *     2*pi * (res/2) * T_P  =  pi / coherent_bins
 *
 * -- `sf` and `chip_rate` CANCEL, so the phase a residual rotates across one
 * code period depends on the coherent depth alone, and on nothing else about
 * the waveform. Across M periods it is `M*pi/coherent_bins`.
 *
 * At the depth this object is usually sized to (`coherent_bins = reps = 4`)
 * one period is already 45 degrees, so M = 1 is the right choice and the
 * fully coherent form is 180 degrees at M = 4 -- which is the null the
 * measurement above walked into.
 *
 * It also says where the choice stops being free: `M <= coherent_bins / 2`
 * keeps the rotation within 90 degrees, so a DEEPER search (`reps = 8` sizes
 * `coherent_bins` up to 8) could coherently integrate 2 or 4 periods and gain
 * amplitude this form discards. Whether that moves the sensitivity knee is
 * unmeasured -- doppler#1177.
 *
 * @param s      Capture.
 * @param anchor Coarse code epoch from the hit.
 * @param start  Written with the refined stream-absolute preamble start.
 * @param margin Written with the runner-up ratio: the best rival period over
 *               the winner. Near 1 means the period was NOT resolved, which
 *               nothing else in the chain can see.
 * @return Non-zero on success; 0 if the search window is not yet reachable,
 *         in which case the caller must try again rather than drop the hit.
 */
static int
burst_capture_refine (burst_capture_state_t *s, uint64_t anchor,
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
  if (!burst_capture_have (s, lo, need))
    return 0;

  /* One correlation per preamble POSITION over the whole candidate range,
     computed once; each candidate's score is a sum of `reps` of them. */
  size_t n_pos = n_cand + reps - 1u;
  if (n_pos > s->corr_len)
    n_pos = s->corr_len;
  for (size_t i = 0; i < n_pos; i++)
    s->corr_buf[i]
        = (float)burst_capture_period_mag (s, lo + (uint64_t)(i * P))
          + 0.0f * I;

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
burst_capture_trim (burst_capture_state_t *s)
{
  uint64_t head = s->hist->head;
  uint64_t keep
      = head > (uint64_t)s->retain_span ? head - (uint64_t)s->retain_span : 0;
  if (s->pending)
    {
      const burst_capture_pending_t *o    = &s->q[s->q_head];
      uint64_t                       base = o->refined ? o->start : o->anchor;
      uint64_t back = (uint64_t)(s->k_lo * s->code_period);
      uint64_t need = base > back ? base - back : 0;
      if (need < keep)
        keep = need;
    }
  if (keep > s->hist->tail)
    dp_f32_consume (s->hist, (size_t)(keep - s->hist->tail));
}

/**
 * @brief Advance the oldest detection: refine it, then emit its window.
 *
 * Both steps are retried rather than abandoned. A detection dropped because
 * its window had not fully arrived is a LOST BURST, and it is the exact bug
 * this shape exists to prevent -- the first version refined inline and
 * discarded on a short window, which silently handed the burst to whatever
 * spurious hit came next.
 *
 * @return Non-zero if a window was emitted.
 */
static int
burst_capture_emit (burst_capture_state_t *s)
{
  if (!s->pending)
    return 0;
  burst_capture_pending_t *e = &s->q[s->q_head];

  if (!e->refined)
    {
      if (!burst_capture_refine (s, e->anchor, &e->start, &e->margin))
        return 0;
      e->refined = 1;
    }
  if (!burst_capture_have (s, e->start, s->burst_len))
    return 0;

  /* Grow the two scratch regions on demand: the row count scales with the
     caller's block size, not with any configuration, which is exactly why
     they are scratch and are never serialized. */
  {
    size_t want = (s->ev_len + 1u) * s->burst_len;
    if (want > s->win_cap)
      {
        size_t cap = s->win_cap ? s->win_cap * 2u : s->burst_len;
        while (cap < want)
          cap *= 2u;
        s->win     = dp_xrealloc (s->win, cap * sizeof *s->win);
        s->win_cap = cap;
      }
  }
  if (s->ev_len == s->ev_cap)
    {
      size_t cap = s->ev_cap ? s->ev_cap * 2u : 8u;
      s->ev      = dp_xrealloc (s->ev, cap * sizeof *s->ev);
      s->ev_cap  = cap;
    }

  /* The window itself, copied out of the ring. One memcpy per BURST -- see
     the `win` field's note on why it is not a borrow. */
  memcpy (s->win + s->ev_len * s->burst_len, burst_capture_at (s, e->start),
          s->burst_len * sizeof *s->win);

  /* Publish the event. These fields ARE the record a consumer receives, so
     they are written together, from one burst, and never left half-updated
     from a previous one. */
  const acq_state_t *eng = s->acq->engine;
  s->preamble_start      = e->start;
  s->doppler_hz_est      = e->doppler_hz;
  s->cn0_dbhz_est        = e->cn0_dbhz;
  s->refine_margin       = e->margin;
  s->doppler_res_hz      = eng->doppler_res_hz;
  s->n_bursts++;

  /* ...and the same event into this burst's OWN row. One push can complete
     several, and the scalars above can only ever describe the last of them;
     a caller needs the record that belongs to each window it was handed. */
  {
    burst_capture_event_t *r = &s->ev[s->ev_len++];
    r->preamble_start        = s->preamble_start;
    r->doppler_hz_est        = s->doppler_hz_est;
    r->doppler_res_hz        = s->doppler_res_hz;
    r->cn0_dbhz_est          = s->cn0_dbhz_est;
    r->refine_margin         = s->refine_margin;
  }

  s->q_head = (s->q_head + 1u) % s->q_cap;
  s->pending--;

  /* A burst that was CAPTURED owns its whole span: its symbols go on firing
     against the acquisition code, and none of that is a new burst. This is
     the only place the long window is armed, and what arms it is refine
     having resolved a start here and this object having handed out the whole
     span -- the physical fact this object owns. Arming it on every DETECTION
     let one spurious hit blind the search for a whole burst and discard the
     next real one (doppler#1004). Candidates already queued inside the span
     go with it; the compaction only ever writes at or behind the entry it
     just read, so it is safe in place. */
  {
    uint64_t until = e->start + (uint64_t)s->burst_len;
    if (until > s->suppress_until)
      s->suppress_until = until;

    size_t keep = 0;
    for (size_t j = 0; j < s->pending; j++)
      {
        burst_capture_pending_t *cand = &s->q[(s->q_head + j) % s->q_cap];
        if (cand->anchor < s->suppress_until)
          continue;
        s->q[(s->q_head + keep) % s->q_cap] = *cand;
        keep++;
      }
    s->pending = keep;
  }

  return 1;
}

/**
 * @brief Emit every detection whose burst window has arrived.
 *
 * Draining FULLY, rather than once per push, is what bounds retention: every
 * entry left in `q` afterwards has a window that has not arrived, so its
 * base lies within burst_len + k_lo*P of the head and burst_capture_trim can
 * always release down to retain_span.
 */
static void
burst_capture_drain (burst_capture_state_t *s)
{
  while (burst_capture_emit (s))
    ;
}

/**
 * @brief Samples the acquisition child has ABSORBED -- framed plus ringed.
 *
 * acq_push() stops once it has filled the caller's result array and leaves
 * the rest of its input unwritten, so a composer has to re-feed the
 * remainder itself. The repo idiom for that diffs against the child's
 * `samples_consumed`, and that is correct where the tail is handed to a
 * different stage.
 *
 * Here the tail goes back to the SAME acq, and `samples_consumed` counts
 * only FRAMED samples -- acq's ring can hold samples it has written but not
 * yet framed. Diffing against it would re-feed those: a DOUBLE-FED stream,
 * which corrupts the detection positions rather than merely losing them.
 * The invariant quantity is framed plus ring-resident.
 */
static uint64_t
burst_capture_acq_absorbed (const burst_capture_state_t *s)
{
  const acq_state_t *e = s->acq->engine;
  uint64_t           h = (uint64_t)DP_LOAD_RLX (&e->ring->head);
  uint64_t           t = (uint64_t)DP_LOAD_RLX (&e->ring->tail);
  return e->samples_consumed + (h - t);
}

size_t
burst_capture_push_max_out (burst_capture_state_t *state, size_t x_len)
{
  /* push() returns EVERY burst it completed, so the bound scales with the
   * input rather than being a constant. Distinct bursts cannot overlap, so
   * they are at least burst_len apart and x_len samples can complete at most
   * x_len/burst_len + 1 of them -- plus every detection already queued from
   * an earlier call, which q_cap bounds. */
  size_t n = x_len / state->burst_len + 1u + state->q_cap;
  return n * state->burst_len;
}

size_t
burst_capture_push (burst_capture_state_t *state, const float complex *x,
                    size_t x_len, float complex *out, size_t max_out)
{
  /* Every call starts fresh: both lists describe THIS push. */
  state->ev_len  = 0;
  state->det_len = 0;
  /* `pending` is NOT cleared here. It is the live queue length -- detections
     whose burst window has not arrived -- and it must survive across pushes,
     because surviving across pushes is the whole point of holding them. */

  /* Drain anything already complete FIRST -- it is returned by this call,
     not held back. In practice this finds nothing, and that is the point:
     because the loop below drains FULLY, a push leaves behind only
     detections whose window has not arrived. */
  burst_capture_drain (state);

  const acq_state_t *e   = state->acq->engine;
  size_t             off = 0;
  while (off < x_len)
    {
      size_t chunk = x_len - off;
      if (chunk > state->chunk_max)
        chunk = state->chunk_max;

      burst_capture_trim (state);
      if (!dp_f32_write (state->hist, (const float *)(x + off), chunk))
        {
          /* A dropped sample is a LOST BURST, not a statistic. Counted so a
             caller can size or throttle rather than silently capture fewer
             bursts than arrived. */
          state->dropped += chunk;
          off += chunk;
          continue;
        }
      state->samples_fed += chunk;

      /* SEARCH -- looping until acq has absorbed the WHOLE chunk. It stops
         as soon as it has filled `hits` and abandons the rest of its input,
         so a single call leaves detections unmade over samples this object
         is holding (doppler#1008). */
      size_t fed = 0;
      while (fed < chunk)
        {
          uint64_t     before = burst_capture_acq_absorbed (state);
          acq_result_t hits[BURST_CAPTURE_HITS];
          size_t nh = burst_acq_push (state->acq, x + off + fed, chunk - fed,
                                      hits, BURST_CAPTURE_HITS);

          for (size_t i = 0; i < nh; i++)
            {
              /* The hit's own END anchor, made stream-absolute:
                 samples_consumed is where this detection's epoch ENDED, so
                 backing off one frame and adding the code phase names a code
                 epoch rather than a position. Which epoch of the preamble it
                 is, refine decides. */
              uint64_t epoch = hits[i].samples_consumed - (uint64_t)e->n
                               + (uint64_t)hits[i].code_phase;

              /* Record the hit BEFORE anything filters it. This is what the
                 search found; `events()` is what survived the claim rule and
                 the suppression window. A bank that wants both would
                 otherwise have to run a second acquisition engine over the
                 same stream (doppler#1174). */
              if (state->det_len == state->det_cap)
                {
                  size_t cap = state->det_cap ? state->det_cap * 2u : 16u;
                  state->det
                      = dp_xrealloc (state->det, cap * sizeof *state->det);
                  state->det_cap = cap;
                }
              {
                burst_capture_detection_t *d = &state->det[state->det_len++];
                d->epoch                     = epoch;
                d->doppler_hz                = dp_fftfreq (
                    hits[i].doppler_bin, e->coherent_bins,
                    e->doppler_res_hz * (double)e->coherent_bins);
                d->cn0_dbhz  = hits[i].cn0_dbhz_est;
                d->test_stat = (double)hits[i].test_stat;
                d->peak_mag  = (double)hits[i].peak_mag;
              }

              /* Inside a burst already CAPTURED: acquisition fires on the
                 payload too, and those are not new bursts. */
              if (epoch < state->suppress_until)
                continue;

              /* THE SAME PREAMBLE as a candidate already queued? Two anchors
                 name one burst exactly when refine can map both onto a single
                 start, and `refine_span` is that reach -- so proximity within
                 it is the identity test, not elapsed distance. Keep the
                 STRONGER of the two: a weak hit that merely arrived first must
                 not own the slot, which is precisely how a spurious detection
                 used to discard the next real burst (doppler#1004). */
              int merged = 0;
              for (size_t j = 0; j < state->pending; j++)
                {
                  burst_capture_pending_t *cand
                      = &state->q[(state->q_head + j) % state->q_cap];
                  uint64_t d = epoch > cand->anchor ? epoch - cand->anchor
                                                    : cand->anchor - epoch;
                  if (d >= (uint64_t)state->refine_span)
                    continue;
                  merged = 1;
                  if ((double)hits[i].peak_mag > cand->peak_mag)
                    {
                      /* The anchor moves, so whatever refine concluded from
                         the old one is stale -- clear `refined` and let it run
                         again rather than pairing a new anchor with an old
                         start. */
                      cand->anchor     = epoch;
                      cand->peak_mag   = (double)hits[i].peak_mag;
                      cand->start      = 0;
                      cand->margin     = 1.0;
                      cand->refined    = 0;
                      cand->doppler_hz = dp_fftfreq (
                          hits[i].doppler_bin, e->coherent_bins,
                          e->doppler_res_hz * (double)e->coherent_bins);
                      cand->cn0_dbhz = hits[i].cn0_dbhz_est;
                    }
                  break;
                }
              if (merged)
                continue;

              if (state->pending >= state->q_cap)
                break;

              burst_capture_pending_t *q
                  = &state->q[(state->q_head + state->pending) % state->q_cap];
              q->anchor   = epoch;
              q->start    = 0;
              q->margin   = 1.0;
              q->peak_mag = (double)hits[i].peak_mag;
              q->refined  = 0;
              q->doppler_hz
                  = dp_fftfreq (hits[i].doppler_bin, e->coherent_bins,
                                e->doppler_res_hz * (double)e->coherent_bins);
              q->cn0_dbhz = hits[i].cn0_dbhz_est;
              state->pending++;
            }

          /* How much acq actually took. A zero means it could not frame at
             all, which its own ring capacity forbids -- break rather than
             spin. */
          uint64_t took64 = burst_capture_acq_absorbed (state) - before;
          size_t   took   = took64 > (uint64_t)(chunk - fed) ? (chunk - fed)
                                                             : (size_t)took64;
          if (!took)
            break;
          fed += took;
        }

      off += chunk;

      /* DRAIN EVERY burst whose window has now arrived -- not one per chunk.
         This is what restores the retention bound: with it, the oldest entry
         left in `q` always has an UNARRIVED window, so its base sits within
         burst_len + k_lo*P of the head and burst_capture_trim can always
         release down to retain_span. Draining one per chunk let the backlog
         grow until dp_f32_write refused and samples were lost
         (doppler#1008). */
      burst_capture_drain (state);
    }

  /* Copy the completed windows out. The scratch is the source of truth for
     both faces: a C consumer borrows it through burst_capture_window()
     instead. */
  size_t have = state->ev_len * state->burst_len;
  size_t rows = have < max_out ? have : max_out;
  /* Never a PARTIAL window: a caller handed 3.5 bursts cannot tell where the
     truncation fell, and a half burst is not a burst. */
  rows -= rows % state->burst_len;
  if (out && rows)
    memcpy (out, state->win, rows * sizeof *out);
  return rows;
}

size_t
burst_capture_detections_max_out (burst_capture_state_t *state, size_t n)
{
  (void)n; /* the count is the last push's, not a request */
  return state->det_len;
}

size_t
burst_capture_detections (burst_capture_state_t *state, size_t n,
                          burst_capture_detection_t *out, size_t max_out)
{
  (void)n;
  const size_t rows = state->det_len < max_out ? state->det_len : max_out;
  if (out && rows)
    memcpy (out, state->det, rows * sizeof *out);
  return rows;
}

size_t
burst_capture_events_max_out (burst_capture_state_t *state, size_t n)
{
  (void)n; /* the count is the last push's, not a request */
  return state->ev_len;
}

size_t
burst_capture_events (burst_capture_state_t *state, size_t n,
                      burst_capture_event_t *out, size_t max_out)
{
  (void)n; /* as events_max_out: the count is the last push's */
  const size_t rows = state->ev_len < max_out ? state->ev_len : max_out;
  if (out && rows)
    memcpy (out, state->ev, rows * sizeof *out);
  return rows;
}

size_t
burst_capture_ready (const burst_capture_state_t *state)
{
  return state->ev_len;
}

const float complex *
burst_capture_window (const burst_capture_state_t *state, size_t i)
{
  if (i >= state->ev_len)
    return NULL;
  return state->win + i * state->burst_len;
}

const burst_capture_event_t *
burst_capture_event_at (const burst_capture_state_t *state, size_t i)
{
  if (i >= state->ev_len)
    return NULL;
  return &state->ev[i];
}

int
burst_capture_configure_search_raw (burst_capture_state_t *state,
                                    size_t doppler_bins, size_t n_noncoh)
{
  /* The child forwards acq's own -1, which is NOT one of the eight codes
     `clib_common.h` defines -- and this header promises DP_ERR_INVALID. A C
     caller branching on the documented code would mis-read a refusal, so the
     translation happens here rather than the doc being weakened to match.
     (The DSP layer returns only DP_OK / DP_ERR_MEMORY / DP_ERR_INVALID; see
     docs/dev/contributing/error-convention.md.) */
  int rc = burst_acq_configure_search_raw (state->acq, doppler_bins, n_noncoh);
  if (rc != 0)
    return DP_ERR_INVALID;
  /* The grid moved, so the child's blob size may have moved with it. This is
     the one call that can legitimately change it underneath a bound that
     state_bytes() promises is a pure function of configuration. */
  state->acq_blob_max = acq_state_bytes (state->acq->engine);
  return DP_OK;
}

/* ── Read-backs ──────────────────────────────────────────────────────────
 *
 * The event of the most recent window emitted. Accessors rather than declared
 * struct fields, because these are written together in one place and read one
 * at a time: a getter is where the "these ARE the record" invariant stays
 * visible. `burst_len`, `refine_span` and `retain_span` are `field = true` in
 * the manifest instead -- they are configuration, fixed at create(). */

uint64_t
burst_capture_get_preamble_start (const burst_capture_state_t *state)
{
  return state->preamble_start;
}

double
burst_capture_get_doppler_hz_est (const burst_capture_state_t *state)
{
  return state->doppler_hz_est;
}

double
burst_capture_get_doppler_res_hz (const burst_capture_state_t *state)
{
  return state->doppler_res_hz;
}

double
burst_capture_get_cn0_dbhz_est (const burst_capture_state_t *state)
{
  return state->cn0_dbhz_est;
}

double
burst_capture_get_refine_margin (const burst_capture_state_t *state)
{
  return state->refine_margin;
}

size_t
burst_capture_get_pending (const burst_capture_state_t *state)
{
  return state->pending;
}

uint64_t
burst_capture_get_dropped (const burst_capture_state_t *state)
{
  return state->dropped;
}

uint64_t
burst_capture_get_n_bursts (const burst_capture_state_t *state)
{
  return state->n_bursts;
}

/* ── The search, as numbers ──────────────────────────────────────────────
 *
 * Forwarded rather than duplicated: every one is the engine's own figure,
 * and re-deriving any of them here would be a second copy of the sizing that
 * the engine already did. */

size_t
burst_capture_get_min_gap (const burst_capture_state_t *state)
{
  return state->min_gap;
}

double
burst_capture_get_eta (const burst_capture_state_t *state)
{
  return state->acq->engine->eta;
}

double
burst_capture_get_eta_nc (const burst_capture_state_t *state)
{
  return state->acq->engine->eta_nc;
}

double
burst_capture_get_straddle_loss (const burst_capture_state_t *state)
{
  return state->acq->engine->straddle_loss;
}

double
burst_capture_get_pd_predicted (const burst_capture_state_t *state)
{
  return state->acq->engine->pd_predicted;
}

size_t
burst_capture_get_doppler_bins (const burst_capture_state_t *state)
{
  return state->acq->engine->coherent_bins;
}

size_t
burst_capture_get_n_noncoh (const burst_capture_state_t *state)
{
  return state->acq->engine->n_noncoh;
}

size_t
burst_capture_get_code_bins (const burst_capture_state_t *state)
{
  return state->acq->engine->code_bins;
}

double
burst_capture_get_doppler_span_hz (const burst_capture_state_t *state)
{
  return state->acq->engine->doppler_span_hz;
}

/* ── Serializable state ──────────────────────────────────────────────── */

size_t
burst_capture_state_bytes (const burst_capture_state_t *s)
{
  /* A pure function of CONFIGURATION, deliberately: jm's binding compares an
     incoming blob's length against this before calling set_state, so a size
     that moved with the stream would make a capture restorable only into an
     instance holding exactly as much history -- which is not resume, it is
     coincidence. Both variable regions are fixed-size with a length prefix. */
  return sizeof (dp_state_hdr_t)
         + sizeof (uint64_t) * 4u /* samples_fed, n_bursts, dropped, start  */
         + sizeof (double) * 4u   /* the event's doubles                    */
         + sizeof (uint64_t)      /* suppress_until                         */
         + sizeof (uint32_t) * 2u /* pending, q_head                        */
         + sizeof (burst_capture_pending_t) * s->q_cap
         + sizeof (uint32_t) /* retained sample count                       */
         /* The look-back, and it is nearly the whole blob -- 2.57 MB at a
            1029-symbol frame, 16.68 MB at 8029 (docs/design/burst-capture.md
            §6). A BACKED capture omits it: the samples are already durable in
            the ring's own file, so the blob only has to name where in the ring
            they sit. `backed` is fixed at create(), so this stays a pure
            function of configuration -- and a backed blob and an in-RAM one
            are different lengths on purpose, which is what stops one being
            restored into the other. */
         + (s->backed ? 0u : s->retain_span * sizeof (float _Complex))
         + sizeof (uint32_t) /* acquisition child length                    */
         + s->acq_blob_max;
}

void
burst_capture_get_state (const burst_capture_state_t *s, void *blob)
{
  DP_GET_OPEN (BURST_CAPTURE_STATE_MAGIC, BURST_CAPTURE_STATE_VERSION,
               burst_capture_state_bytes (s));

  dp_w_u64 (&_w, s->samples_fed);
  dp_w_u64 (&_w, s->n_bursts);
  dp_w_u64 (&_w, s->dropped);
  dp_w_u64 (&_w, s->preamble_start);
  dp_w_f64 (&_w, s->doppler_hz_est);
  dp_w_f64 (&_w, s->doppler_res_hz);
  dp_w_f64 (&_w, s->cn0_dbhz_est);
  dp_w_f64 (&_w, s->refine_margin);
  dp_w_u64 (&_w, s->suppress_until);

  /* The detections in flight. Omitting these would resume a capture that had
     forgotten a burst it had already found but not yet returned -- a
     silently lost burst, which is the failure this object exists to avoid. */
  dp_w_u32 (&_w, (uint32_t)s->pending);
  dp_w_u32 (&_w, (uint32_t)s->q_head);
  dp_w_bytes (&_w, s->q, s->q_cap * sizeof *s->q);

  /* The retained look-back, into a fixed region: the next burst's window may
     begin inside it, so a resume without it cannot reach back. */
  size_t n = dp_f32_available (s->hist);
  if (n > s->retain_span)
    n = s->retain_span;
  dp_w_u32 (&_w, (uint32_t)n);
  if (s->backed)
    {
      /* The samples are the FILE's, so the blob names the count and stops.
         Flushing here is what makes the pair consistent: until the pages are
         written back they live in the page cache, and a blob taken without
         this names a history a crash can still lose. */
      dp_f32_sync (s->hist);
    }
  else
    {
      void *region
          = dp_w_reserve (&_w, s->retain_span * sizeof (float _Complex));
      if (region)
        {
          memset (region, 0, s->retain_span * sizeof (float _Complex));
          uint64_t from = s->hist->head - (uint64_t)n;
          memcpy (region, burst_capture_at (s, from),
                  n * sizeof (float _Complex));
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
burst_capture_set_state (burst_capture_state_t *s, const void *blob)
{
  /* Opens with dp_state_validate, so a wrong-object, wrong-version,
     wrong-size or foreign-endian blob is REJECTED rather than
     reinterpreted. */
  DP_SET_OPEN (BURST_CAPTURE_STATE_MAGIC, BURST_CAPTURE_STATE_VERSION,
               burst_capture_state_bytes (s));

  s->samples_fed    = dp_r_u64 (&_r);
  s->n_bursts       = dp_r_u64 (&_r);
  s->dropped        = dp_r_u64 (&_r);
  s->preamble_start = dp_r_u64 (&_r);
  s->doppler_hz_est = dp_r_f64 (&_r);
  s->doppler_res_hz = dp_r_f64 (&_r);
  s->cn0_dbhz_est   = dp_r_f64 (&_r);
  s->refine_margin  = dp_r_f64 (&_r);
  s->suppress_until = dp_r_u64 (&_r);

  uint32_t pending = dp_r_u32 (&_r);
  uint32_t q_head  = dp_r_u32 (&_r);
  if (pending > s->q_cap || q_head >= s->q_cap)
    return DP_ERR_INVALID;
  s->pending = pending;
  s->q_head  = q_head;
  dp_r_bytes (&_r, s->q, s->q_cap * sizeof *s->q);

  uint32_t n = dp_r_u32 (&_r);
  if ((size_t)n > s->retain_span)
    return DP_ERR_INVALID;
  if (s->backed)
    {
      /* The samples are already in the ring -- they are the file's contents,
         mapped by create(). Only the POSITIONS are restored, so a look-back
         read at an absolute sample index lands where the saving capture had
         it, in the very bytes it wrote.

         Unless the file was not there. create() reports whether it adopted a
         ring of this exact geometry or made a fresh (zeroed) one, and a blob
         that claims retained history against a fresh file is a resume into
         silence: the positions would be right and the samples would be zeros,
         so every later burst would simply not be found. Refuse it. */
      if (n && !s->recovered)
        return DP_ERR_INVALID;
      uint64_t head = s->samples_fed;
      DP_STORE_REL (&s->hist->tail, (size_t)(head - (uint64_t)n));
      DP_STORE_REL (&s->hist->head, (size_t)head);
    }
  else
    {
      const void *region
          = dp_r_reserve (&_r, s->retain_span * sizeof (float _Complex));
      if (!region)
        return DP_ERR_INVALID;
      /* Rewind the ring to the saved stream position, so a look-back read at
         an absolute sample index lands where the saving capture had it. */
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

  /* The last push's rows describe a call that did not happen on this
     instance. Clearing them is what keeps both faces honest after a resume. */
  s->ev_len  = 0;
  s->det_len = 0;
  return DP_OK;
}
