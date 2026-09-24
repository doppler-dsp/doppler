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

#include "util/util_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Both constructors, differing only in where the ring's pages live.
 *
 * @param path          NULL for an anonymous ring; a file to back it with
 *                      otherwise.
 * @param preamble      One period of the preamble, @p n samples.
 * @param n             Samples per repetition.
 * @param burst_len     Samples in one burst -- what gets captured.
 * @param reps          Preamble repetitions.
 * @param fs            Sample rate, Hz.
 * @param cn0_dbhz      Design (minimum) C/N0 the search is sized for,
 *                      dB-Hz; NaN = no design point (see acq_create_burst).
 * @param doppler_uncertainty  Doppler search half-range, Hz (0 = native).
 * @param pfa           Target false-alarm probability, in (0, 1).
 * @param pd            Target detection probability, in (0, 1).
 * @param noise_mode    CFAR reference: 0=mean, 1=median, 2=min, 3=max.
 * @param doppler_rate  Doppler rate, Hz/s, capping the coherent depth; 0 is
 *                      no bound.
 * @return Heap state, or NULL on an out-of-range parameter or a file the
 *         ring could not be backed with.
 */
static burst_capture_state_t *
burst_capture_create_impl (const char *path, const float _Complex *preamble,
                           size_t n, size_t burst_len, size_t reps, double fs,
                           double cn0_dbhz, double doppler_uncertainty,
                           double pfa, double pd, int noise_mode,
                           double doppler_rate)
{
  /* Every one of these is an ARGUMENT error, and the manifest's
   * create_error/create_error_message turn a NULL return into a ValueError
   * naming the constraint -- not the blanket MemoryError this would
   * otherwise surface as. */
  if (!preamble || n == 0 || burst_len == 0 || reps < 1 || !(fs > 0.0)
      || pfa <= 0.0 || pfa >= 1.0 || pd <= 0.0 || pd >= 1.0)
    return NULL;
  /* cn0_dbhz and the preamble's energy are NOT checked here: what a valid
     design C/N0 is -- any finite value, or NaN for none (doppler#1484) --
     and what a searchable preamble is are the engine's rules, and it
     refuses the rest; burst_acq_create() returns NULL and this unwinds.
     A second copy here is how the old `< 0` outlived the rule it copied. */

  /* dp_xcalloc and friends abort on OOM rather than threading an unwind path
     no test can reach: arguments are validated above, so the only remaining
     failure is genuine exhaustion. The one create() still checked below is
     the acquisition child's, which can refuse a geometry rather than a
     size. */
  burst_capture_state_t *s = dp_xcalloc (1, sizeof *s);

  s->horizon   = UINT64_MAX; /* unbounded outside push()'s claim loop */
  s->reps      = reps;
  s->burst_len = burst_len;

  /* ── The composed child, FIRST ──────────────────────────────────────
   * Certified individually; this object owns only the seam around it. It
   * comes first because refine correlates against ITS reference row, the
   * one replica of the preamble in the tree (below). The preamble is not
   * kept here: the engine holds it, at unit RMS. */
  s->acq
      = burst_acq_create (preamble, n, reps, fs, cn0_dbhz, doppler_uncertainty,
                          pfa, pd, noise_mode, doppler_rate);
  if (!s->acq)
    goto fail;

  /* code_period: one acquisition code repetition, in samples. This is the
   * modulus every epoch ambiguity in the design doc is stated against --
   * acq's code_phase is exactly `burst_start mod code_period` (§3.1). */
  s->code_period = n;

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
       past the true start.

       k_hi is `reps`, not the 2 it was. The detecting frame can also START
       before the preamble -- acquisition's framing is not aligned to it, and
       a frame overlapping the preamble by a single period still clears the
       gate on a strong burst -- so the anchor sits up to `coherent_bins - 1`
       periods EARLY. Measured at reps=10 (doppler#1181): the frame that won
       started 3 periods ahead, k_hi = 2 could not reach the truth, and
       refine returned the best it could see -- one period early, with the
       margin of a resolved period. The early direction is the one a demod
       can survive only if told how early; it is not told. */
    s->k_lo        = 3u * reps + 2u;
    s->k_hi        = reps;
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
    size_t cap = next_pow_two (2u * s->retain_span);
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

  /* Refine's per-period correlator. Acquisition has already fixed the code
     phase within a period, so refine needs exactly ONE lag of the
     correlation -- which is `corr2d`'s known-lag mode, a plain O(P) sum
     with no transform in either direction. The replica is the ENGINE's
     reference row -- the preamble's samples at unit RMS -- so there is one
     replica of the preamble per capture (doppler#1470). This used to expand
     PN chips a second time, by its own rule, free to drift from the one
     acquisition correlates against, and unable to describe anything but a
     code. */
  /* One row of cells per candidate preamble POSITION, not per sample: the
     candidates are anchor + k*P, so the whole search is (k_lo+k_hi+reps)
     code periods, each evaluated at the Doppler cells refine needs. */
  s->corr_len = s->k_lo + s->k_hi + reps + 1u;
  /* + BURST_CAPTURE_EDGE_TWINS: the cells at the native span's edge are
     scored at BOTH aliases (see burst_capture_refine). */
  s->max_cells = 2u * ((reps * BURST_CAPTURE_REFINE_INTERP + 1u) / 2u) + 3u
                 + BURST_CAPTURE_EDGE_TWINS;
  s->cell_buf  = dp_xmalloc (s->corr_len * s->max_cells * sizeof *s->cell_buf);

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
burst_capture_create (const float _Complex *preamble, size_t preamble_len,
                      size_t burst_len, size_t reps, double fs,
                      double cn0_dbhz, double doppler_uncertainty, double pfa,
                      double pd, int noise_mode, double doppler_rate)
{
  return burst_capture_create_impl (NULL, preamble, preamble_len, burst_len,
                                    reps, fs, cn0_dbhz, doppler_uncertainty,
                                    pfa, pd, noise_mode, doppler_rate);
}

burst_capture_state_t *
burst_capture_create_backed (const char *path, const float _Complex *preamble,
                             size_t preamble_len, size_t burst_len,
                             size_t reps, double fs, double cn0_dbhz,
                             double doppler_uncertainty, double pfa, double pd,
                             int noise_mode, double doppler_rate)
{
  if (!path || !*path)
    return NULL;
  return burst_capture_create_impl (path, preamble, preamble_len, burst_len,
                                    reps, fs, cn0_dbhz, doppler_uncertainty,
                                    pfa, pd, noise_mode, doppler_rate);
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
  free (state->cell_buf);
  free (state->q);
  free (state->win);
  free (state->released);
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
  state->suppress_base  = 0;
  state->horizon        = UINT64_MAX;
  state->preamble_start = 0;
  state->doppler_hz_est = 0.0;
  state->doppler_res_hz = 0.0;
  state->cn0_dbhz_est   = 0.0;
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
  /* Arrived means written AND not past the horizon: inside push()'s claim
     loop, samples later than the detection being claimed have not arrived
     yet as far as stream time is concerned (doppler#1527). */
  const uint64_t end = s->hist->head < s->horizon ? s->hist->head : s->horizon;
  return pos >= s->hist->tail && pos + n <= end;
}

/** @brief Correlation of one code period of preamble at @p pos, WITH phase.
 *
 * The magnitude used to be taken here, which threw away the one thing that
 * lets the repetitions be combined coherently (doppler#1312).
 */
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
 * **Acquisition's statistic, at the settled cell.** Acquisition has
 * already settled the code phase and the Doppler; what is left is the
 * period. So each candidate is scored by the statistic acquisition itself
 * would compute for a preamble starting there -- acq_cell_corr() of every
 * period the preamble would occupy, mixed at one Doppler on one time
 * reference and summed coherently over the `reps` periods -- and the
 * strongest candidate wins. Only `reps - abs(k)` of the positions still land
 * on preamble when a candidate is `k` periods off, so the score peaks at the
 * truth.
 *
 * Settled to the ENGINE's bin, not to refine's: an engine at depth D knows
 * the Doppler to 1/(D*P), and combining `reps` periods coherently needs
 * 1/(reps*P). So the cells are the depth-`reps` Doppler grid inside the
 * detecting engine's bin, BURST_CAPTURE_REFINE_INTERP per native bin, plus
 * one each side. Nothing past the engine's bin is searched -- acquisition
 * has ruled it out: measured, the whole native span chose the same period
 * as the engine's bin to within one trial in 656. Because the mixer runs
 * INSIDE each period, the intra-period rotation is removed too, which the
 * slow-time transform this replaces could not do: it rotated between periods
 * only, and measured on Zadoff-Chu 127 x 8 it chose the wrong period for 51 of
 * 656 engine hits at D = 5 (doppler#1502).
 *
 * **Coherent, with the Doppler search that makes it survivable.** Summing
 * the magnitudes instead sheds the combining loss as the preamble deepens:
 * coherent gains about `10*log10(reps)` where non-coherent gains
 * `5*log10(reps)`, and swept at 300 trials a point (255 chips, spc 2,
 * residual half a slow-time bin) the correct-repetition rate at 39 dB-Hz
 * went 0.59 -> 0.70 at reps 5, 0.60 -> 0.81 at 10 and 0.54 -> 0.80 at 16.
 *
 * A fixed-phase coherent sum really does collapse under the residual
 * acquisition leaves -- measured, a quarter of a Doppler bin put that peak
 * two whole periods off, the true position 639x below it. The slow-time
 * transform is what removes that failure rather than tolerating it: the
 * unambiguous span across the repetitions is `+-1/(2*P)` and acquisition
 * leaves at most half its own bin, `1/(2*D*P)`, so the search covers the
 * residual BY CONSTRUCTION -- there is no hypothesis range to choose and
 * nothing to tune (doppler#1312). It improves WITH residual rather than
 * degrading: at reps 5, 42 dB-Hz, 0.84 -> 0.93 at a quarter bin.
 *
 * @param s      Capture.
 * @param anchor Coarse code epoch from the hit.
 * @param doppler_hz The detecting engine's Doppler estimate, Hz.
 * @param start  Written with the refined stream-absolute preamble start.
 * @param score  Written with that start's power, |sum over reps|^2 at its
 *               best Doppler cell, so burst_capture_refine_phases() can
 *               compare one phase against another.
 * @return Non-zero on success; 0 if the search window is not yet reachable,
 *         in which case the caller must try again rather than drop the hit.
 */
static int
burst_capture_refine (burst_capture_state_t *s, uint64_t anchor,
                      double doppler_hz, uint64_t *start, double *score)
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

  /* The Doppler cells: the depth-`reps` grid (fine = fs / (P * reps *
     ACQ_DOPPLER_INTERP)) across the detecting engine's bin (fs / (P * D))
     centred on its estimate, one cell of slack each side. */
  const acq_state_t *eng  = s->acq->engine;
  const size_t       IP   = BURST_CAPTURE_REFINE_INTERP;
  const double       fine = eng->fs / ((double)P * (double)reps * (double)IP);
  const size_t       half
      = (reps * IP + 2u * eng->coherent_bins - 1u) / (2u * eng->coherent_bins)
        + 1u;
  size_t cells = 2u * half + 1u;
  if (cells > s->max_cells - BURST_CAPTURE_EDGE_TWINS)
    cells = s->max_cells - BURST_CAPTURE_EDGE_TWINS;
  const long hc = (long)(cells / 2u);

  /* Every candidate POSITION's period, at every cell, computed once; each
     candidate sums `reps` consecutive rows of one cell. */
  size_t n_pos = n_cand + reps - 1u;
  if (n_pos > s->corr_len)
    n_pos = s->corr_len;
  /* Each cell is wrapped into the native span +-fs/(2P). The engine's
     slow-time axis is periodic in the epoch rate fs/P -- at an even depth
     its Nyquist bin reads -fs/(2P) for a carrier at +fs/(2P) -- and between
     epochs the two are one frequency. Mixed WITHIN each epoch they are a
     whole span apart, and the carrier is the one inside the span: unwrapped,
     every even depth centred on the wrong alias (D = 2 lost 0.13 of Pd). */
  /* At the span's EDGE the two aliases are one frequency between epochs
     but a whole span apart within one, and wrapping keeps only one of
     them: a carrier at +0.99 of the half-span has its nearest cell wrapped
     to -span/2. For most preambles that costs a sliver of Pd; for a
     Zadoff-Chu preamble it hands the cell to the RIDGE hypothesis -- the
     same energy u^-1 samples along -- which then outscores the true one
     (doppler#1519). So each cell within 1.5 fine steps of the edge is
     scored at its other alias too, and the finite preamble decides. */
  const double span = eng->fs / (double)P;
  /* The fold is centred on the TILE the hit came from. A native search has
     one, at 0; a tiled one (doppler_uncertainty past the native span) puts
     each window at a multiple of the span, and folding about 0 mixed a
     burst in any other window at the wrong frequency (doppler#1512). */
  const double c
      = eng->window_bins > 1 ? span * floor (doppler_hz / span + 0.5) : 0.0;
  double twin[BURST_CAPTURE_EDGE_TWINS];
  size_t n_twin = 0;
  for (size_t j = 0; j < cells; j++)
    {
      double f = doppler_hz + (double)((long)j - hc) * fine - c;
      f -= span * floor (f / span + 0.5);
      f += c;
      if (fabs (f - c) >= 0.5 * span - 1.5 * fine
          && n_twin < BURST_CAPTURE_EDGE_TWINS)
        twin[n_twin++] = f - copysign (span, f - c);
    }
  const size_t nf = cells + n_twin;
  for (size_t j = 0; j < nf; j++)
    {
      double f;
      if (j < cells)
        {
          f = doppler_hz + (double)((long)j - hc) * fine - c;
          f -= span * floor (f / span + 0.5);
          f += c;
        }
      else
        f = twin[j - cells];
      for (size_t i = 0; i < n_pos; i++)
        s->cell_buf[i * nf + j] = (float _Complex)acq_cell_corr (
            eng, burst_capture_at (s, lo + (uint64_t)(i * P)), 0, f,
            (double)(i * P));
    }

  double best   = -1.0;
  size_t best_k = 0;
  for (size_t k = 0; k + reps <= n_pos; k++)
    for (size_t j = 0; j < nf; j++)
      {
        double _Complex acc = 0.0;
        for (size_t r = 0; r < reps; r++)
          acc += (double _Complex)s->cell_buf[(k + r) * nf + j];
        const double pk
            = creal (acc) * creal (acc) + cimag (acc) * cimag (acc);
        if (pk > best)
          {
            best   = pk;
            best_k = k;
          }
      }

  *start = lo + (uint64_t)(best_k * P);
  *score = best;
  return 1;
}

/** @brief Record `epoch`'s code phase on `e` if it is new: returns non-zero
 *         when it was. Phases within 2 samples are one phase -- a continuous
 *         delay straddles two samples -- and a full set drops the newcomer,
 *         leaving refine as it was before phases were kept. */
static int
burst_capture_note_phase (const burst_capture_state_t *s,
                          burst_capture_pending_t *e, uint64_t epoch)
{
  const uint32_t P  = (uint32_t)s->code_period;
  const uint32_t ph = (uint32_t)(epoch % (uint64_t)P);
  for (uint32_t i = 0; i < e->n_phase; i++)
    {
      uint32_t d = ph > e->phase[i] ? ph - e->phase[i] : e->phase[i] - ph;
      if (d > P - d)
        d = P - d;
      if (d <= 2u)
        return 0;
    }
  if (e->n_phase >= BURST_CAPTURE_MAX_PHASES)
    return 0;
  e->phase[e->n_phase++] = ph;
  return 1;
}

/** @brief Refine `e` at every code phase its detections carried, and keep
 *         the best-scoring start (doppler#1519).
 *
 * Each phase is scored on the ANCHOR's grid, shifted by the phase's offset
 * folded into (-P/2, P/2], so no phase reaches further than half a period
 * beyond what the anchor's refine does: the history trim keeps that half
 * period. A phase whose window has not fully arrived holds the whole
 * refine, exactly as the anchor's own does, so the answer never depends on
 * how the stream was blocked.
 *
 * @return Non-zero once refined; 0 to retry after more samples arrive.
 */
static int
burst_capture_refine_phases (burst_capture_state_t   *s,
                             burst_capture_pending_t *e)
{
  const int64_t  P    = (int64_t)s->code_period;
  const int64_t  base = (int64_t)(e->anchor % (uint64_t)P);
  double         best = -1.0;
  uint64_t       pick = 0;
  const uint32_t n    = e->n_phase ? e->n_phase : 1u;
  for (uint32_t i = 0; i < n; i++)
    {
      int64_t d = e->n_phase ? (int64_t)e->phase[i] - base : 0;
      d -= P * (int64_t)floor ((double)d / (double)P + 0.5);
      if ((int64_t)e->anchor + d < 0)
        continue;
      uint64_t start = 0;
      double   score = 0.0;
      if (!burst_capture_refine (s, (uint64_t)((int64_t)e->anchor + d),
                                 e->doppler_hz, &start, &score))
        return 0;
      if (score > best)
        {
          best = score;
          pick = start;
        }
    }
  e->start = pick;
  return 1;
}

/** @brief Release history no stage can still need. */
static void
burst_capture_trim (burst_capture_state_t *s)
{
  uint64_t head = s->hist->head;
  uint64_t keep
      = head > (uint64_t)s->retain_span ? head - (uint64_t)s->retain_span : 0;
  /* One period beyond the anchor's reach: refine scores each remembered
     phase on the anchor's grid shifted by up to half a period. */
  const uint64_t back = (uint64_t)((s->k_lo + 1u) * s->code_period);
  /* History is held for the oldest entry that can still be EMITTED -- not
     for a shadowed one, which waits for a verdict and would otherwise pin
     the ring for as long as a push lasts (doppler#1527). */
  for (size_t j = 0; j < s->pending; j++)
    {
      const burst_capture_pending_t *o = &s->q[(s->q_head + j) % s->q_cap];
      if (o->shadowed)
        continue;
      uint64_t base = o->refined ? o->start : o->anchor;
      uint64_t need = base > back ? base - back : 0;
      if (need < keep)
        keep = need;
    }
  if (keep > s->hist->tail)
    dp_f32_consume (s->hist, (size_t)(keep - s->hist->tail));

  /* ...and a shadowed entry whose history is now gone can never be refined,
     so a later release could not bring it back: it goes, as it would have
     at the next push. Only a push long enough to trim past it gets here. */
  size_t kept = 0;
  for (size_t j = 0; j < s->pending; j++)
    {
      burst_capture_pending_t *o    = &s->q[(s->q_head + j) % s->q_cap];
      uint64_t                 base = o->refined ? o->start : o->anchor;
      if (o->shadowed && (base > back ? base - back : 0) < s->hist->tail)
        continue;
      s->q[(s->q_head + kept) % s->q_cap] = *o;
      kept++;
    }
  s->pending = kept;
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
  /* The first entry NOT held. A held (shadowed) one waits for a verdict
     only the consumer can give, between pushes (burst_capture_release());
     it must not stall every burst queued behind it. A long burst makes the
     stall a loss: a complete window waiting behind a held head keeps the
     history tail pinned, the ring refuses the next chunk, and a whole-capture
     push of four 41548-sample bursts dropped 52596 samples and the fourth
     burst, where 1000-sample blocks lost nothing (doppler#1534). */
  size_t j = 0;
  while (j < s->pending && s->q[(s->q_head + j) % s->q_cap].shadowed)
    j++;
  if (j == s->pending)
    return 0;
  if (j)
    {
      burst_capture_pending_t  tmp = s->q[s->q_head];
      burst_capture_pending_t *hj  = &s->q[(s->q_head + j) % s->q_cap];
      s->q[s->q_head]              = *hj;
      *hj                          = tmp;
    }
  burst_capture_pending_t *e = &s->q[s->q_head];

  if (!e->refined)
    {
      if (!burst_capture_refine_phases (s, e))
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
      size_t cap  = s->ev_cap ? s->ev_cap * 2u : 8u;
      s->ev       = dp_xrealloc (s->ev, cap * sizeof *s->ev);
      s->released = dp_xrealloc (s->released, cap * sizeof *s->released);
      s->ev_cap   = cap;
    }
  s->released[s->ev_len] = 0;

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
     are SHADOWED rather than dropped: a consumer whose verdict on this window
     is "not a burst" gives them back with release(), and the next push drops
     whatever is still shadowed (doppler#1181). */
  {
    uint64_t until = e->start + (uint64_t)s->burst_len;
    if (until > s->suppress_until)
      s->suppress_until = until;
    for (size_t j = 0; j < s->pending; j++)
      {
        burst_capture_pending_t *cand = &s->q[(s->q_head + j) % s->q_cap];
        if (cand->anchor < s->suppress_until)
          cand->shadowed = 1;
      }
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
burst_capture_push (burst_capture_state_t *state, const float _Complex *x,
                    size_t x_len, float _Complex *out, size_t max_out)
{
  /* Every call starts fresh: both lists describe THIS push. */
  state->ev_len  = 0;
  state->det_len = 0;
  /* `pending` is NOT cleared here. It is the live queue length -- detections
     whose burst window has not arrived -- and it must survive across pushes,
     because surviving across pushes is the whole point of holding them. */

  /* What the previous push SHADOWED goes now, unless the consumer released
     the window that shadowed it between the two calls (doppler#1181). Held
     rather than dropped at the time so that a consumer with a verdict this
     object cannot reach -- error detection, in whatever form its frame
     carries it -- can give a decoy's span back; dropped here so a consumer
     with no verdict gets exactly the behaviour a bare capture always had. */
  {
    size_t keep = 0;
    for (size_t j = 0; j < state->pending; j++)
      {
        burst_capture_pending_t *cand
            = &state->q[(state->q_head + j) % state->q_cap];
        if (cand->shadowed)
          continue;
        state->q[(state->q_head + keep) % state->q_cap] = *cand;
        keep++;
      }
    state->pending = keep;
  }
  state->suppress_base = state->suppress_until;

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
              /* STREAM TIME FIRST: emit every burst whose window was complete
                 by the time this detection was made, before the detection
                 is claimed. Without it a whole-capture push claimed every
                 detection of a chunk before draining once, so a burst that a
                 small-block caller had already been handed was still pending
                 -- and a later burst within refine_span MERGED into it: four
                 bursts at a quarter of min_gap came out as one wrong window
                 pushed whole and as four exact ones in 1000-sample blocks
                 (doppler#1527). */
              state->horizon = hits[i].samples_consumed;
              burst_capture_drain (state);
              state->horizon = UINT64_MAX;

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
                d->doppler_hz = acq_bin_doppler_hz (e, hits[i].doppler_bin);
                d->cn0_dbhz   = hits[i].cn0_dbhz_est;
                d->test_stat  = (double)hits[i].test_stat;
                d->peak_mag   = (double)hits[i].peak_mag;
              }

              /* Inside a burst already CAPTURED: acquisition fires on the
                 payload too, and those are not new bursts -- unless the
                 window that owns this span turns out not to be a burst, which
                 only a consumer can know. So the hit is queued SHADOWED: not
                 emitted, dropped at the next push, given back by release(). */
              const int shadow = epoch < state->suppress_until;

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
                  /* A phase refine has not scored re-arms it, the same way
                     a stronger anchor does below (doppler#1519). */
                  if (burst_capture_note_phase (state, cand, epoch))
                    {
                      cand->refined = 0;
                      cand->start   = 0;
                    }
                  if ((double)hits[i].peak_mag > cand->peak_mag)
                    {
                      /* The anchor moves, so whatever refine concluded from
                         the old one is stale -- clear `refined` and let it run
                         again rather than pairing a new anchor with an old
                         start. */
                      cand->anchor   = epoch;
                      cand->peak_mag = (double)hits[i].peak_mag;
                      cand->start    = 0;
                      cand->shadowed = shadow;
                      cand->refined  = 0;
                      cand->doppler_hz
                          = acq_bin_doppler_hz (e, hits[i].doppler_bin);
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
              q->anchor  = epoch;
              q->n_phase = 0;
              (void)burst_capture_note_phase (state, q, epoch);
              q->start      = 0;
              q->peak_mag   = (double)hits[i].peak_mag;
              q->refined    = 0;
              q->shadowed   = shadow;
              q->doppler_hz = acq_bin_doppler_hz (e, hits[i].doppler_bin);
              q->cn0_dbhz   = hits[i].cn0_dbhz_est;
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

const float _Complex *
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
  /* A grid refine cannot reach is refused HERE, before the engine sees it.
     acq stamps a hit at the end of the LAST of n_noncoh accumulated frames,
     so the preamble can sit up to n_noncoh*doppler_bins code periods before
     the anchor; refine searches k_lo periods back and no further. Past that
     it returns the best candidate it CAN see -- a wrong period with a
     plausible margin, which is worse than a refusal (doppler#1181). The
     engine itself accepts any n_noncoh up to its safety ceiling, because for
     a continuous signal that is a real sensitivity lever; for a capture it
     is a way to lose the burst quietly. */
  if (n_noncoh * doppler_bins > state->k_lo)
    return DP_ERR_INVALID;
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
  /* The ENGINE's, not the last event's mirror of it. The bin width is a
     property of the configured search and a composing bank sizes its
     cross-channel dedup from it at construction -- the mirror read 0.0
     there, before any burst, and a dedup window of zero merged nothing
     (doppler#1174). The event row keeps its own copy per burst. */
  return state->acq->engine->doppler_res_hz;
}

double
burst_capture_get_cn0_dbhz_est (const burst_capture_state_t *state)
{
  return state->cn0_dbhz_est;
}

size_t
burst_capture_get_pending (const burst_capture_state_t *state)
{
  /* The read-back's meaning is "a burst you would lose by stopping now", so
     the shadowed entries -- payload hits inside a window already handed out
     -- are not counted. They are the next push's to drop. */
  size_t n = 0;
  for (size_t j = 0; j < state->pending; j++)
    n += !state->q[(state->q_head + j) % state->q_cap].shadowed;
  return n;
}

int
burst_capture_release (burst_capture_state_t *state, size_t i)
{
  if (i >= state->ev_len)
    return DP_ERR_INVALID;
  state->released[i] = 1;

  /* The span is owned by whatever this push emitted and did NOT release, on
     top of what earlier pushes owned; a released window's span is simply
     no longer part of that. Then every held detection is re-judged against
     the new boundary -- the ones the released window shadowed come back. */
  uint64_t until = state->suppress_base;
  for (size_t k = 0; k < state->ev_len; k++)
    {
      uint64_t end = state->ev[k].preamble_start + (uint64_t)state->burst_len;
      if (!state->released[k] && end > until)
        until = end;
    }
  state->suppress_until = until;
  for (size_t j = 0; j < state->pending; j++)
    {
      burst_capture_pending_t *cand
          = &state->q[(state->q_head + j) % state->q_cap];
      cand->shadowed = cand->anchor < state->suppress_until;
    }
  return DP_OK;
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

double
burst_capture_get_pd_burst (const burst_capture_state_t *state)
{
  return state->acq->engine->pd_burst;
}

double
burst_capture_get_psl_db (const burst_capture_state_t *state)
{
  return acq_psl_db (state->acq->engine);
}

double
burst_capture_get_doppler_rate (const burst_capture_state_t *state)
{
  return state->acq->engine->doppler_rate;
}

size_t
burst_capture_get_doppler_bins (const burst_capture_state_t *state)
{
  /* The grid the search covers, tiles included -- the same number
     BurstAcquisition's doppler_bins reads (doppler#1512). */
  return acq_grid_bins (state->acq->engine);
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
         + sizeof (double) * 3u   /* the event's doubles                    */
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
  s->suppress_until = dp_r_u64 (&_r);
  s->suppress_base  = s->suppress_until;

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

         Unless the file does not hold them. The file holds the span the blob
         names, `[head - n, head)`, in two cases: create() adopted a ring of
         this exact geometry with history in it (`recovered`), or THIS object
         wrote it -- its ring head is its own stream position, so a span
         ending at or before it is in the file in the very bytes it put
         there, which is what lets a live capture restore its own checkpoint
         (doppler#1190: the object that created the file has `recovered`
         zero for ever, and used to refuse every blob it took after a push,
         while a fresh object over the same file accepted them). Either way
         the span must still be inside the ring: past the capacity it has
         been overwritten. A blob that claims retained history the file
         cannot hold is a resume into silence -- the positions would be
         right and the samples zeros or someone else's, so every later burst
         would simply not be found. Refuse it. */
      uint64_t head = s->samples_fed;
      {
        const uint64_t from = head - (uint64_t)n;
        const uint64_t mine = DP_LOAD_ACQ (&s->hist->head);
        const int      have = s->recovered || mine >= head;
        const int      overwritten
            = mine > from && mine - from > (uint64_t)s->hist->capacity;
        if (n && (!have || overwritten))
          return DP_ERR_INVALID;
      }
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
