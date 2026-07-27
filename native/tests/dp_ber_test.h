/**
 * @file dp_ber_test.h
 * @brief Error-rate measurement harness: alignment, sampling, and the CI.
 *
 * **An error rate is a measurement, and this file is its instrument.** Every
 * confidently-wrong receiver number this project has produced came from one of
 * three places, and each one has a gate here. A measurement that has not
 * passed all three is not a result:
 *
 * 1. **Is it SETTLED?** A second-order loop needs ~5/Bn symbols, the two loops
 *    are cascaded so their budgets ADD, and joint tracking DOUBLES the sum —
 *    `2*(5/bn_timing + 5/bn_carrier)`, plus the handover instant again if
 *    `acq_to_track` is on. And the analytic budget alone is not enough: take
 *    `max(budget, timing lock, carrier lock, handover + budget)` from the
 *    receiver's own verify-counted indicators. Measuring inside that window
 *    measures settling and reports it as steady state (measured cost: -9.0 dB
 *    EVM where the settled answer is -23.2 dB; SER 5.9x the bound where the
 *    settled answer is 1.7x). dp_ber_settle() decides this, once.
 *
 * 2. **Have we counted enough ERRORS?** Fix the ERROR count and let the symbol
 *    count fall out (inverse binomial sampling). Then the relative standard
 *    error is `1/sqrt(r)` — a function of the error count ALONE — so the error
 *    target IS the measurement precision. Stopping on a fixed symbol count
 *    makes precision depend on the very rate being measured: 20 000 symbols at
 *    SER 1e-3 yields ~20 errors and ~22% relative error, which reads as real
 *    seed-to-seed variation in the receiver. dp_ber_enough() is the stop
 *    condition; dp_ber_ci() is the interval, and it is EXACT (not a normal
 *    approximation) at every error count including 1.
 *
 * 3. **Does it MAKE SENSE?** Cross-validate against measurements that cannot
 *    fail the same way. The error rate is truth-referenced and needs an
 *    alignment; EVM-against-own-hard-decisions and the blind M2M4 Es/N0 need
 *    neither, and theory needs no measurement at all. dp_ber_report() checks
 *    all four against each other and refuses to call a result OK when they
 *    disagree — including the "too good to be true" direction, which is the
 *    one a passing test never questions.
 *
 * ## The alignment is resolved, never searched
 *
 * The historic footgun is scoring `min over (lag, rotation)` of the error
 * count. That is not a measurement of the receiver, it is an optimisation over
 * the answer, and it fails in both directions: a wide search on a short window
 * finds a lucky low-error alignment on garbage (false PASS), and a narrow one
 * misses the true alignment on a healthy receiver and reports chance (false
 * FLOOR — a committed "~12 dB floor" claim that was really ~5 dB, and an
 * "SER 0.48" on a receiver at 0.0000 that needed lag -34).
 *
 * So dp_ber_sync() *detects* the alignment instead, by correlating against a
 * known marker — a sync word, a PN code period, or (in a simulation, where
 * truth exists) a prefix of the truth sequence itself. That gives:
 *
 *   - the lag and the absolute carrier phase from the correlation peak, with
 *     no reference to the symbols being scored;
 *   - a **false-alarm gate**: the peak must beat a threshold derived from a
 *     Pfa via the canonical detection primitives, Bonferroni-corrected for the
 *     number of lags searched. A marker too short to resolve the alignment
 *     says so (`ok == 0`) instead of returning a plausible wrong lag;
 *   - the marker symbols EXCLUDED from scoring, so the symbols that fixed the
 *     alignment cannot also flatter the rate.
 *
 * A repeating marker is combined non-coherently across its occurrences, which
 * both raises the processing gain and exposes cycle slips (`slips`).
 *
 * ## Reuse, not re-derivation
 *
 * Nothing numeric is invented here. The confidence interval is the exact
 * Gamma/chi-square one, and its quantiles come from `det_threshold()` /
 * `det_threshold_noncoherent()` — doppler's own regularized-incomplete-gamma
 * inverse, already validated in the detection module — rather than a second
 * copy of a series/continued-fraction kernel. Gray coding comes from
 * `mpsk_core.h`. EVM and the blind SNR come from `dp_sym_test.h`. This file
 * owns the coherent M-PSK theory curve so the FIVE hand-rolled copies of
 * `qfunc`/`theory_ser` scattered across the validators and the demos can
 * collapse onto one.
 *
 * ## Linking
 *
 * Header-only, but the detection and snr cores must be on the link line. In
 * `objects/<obj>.toml`, add BARE dependencies (not `link = true` — the test
 * and bench need the symbols, the `.so` does not):
 *
 *     depends_on = [ { name = "detection" }, { name = "snr" }, ... ]
 *
 * The Python twin of this harness is
 * `src/doppler/track/tests/_mpsk_rx_harness.py` (`settle_floor`,
 * `lock_symbol`, `settle_from`, `coherent_errors`, `ser_confidence`); keep the
 * two in step.
 */
#ifndef DP_BER_TEST_H
#define DP_BER_TEST_H

#include "ber/ber_core.h"             /* the measurement primitives, once  */
#include "ber_meter/ber_meter_core.h" /* the accumulator + the detector    */
#include "dp_sym_test.h"              /* EVM / M2M4 / the settling budget    */
#include "mpsk/mpsk_core.h"           /* mpsk_bps, mpsk_phi0, gray encode    */
#include <complex.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

/** @brief Default operating point: the SER a characterisation anchors at.
 *
 * Anchoring at a fixed error rate rather than a fixed Es/N0 asks "does the
 * receiver meet its bound" at the SAME place on the curve for every
 * constellation (per-M that is 6.8 / 10.3 / 15.7 dB for BPSK / QPSK / 8PSK).
 * It is also where the squaring loss is comparable across M, which a fixed
 * Es/N0 is not. */
#define DP_BER_TARGET_SER 1e-3

/** @brief Default error target: ~7% relative standard error (`1/sqrt(r)`),
 * i.e. about +-14% at 95% and +-18% at 99%. Raise it for a tighter gate; the
 * cost is linear in run time and the precision is `1/sqrt` of it. */
#define DP_BER_TARGET_ERRORS 200uL

/** @brief Default two-sided confidence level for the reported interval. */
#define DP_BER_CONF 0.99

/** @brief Default lag search half-width, symbols.
 *
 * Generous on purpose. Group delay varies a lot with pulse shape, front end
 * and rate — RRC on the complex path needed lag -34 — and a clipped search
 * reported SER 0.48 on a receiver running at 0.0000. dp_ber_sync() reports
 * `saturated` when the peak lands on either end, so a too-small span is
 * visible rather than silent. */
#define DP_BER_LAG_SPAN 200L

/** @brief Default marker length when aligning on a prefix of the truth.
 *
 * 256 symbols gives `sqrt(2*256) = 22.6` of detection statistic against a
 * threshold near 6.5 for a 1e-6 Pfa over 401 lags — roughly 11 dB of margin,
 * so the alignment is decided, not guessed. */
#define DP_BER_SYNC_SYMS 256u

/** @brief Whole-experiment false-alarm probability for the alignment gate. */
#define DP_BER_SYNC_PFA 1e-6

/** @brief Largest lag search the fixed-size scratch supports. */
#define DP_BER_MAX_LAGS 2048

/* --- 1. Theory - the coherent M-PSK bound, in one place ------------------ */

/** @brief Gaussian tail `Q(x) = P(N(0,1) > x)`. */
static inline double
dp_ber_qfunc (double x)
{
  return ber_qfunc (x);
}

/**
 * @brief Coherent M-PSK symbol error rate at matched-filter Es/N0 (LINEAR).
 *
 * `BPSK: Q(sqrt(2 Es/N0))`, `QPSK: 2 Q(sqrt(Es/N0))`,
 * `8PSK: 2 Q(sqrt(2 Es/N0) sin(pi/8))`. The QPSK and 8PSK forms are the
 * standard nearest-neighbour union bound, tight to well under a percent at any
 * Es/N0 worth testing at.
 *
 * **This is a COHERENT bound.** A differentially-decoded error rate is ~2x it,
 * because a differential decision fails when either of its two symbols is
 * wrong (measured 1.88-2.11 across M and both receiver paths). Pairing a
 * differential measurement with this curve invents a factor of two of
 * "implementation loss" — it cost a session of chasing 2-4.75x where the
 * coherent measurement is 1.2-2.4x.
 */
static inline double
dp_ber_theory_ser (int m, double esn0)
{
  return ber_theory_ser (m, esn0);
}

/**
 * @brief Coherent GRAY-coded M-PSK bit error rate at Es/N0 (LINEAR).
 *
 * BPSK and Gray QPSK are exactly `Q(sqrt(2 Eb/N0))` — the same curve per bit,
 * which is the whole point of Gray QPSK. 8PSK uses `SER / log2(M)`, exact in
 * the high-Es/N0 limit where a symbol error lands on a neighbour and flips one
 * bit.
 */
static inline double
dp_ber_theory_ber (int m, double esn0)
{
  return ber_theory_ber (m, esn0);
}

/**
 * @brief Es/N0 (dB) at which the coherent bound equals @p ser.
 *
 * Bisects dp_ber_theory_ser(), which is monotone decreasing. This is how an
 * implementation loss is quoted honestly: convert the MEASURED rate to the
 * Es/N0 that theory would need to produce it, and subtract. A loss in dB is
 * comparable across M and across operating points; a rate ratio is not.
 *
 * Returns 40.0 for a rate below anything reachable, -10.0 for one above.
 */
static inline double
dp_ber_esn0_db_for_ser (int m, double ser)
{
  return ber_esn0_db_for_ser (m, ser);
}

/* --- 2. Alignment - detected from a marker, never searched --------------- */

/**
 * @brief A known pattern in the transmitted sequence, used to fix alignment.
 *
 * The pattern is given in SYMBOL INDICES (0..m-1), the same alphabet as the
 * truth array — a sync word, one period of a spreading/PN code mapped to
 * symbols, or, when `sym` is NULL, a slice of the truth sequence itself. The
 * last case is the normal one in a simulation: truth is known, so a stretch of
 * it IS a valid marker, and using a stretch DISJOINT from the scored window
 * gives a rigorous, Pfa-gated alignment at no cost.
 */
typedef struct
{
  /** Marker symbols, or NULL to use `truth[t0 .. t0+n)` as the marker. */
  const uint8_t *sym;
  /** Marker length in symbols. 0 selects DP_BER_SYNC_SYMS with `sym = NULL`.
   */
  size_t n;
  /** Truth index at which the first occurrence starts. */
  size_t t0;
  /** Repeat period in symbols; 0 for a single occurrence (a preamble). */
  size_t period;
  /** Cap on occurrences used; 0 for as many as the record holds. */
  size_t reps;
} dp_ber_marker_t;

/** @brief Outcome of dp_ber_sync(): where the stream sits, and how sure. */
typedef struct
{
  /** Alignment: `rx[i]` carries the transmitted symbol `truth[i + lag]`. */
  long lag;
  /** Absolute residual constellation rotation (rad), from the peak's phase. */
  double phase;
  /** Non-coherent detection statistic at the peak, in threshold units. */
  double stat;
  /** Pfa-derived threshold it had to beat (Bonferroni over the lags). */
  double threshold;
  /** `20*log10(stat/threshold)` — headroom. Negative means not detected. */
  double margin_db;
  /** `10*log10(peak/runner-up)` outside the guard band; ambiguity check. */
  double runner_db;
  /** Marker occurrences combined. */
  size_t occurrences;
  /** Occurrences whose phase disagreed with the peak's by > pi/m: slips. */
  size_t slips;
  /** 1 when the peak sat on an end of the lag search — the span is too small.
   */
  int saturated;
  /** 1 when the alignment is trustworthy: detected, unambiguous, unsaturated.
   */
  int ok;
  /** First failing gate, for the failure message. */
  const char *why;
} dp_ber_sync_t;

/**
 * @brief Detect the (lag, phase) alignment of @p rx by correlating a marker.
 *
 * For each candidate lag the marker is correlated against the recovered
 * symbols; occurrences of a repeating marker are combined NON-COHERENTLY (so
 * a slow phase drift between them cannot cancel the peak), which is exactly
 * the statistic `marcum_q(K, 0, .)` describes and
 * `det_threshold_noncoherent()` inverts. The noise floor is estimated from the
 * off-peak lags themselves — a CFAR reference, so no knowledge of the Es/N0 is
 * needed and the partial correlation of the marker against random data (which
 * is what a false alarm actually looks like) is the reference, as it should
 * be.
 *
 * The peak's phase is the ABSOLUTE constellation rotation, so there is no
 * residual M-fold ambiguity to search: the marker resolves it. That is what
 * removes the `min over rotation` bias from the error count.
 *
 * The processing gain is `sqrt(2 * K * L)` in statistic units, so a marker
 * that is too short simply cannot clear the threshold — and then `ok` is 0 and
 * `why` says so, rather than a plausible wrong lag being returned. That is the
 * intended behaviour: a 16-symbol marker over a 401-lag search genuinely does
 * not identify an alignment at a 1e-6 false-alarm probability.
 *
 * @param rx        Recovered symbols.
 * @param n_rx      How many.
 * @param truth     Transmitted symbol indices (0..m-1).
 * @param n_truth   How many.
 * @param mk        Marker description; NULL uses the defaults at `t0 = 0`.
 * @param m         Constellation order.
 * @param lag_span  Search half-width in symbols (DP_BER_LAG_SPAN is sane).
 * @param pfa       Whole-search false-alarm probability (DP_BER_SYNC_PFA).
 * @return          The alignment, with `ok` telling you whether to believe it.
 */
static inline dp_ber_sync_t
dp_ber_sync (const float complex *rx, size_t n_rx, const uint8_t *truth,
             size_t n_truth, const dp_ber_marker_t *mk, int m, long lag_span,
             double pfa)
{
  dp_ber_marker_t def = { NULL, DP_BER_SYNC_SYMS, 0, 0, 0 };
  dp_ber_sync_t   o;
  ber_align_t     a;
  if (!mk)
    mk = &def;
  a       = ber_align_detect (rx, n_rx, mk->sym ? mk->sym : truth, n_truth, m,
                              mk->t0, mk->n, mk->period, (int)lag_span, pfa);
  o.lag   = a.lag;
  o.phase = a.phase;
  o.stat  = a.stat;
  o.threshold   = a.threshold;
  o.margin_db   = a.margin_db;
  o.runner_db   = a.runner_db;
  o.occurrences = a.occurrences;
  o.slips       = a.slips;
  o.saturated   = a.saturated;
  o.ok          = a.ok;
  o.why         = a.ok          ? "aligned"
                  : a.saturated ? "peak on the edge of the lag search"
                  : (a.stat < a.threshold)
                      ? "no marker detection: raise the marker length or Es/N0"
                      : "ambiguous peak: a runner-up lag is within 3 dB";
  return o;
}

/** @brief 1 when truth index @p t is covered by a marker occurrence.
 *
 * Marker symbols are KNOWN, so scoring them would flatter the error rate with
 * symbols that had no chance of being wrong — and, in the blind case, with the
 * very symbols that fixed the alignment. dp_ber_score() excludes them. */

/* --- 3. The settled window ----------------------------------------------- */

/**
 * @brief First symbol from which a verify-counted flag is SUSTAINED.
 *
 * "Sustained" is @p sustain consecutive symbols high AND at least @p min_frac
 * of everything after that point high too. Both halves carry weight: the run
 * rejects a single lucky decision, and the fraction rejects a detector that
 * declares early and then flaps for the rest of the burst.
 *
 * Dating the lock by the FINAL contiguous run of ones instead is right with no
 * noise and badly wrong with it — a verify-counted detector legitimately dips
 * under AWGN, and one late dip once moved a reported lock from 415 to 2286,
 * which left no measurement window at all and read as a receiver that never
 * locked.
 *
 * @return The symbol index, or -1 when no such point exists — the honest
 *         answer for "never locked", which forces the caller to say so rather
 *         than quietly measuring a transient.
 */
static inline long
dp_ber_lock_symbol (const unsigned char *flag, size_t n, size_t sustain,
                    double min_frac)
{
  return ber_lock_symbol (flag, n, sustain, min_frac);
}

/**
 * @brief Where a steady-state measurement may start: `max` of every budget.
 *
 * The analytic budget (dp_test_settle_syms(), `2*(5/bn_t + 5/bn_c)`) and the
 * receiver's own lock indicators are both fallible in the SAME direction, so
 * take whichever settles last. The budget can be optimistic when a geometry
 * converges slowly for reasons bandwidth does not capture (at `sps=10,
 * m_out=4` the timing detector does not declare until symbol 1063); the locks
 * can be optimistic because a detector declares on a statistic that crossed a
 * threshold, not on a settled loop.
 *
 * **A handover settles last of all.** With `acq_to_track` on it fires on
 * carrier lock plus a warmup — strictly after the budget and after every lock
 * indicator — and the decision-directed loop then has its own transient. So a
 * handover contributes `its instant + the budget again`. Measured on 8PSK at
 * its SER=1e-3 anchor: handover at symbol 2525 against a 2000-symbol budget,
 * SER 5.95x the coherent bound measured from 2000 and 1.68x from 4525, with
 * essentially every error in the one pre-handover block.
 *
 * Pass NULL for any indicator the receiver does not publish. Pass a loop's
 * `bn` as 0 if it is not running.
 *
 * @param bn_timing    Timing loop noise bandwidth per symbol (0 if none).
 * @param bn_carrier   Carrier loop noise bandwidth per symbol (0 if none).
 * @param lock_timing  Per-symbol timing lock flag, or NULL.
 * @param lock_carrier Per-symbol carrier lock flag, or NULL.
 * @param tracking     Per-symbol handover flag, or NULL.
 * @param n            Length of whichever flag arrays were passed.
 * @param ok           Out: 0 when a supplied indicator never sustained lock,
 *                     meaning there is NO valid steady-state window. May be
 *                     NULL.
 * @return             First symbol of the measurement window.
 */
static inline size_t
dp_ber_settle (double bn_timing, double bn_carrier,
               const unsigned char *lock_timing,
               const unsigned char *lock_carrier,
               const unsigned char *tracking, size_t n, int *ok)
{
  size_t budget = ber_settle_syms (bn_timing, bn_carrier);
  /* An indicator the receiver does not publish is "not required", which is
     what ber_settle_from() reads a -1 as for the handover. For timing and
     carrier a -1 means the loop never locked, so distinguish "absent" from
     "never locked" here and let the core own the max/handover policy. */
  int t = lock_timing ? (int)ber_lock_symbol (lock_timing, n, 200, 0.9) : 0;
  int c = lock_carrier ? (int)ber_lock_symbol (lock_carrier, n, 200, 0.9) : 0;
  int h = tracking ? (int)ber_lock_symbol (tracking, n, 200, 0.9) : -1;
  if (ok)
    *ok = (t >= 0 && c >= 0);
  return ber_settle_from (budget, t, c, h);
}

/* --- 4. Counting - inverse binomial sampling ----------------------------- */

/**
 * @brief Test-side handle on a `ber_meter` plus mirrors of its counters.
 *
 * Owns NO counting logic: every symbol is scored by ber_meter_score() in the
 * library, and the fields below are refreshed from it so existing call sites
 * can keep reading `acc.errors`. Call dp_ber_free() when done.
 */
typedef struct
{
  ber_meter_state_t *meter;  /**< the one implementation                */
  int                m, bps; /**< mirrors of its configuration          */
  unsigned long      target_errors;
  unsigned long      errors, symbols, bit_errors, bits, skipped, bursts;
  double evm_sum, m2m4_sum; /**< test-side cross-checks               */
} dp_ber_t;

/** @brief Refresh the mirrors from the meter. */
static inline void
dp_ber_refresh (dp_ber_t *b)
{
  b->errors     = (unsigned long)ber_meter_get_errors (b->meter);
  b->symbols    = (unsigned long)ber_meter_get_symbols (b->meter);
  b->bit_errors = (unsigned long)ber_meter_get_bit_errors (b->meter);
  b->bits       = (unsigned long)ber_meter_get_bits (b->meter);
  b->skipped    = (unsigned long)ber_meter_get_skipped (b->meter);
}

/** @brief Accumulate the two truth-free cross-checks over `[lo, hi)` — the
 *  SAME window the error rate used. Composition over library primitives, not
 *  a second implementation of either. */
static inline int
dp_ber_evm_m2m4 (dp_ber_t *b, const float complex *rx, size_t lo, size_t hi)
{
  if (hi <= lo || hi - lo < 20)
    return 0;
  b->evm_sum += ber_evm_db (rx, hi, lo, hi, b->m);
  b->m2m4_sum += snr_m2m4_db (rx + lo, hi - lo);
  return 1;
}

/** @brief Reset an accumulator for constellation @p m and an error target. */
static inline void
dp_ber_init (dp_ber_t *b, int m, unsigned long target_errors)
{
  b->m             = (m < 2) ? 2 : m;
  b->bps           = mpsk_bps (b->m);
  b->target_errors = target_errors ? target_errors : DP_BER_TARGET_ERRORS;
  b->meter         = ber_meter_create (b->m, b->target_errors, DP_BER_CONF);
  b->errors = b->symbols = b->bit_errors = b->bits = 0;
  b->skipped = b->bursts = 0;
  b->evm_sum = b->m2m4_sum = 0.0;
}

/** @brief Release the meter behind a dp_ber_t. */
static inline void
dp_ber_free (dp_ber_t *b)
{
  ber_meter_destroy (b->meter);
  b->meter = NULL;
}

/**
 * @brief The stop condition: have we counted enough ERRORS yet?
 *
 * This is the whole of inverse binomial sampling. Loop bursts (or seeds) until
 * this returns 1, and the precision of the result is fixed by the error target
 * alone — `1/sqrt(r)` relative — instead of depending on the rate you are
 * trying to measure. Guard the loop with a symbol budget as well, or a
 * receiver that is working perfectly will run forever.
 */
static inline int
dp_ber_enough (const dp_ber_t *b)
{
  return ber_meter_get_enough (b->meter);
}

/**
 * @brief Score `rx[lo .. hi)` against the truth and accumulate.
 *
 * Uses the alignment from dp_ber_sync() verbatim — no lag search, no rotation
 * search, no minimisation of any kind over the answer. Symbols are de-rotated
 * by the marker-derived phase, sliced, and compared; bit errors come from the
 * canonical Gray mapping in `mpsk_core.h`, so they match what
 * `mpsk_receiver_steps_bits()` would emit.
 *
 * Symbols covered by a marker occurrence are excluded (see
 * dp_ber_in_marker()), as are any whose truth index falls outside the truth
 * array; both land in `skipped`.
 */
static inline void
dp_ber_score (dp_ber_t *b, const float complex *rx, size_t lo, size_t hi,
              const uint8_t *truth, size_t n_truth, const dp_ber_marker_t *mk,
              const dp_ber_sync_t *sy)
{
  ber_align_t a;
  /* No marker means no exclusion. ber_meter_set_align() reads n_marker == 0
     as "use the default 256", so say it with a t0 past the end of the truth
     instead — every index then falls before the marker and is scored. */
  size_t t0     = mk ? mk->t0 : n_truth;
  size_t mn     = mk ? (mk->n ? mk->n : DP_BER_SYNC_SYMS) : 1;
  size_t pd     = mk ? mk->period : 0;
  a.lag         = sy ? sy->lag : 0;
  a.phase       = sy ? sy->phase : 0.0;
  a.occurrences = sy ? sy->occurrences : 0;
  a.stat = a.threshold = a.margin_db = a.runner_db = 0.0;
  a.slips                                          = 0;
  a.saturated = a.ok = 0;
  ber_meter_set_truth (b->meter, truth, n_truth);
  ber_meter_set_align (b->meter, a, t0, mn, pd);
  ber_meter_score (b->meter, rx, hi, lo, hi);
  dp_ber_refresh (b);
  if (dp_ber_evm_m2m4 (b, rx, lo, hi))
    b->bursts++;
}

/* --- 5. The confidence interval - exact, at every error count ------------ */

/** @brief A rate with its interval. Assert on `lo`/`hi`, never on `p_hat`. */
typedef struct
{
  double        p_hat;   /**< Unbiased point estimate `(r-1)/(N-1)`. */
  double        lo;      /**< Lower confidence limit. */
  double        hi;      /**< Upper confidence limit. */
  double        rel;     /**< Relative standard error `1/sqrt(r)`. */
  double        conf;    /**< The confidence level used. */
  unsigned long errors;  /**< `r`. */
  unsigned long symbols; /**< `N` (or bits, for a BER). */
} dp_ber_ci_t;

/**
 * @brief Exact confidence interval for a run stopped on an ERROR count.
 *
 * Under inverse binomial sampling — fix the errors `r`, let the trials `N` be
 * what falls out — `N` is negative-binomially distributed, not the errors, and
 * two things the fixed-`N` habit gets wrong follow:
 *
 *   - the naive `r/N` is **biased**; the unbiased estimator is `(r-1)/(N-1)`;
 *   - the interval comes from the Gamma/chi-square relation, exactly:
 *     `p in [chi2_{a/2}(2r)/2N, chi2_{1-a/2}(2r)/2N]`.
 *
 * Both quantiles are evaluated with doppler's own inverse regularized
 * incomplete gamma rather than a second copy of one:
 * `det_threshold_noncoherent (q, r)` returns the `b` with `marcum_q(r, 0, b) =
 * q`, and `marcum_q(r, 0, b) = Q(r, b^2/2)` for the regularized upper
 * incomplete gamma `Q`, so `chi2_q(2r)/2 = 0.5 *
 * det_threshold_noncoherent(1-q, r)^2`. At `r = 1` that reduces to the closed
 * form `det_threshold()`, and the interval is `[-ln(1-a/2)/N, -ln(a/2)/N]` —
 * correct, and no normal approximation anywhere, so the interval stays honest
 * at the small error counts where a Wald interval is worst.
 *
 * **Assert on `lo`.** Comparing the lower limit against a spec is the form
 * that cannot flake on counting noise; comparing `p_hat` will.
 *
 * With `r = 0` there is no point estimate, but the one-sided exact upper limit
 * `-ln(alpha)/N` still holds and is returned — that is the honest way to
 * report "no errors in N symbols".
 */
static inline dp_ber_ci_t
dp_ber_ci (unsigned long errors, unsigned long symbols, double conf)
{
  ber_interval_t i = ber_confidence (errors, symbols, conf);
  dp_ber_ci_t    c;
  c.p_hat   = i.p_hat;
  c.lo      = i.lo;
  c.hi      = i.hi;
  c.rel     = i.rel;
  c.conf    = i.conf;
  c.errors  = (unsigned long)i.errors;
  c.symbols = (unsigned long)i.symbols;
  return c;
}

/* --- 6. The verdict - does the number make sense? ------------------------ */

/** @brief Everything needed to defend a reported error rate. */
typedef struct
{
  dp_ber_ci_t ser;     /**< Symbol error rate with its interval. */
  dp_ber_ci_t ber;     /**< Bit error rate with its interval. */
  double      evm_db;  /**< Self-referenced EVM over the SAME window. */
  double evm_floor_db; /**< What a fully scattered constellation would read. */
  double m2m4_db;      /**< Blind M2M4 Es/N0 over the SAME window. */
  double esn0_db;      /**< The Es/N0 the stimulus was built at. */
  double theory_ser;   /**< Coherent bound at that Es/N0. */
  double theory_ber;   /**< Gray-coded bit bound at that Es/N0. */
  double loss_db;      /**< Implementation loss: `esn0_db - esn0(measured)`. */
  size_t window_lo;    /**< First symbol measured. */
  size_t window_hi;    /**< One past the last. */
  int    settled;      /**< The window cleared every settling budget. */
  int    aligned;      /**< The marker detection passed its Pfa gate. */
  int    enough;       /**< The error target was reached. */
  int    sane;         /**< EVM / M2M4 / theory all agree with the rate. */
  int    ok;           /**< All four gates. Anything less is not a result. */
  const char *why;     /**< The first failing gate. */
} dp_ber_report_t;

/**
 * @brief Apply the three gates to an accumulated measurement.
 *
 * The `sane` gate is the one that catches what a passing test never questions.
 * It fails when:
 *
 *   - **the EVM BEATS the matched-filter bound** by more than 1 dB. `EVM_dB =
 *     -(Es/N0)_dB` at the matched-filter output (an I/Q-plane quantity against
 *     an I/Q-plane bound — no factor of two, and quoting one flatters the
 *     result by 3 dB). Nothing can be better than that, so an EVM below it
 *     means the measurement is wrong, never that the receiver is brilliant;
 *   - **the whole SER interval sits BELOW the coherent bound.** A receiver
 *     cannot beat theory at its own Es/N0. Statistically significant "better
 *     than theory" is the signature of an alignment that was optimised over
 *     rather than detected, or of scoring known symbols;
 *   - **the blind M2M4 Es/N0 disagrees with the stated one by > 3 dB.** It is
 *     truth-free and independent, so a disagreement means the stimulus is not
 *     at the Es/N0 you think it is;
 *   - **EVM and M2M4 disagree with each other by > 4 dB.** They are not
 *     redundant: M2M4 uses only `|x|^2` and `|x|^4` moments, so it is
 *     ROTATION-BLIND, while the EVM estimates one global rotation. A spinning
 *     constant-modulus constellation reads a healthy M2M4 and a collapsed EVM
 *     — measured `ber=0.404 evm=-2.5 dB m2m4=15.3 dB`, which localised the
 *     fault to unrecovered carrier phase and ruled out SNR, timing and a
 *     symbol famine in one measurement.
 *
 * Note what `sane` does NOT claim. Both truth-free validators estimate the
 * constellation rotation from the data, so **neither can see an unsettled
 * window**: an 8PSK measurement once read an EVM within 0.3 dB of the bound,
 * a mean phase error of -0.0002 rad, and not one symbol beyond the +-pi/8
 * decision boundary, beside an SER of 5.9e-3. That is what the `settled` gate
 * is for, and why it is separate.
 *
 * @param b        The accumulator.
 * @param esn0_db  Matched-filter-output Es/N0 the stimulus was built at.
 * @param sy       The alignment (may be NULL — then `aligned` is 0).
 * @param lo       First symbol measured (for the record).
 * @param hi       One past the last.
 * @param settled  Whether the window cleared dp_ber_settle().
 * @param conf     Two-sided confidence level (DP_BER_CONF).
 */
static inline dp_ber_report_t
dp_ber_report (const dp_ber_t *b, double esn0_db, const dp_ber_sync_t *sy,
               size_t lo, size_t hi, int settled, double conf)
{
  dp_ber_report_t r;
  double          esn0 = pow (10.0, esn0_db / 10.0);
  double          n    = (double)(b->bursts ? b->bursts : 1);

  r.ser          = dp_ber_ci (b->errors, b->symbols, conf);
  r.ber          = dp_ber_ci (b->bit_errors, b->bits, conf);
  r.evm_db       = b->evm_sum / n;
  r.evm_floor_db = dp_test_evm_scatter_floor_db (b->m);
  r.m2m4_db      = b->m2m4_sum / n;
  r.esn0_db      = esn0_db;
  r.theory_ser   = dp_ber_theory_ser (b->m, esn0);
  r.theory_ber   = dp_ber_theory_ber (b->m, esn0);
  r.window_lo    = lo;
  r.window_hi    = hi;
  r.settled      = settled ? 1 : 0;
  r.aligned      = (sy && sy->ok) ? 1 : 0;
  r.enough       = dp_ber_enough (b);
  r.loss_db      = (b->errors && b->symbols)
                       ? esn0_db - dp_ber_esn0_db_for_ser (b->m, r.ser.p_hat)
                       : NAN;

  r.sane = 1;
  r.why  = "ok";
  if (!r.settled)
    {
      r.why  = "window is not settled: every metric in it measures settling";
      r.sane = 0;
    }
  else if (!r.aligned)
    {
      r.why  = sy ? sy->why : "no alignment was detected";
      r.sane = 0;
    }
  else if (b->symbols == 0)
    {
      r.why  = "no symbols scored";
      r.sane = 0;
    }
  else if (r.evm_db < -esn0_db - 1.0)
    {
      r.why  = "EVM beats the matched-filter bound: the measurement is wrong";
      r.sane = 0;
    }
  else if (r.evm_db > r.evm_floor_db - 1.0)
    {
      r.why  = "EVM is at the fully-scattered floor: no usable constellation";
      r.sane = 0;
    }
  else if (fabs (r.evm_db + esn0_db) > 3.0)
    {
      r.why  = "EVM does not track -(Es/N0): the window or the level is wrong";
      r.sane = 0;
    }
  else if (b->errors && r.ser.hi < r.theory_ser)
    {
      r.why  = "SER interval sits entirely below the coherent bound";
      r.sane = 0;
    }
  else if (fabs (r.m2m4_db - esn0_db) > 3.0)
    {
      r.why  = "blind M2M4 Es/N0 disagrees with the stimulus";
      r.sane = 0;
    }
  else if (fabs (r.m2m4_db + r.evm_db) > 4.0)
    {
      r.why  = "EVM and M2M4 disagree: suspect an unrecovered rotation";
      r.sane = 0;
    }
  else if (!r.enough)
    {
      r.why = "error target not reached: the interval is wider than reported";
    }
  r.ok = r.settled && r.aligned && r.enough && r.sane;
  return r;
}

/**
 * @brief Print a report in the canonical form — every number with its window.
 *
 * A bare error rate invites exactly the class of mistake this file exists to
 * prevent, so the printer always emits the window, the alignment, the counts,
 * the interval and the cross-checks alongside it. Copy this line into a commit
 * message or a session note verbatim; it is self-defending.
 */
static inline void
dp_ber_print (const char *label, const dp_ber_report_t *r)
{
  printf ("%-24s %s  SER %.3e [%.3e, %.3e] %.0f%%  r=%lu N=%lu\n", label,
          r->ok ? "OK  " : "BAD ", r->ser.p_hat, r->ser.lo, r->ser.hi,
          100.0 * r->ser.conf, r->ser.errors, r->ser.symbols);
  printf ("%-24s   BER %.3e [%.3e, %.3e]  r=%lu N=%lu\n", "", r->ber.p_hat,
          r->ber.lo, r->ber.hi, r->ber.errors, r->ber.symbols);
  printf ("%-24s   window [%zu, %zu) settled=%d aligned=%d enough=%d\n", "",
          r->window_lo, r->window_hi, r->settled, r->aligned, r->enough);
  printf ("%-24s   Es/N0 %.2f dB  theory SER %.3e  loss %.2f dB\n", "",
          r->esn0_db, r->theory_ser, r->loss_db);
  printf ("%-24s   EVM %.2f dB (bound %.2f, scatter floor %.2f)  "
          "M2M4 %.2f dB\n",
          "", r->evm_db, -r->esn0_db, r->evm_floor_db, r->m2m4_db);
  printf ("%-24s   -- %s\n", "", r->why);
}

/**
 * @brief One-call measurement of a single burst: sync, score, report.
 *
 * The convenience path, and the one to prefer: it wires the three gates
 * together in the only order that is correct, and it places the marker so the
 * alignment is fixed on symbols DISJOINT from the ones scored.
 *
 * Accumulates into @p b, so the caller's loop is simply
 *
 * @code
 * dp_ber_t acc;
 * int      ok;
 * dp_ber_init (&acc, m, DP_BER_TARGET_ERRORS);
 * for (unsigned seed = 0; !dp_ber_enough (&acc) && seed < 200; seed++)
 *   {
 *     size_t nout   = run_receiver (seed, rx, truth, lock_t, lock_c);
 *     size_t settle = dp_ber_settle (bn_t, bn_c, lock_t, lock_c, NULL,
 *                                    nout, &ok);
 *     r = dp_ber_measure (&acc, rx, nout, truth, nsym, esn0_db, settle, ok,
 *                         NULL);
 *   }
 * dp_ber_print ("qpsk @10.3dB", &r);
 * @endcode
 *
 * with the seed cap as the guard that stops a working receiver running
 * forever.
 *
 * **@p settled is not a formality.** It is gate 1, and it is the caller's to
 * establish because only the caller has the receiver's lock indicators. Pass
 * dp_ber_settle()'s `ok` straight through: a 0 makes the report `BAD` with the
 * unsettled-window reason, which is the correct outcome for a burst that never
 * had a steady state — not something to be worked around by widening a
 * tolerance.
 *
 * @param b        Accumulator, already dp_ber_init()ed.
 * @param rx       Recovered symbols.
 * @param n_rx     How many.
 * @param truth    Transmitted symbol indices.
 * @param n_truth  How many.
 * @param esn0_db  Matched-filter Es/N0 the stimulus was built at.
 * @param settle   Window start from dp_ber_settle().
 * @param settled  dp_ber_settle()'s `ok`: did every indicator reach lock?
 * @param mk       Marker, or NULL to align on a prefix of the settled window.
 * @return         The per-burst report (also folded into @p b).
 */
static inline dp_ber_report_t
dp_ber_measure (dp_ber_t *b, const float complex *rx, size_t n_rx,
                const uint8_t *truth, size_t n_truth, double esn0_db,
                size_t settle, int settled, const dp_ber_marker_t *mk)
{
  dp_ber_marker_t local;
  dp_ber_sync_t   sy;
  size_t          lo;

  if (!mk)
    {
      /* Align on truth symbols placed a full lag-span past the settling
         point, so the marker is guaranteed to sit inside the settled part of
         the record whatever the (still unknown) lag turns out to be. */
      local.sym    = NULL;
      local.n      = DP_BER_SYNC_SYMS;
      local.t0     = settle + (size_t)DP_BER_LAG_SPAN;
      local.period = 0;
      local.reps   = 0;
      mk           = &local;
    }
  sy = dp_ber_sync (rx, n_rx, truth, n_truth, mk, b->m, DP_BER_LAG_SPAN,
                    DP_BER_SYNC_PFA);

  /* Score from the settled point, but never before the marker ends: the
     symbols that fixed the alignment must not also be scored. */
  lo = settle;
  if (sy.ok)
    {
      size_t mn  = mk->n ? mk->n : DP_BER_SYNC_SYMS;
      size_t end = mk->t0 + mn;
      long   e   = (long)end - sy.lag;
      if (!mk->period && e > 0 && (size_t)e > lo)
        lo = (size_t)e;
      dp_ber_score (b, rx, lo, n_rx, truth, n_truth, mk, &sy);
    }
  return dp_ber_report (b, esn0_db, &sy, lo, n_rx, settled, DP_BER_CONF);
}

#endif /* DP_BER_TEST_H */
