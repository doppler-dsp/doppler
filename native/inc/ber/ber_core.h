/**
 * @file ber_core.h
 * @brief Error-rate measurement: settled windows, detected alignment, and an
 *        exact confidence interval.
 *
 * **An error rate is a measurement, and this module is its instrument.** Every
 * confidently-wrong receiver number this project has produced came from one of
 * three places, and each has a gate here. A measurement that has not passed
 * all three is not a result:
 *
 * 1. **Is it SETTLED?** A second-order loop needs ~5/Bn symbols; two cascaded
 *    loops ADD their budgets; joint tracking DOUBLES the sum. So the floor is
 *    `2*(5/bn_timing + 5/bn_carrier)` — ber_settle_syms() — and the window
 *    must additionally clear every lock indicator the receiver publishes, plus
 *    the handover instant again when one is enabled. Measuring inside that
 *    window measures settling and reports it as steady state: measured cost,
 *    -9.0 dB EVM where the settled answer is -23.2 dB, and SER 5.9x the
 *    coherent bound where the settled answer is 1.7x.
 *
 * 2. **Have we counted enough ERRORS?** Fix the ERROR count and let the symbol
 *    count fall out (inverse binomial sampling). The relative standard error
 *    is then `1/sqrt(r)` — a function of the error count ALONE — so the error
 *    target IS the precision. Stopping on a fixed symbol count makes precision
 *    depend on the very rate being measured: 20 000 symbols at SER 1e-3 gives
 *    ~20 errors and ~22% relative error, which reads as real seed-to-seed
 *    variation in the receiver and is not.
 *
 * 3. **Does it MAKE SENSE?** Cross-check against measurements that cannot fail
 *    the same way: the truth-free EVM (ber_evm_scatter_floor_db() bounds what
 *    it can prove) and the coherent theory curve (ber_theory_ser()).
 *
 * ## The alignment is DETECTED, never searched
 *
 * The historic footgun is scoring `min over (lag, rotation)` of the error
 * count. That is not a measurement of the receiver, it is an optimisation over
 * the answer, and it fails both ways: a wide search on a short window finds a
 * lucky low-error alignment on garbage (false PASS), and a narrow one misses
 * the true alignment on a healthy receiver and reports chance (false FLOOR).
 * Both have shipped here — a committed "~12 dB floor" that was really ~5 dB,
 * and an "SER 0.48" on a receiver running at 0.0000 that needed lag -34.
 *
 * ber_meter_align() *detects* the alignment instead, correlating against a
 * known marker (a sync word, a PN code period, or — in a simulation, where
 * truth exists — a stretch of the truth sequence itself). It returns the lag
 * and the absolute carrier phase from the correlation peak, gated by a
 * false-alarm probability through the canonical detection primitives and
 * Bonferroni-corrected over the lags searched. A marker too short to identify
 * an alignment reports `ok = 0` rather than a plausible wrong lag, and the
 * marker's own symbols are excluded from scoring so the symbols that fixed the
 * alignment cannot also flatter the rate.
 *
 * ## Reuse, not re-derivation
 *
 * Nothing numeric is invented here. The confidence interval is the exact
 * Gamma/chi-square one and its quantiles come from `det_threshold()` /
 * `det_threshold_noncoherent()` — doppler's own inverse regularized incomplete
 * gamma, already validated in the detection module — rather than a second copy
 * of a series/continued-fraction kernel. Verified bit-identical to SciPy's
 * `chi2.ppf` at r = 1, 2, 20, 200 and 1000. Gray coding comes from `mpsk`, the
 * blind SNR from `snr`.
 */
#ifndef BER_CORE_H
#define BER_CORE_H

#include "dp_state.h"
#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Default operating point: the SER a characterisation anchors at.
 *
 * Anchoring at a fixed error rate rather than a fixed Es/N0 asks "does the
 * receiver meet its bound" at the same place on the curve for every
 * constellation (per-M, 6.8 / 10.3 / 15.7 dB for BPSK / QPSK / 8PSK). */
#define BER_TARGET_SER 1e-3

/** @brief Default error target: ~7% relative standard error (`1/sqrt(r)`). */
#define BER_TARGET_ERRORS 200u

/** @brief Default two-sided confidence level. */
#define BER_CONF 0.99

/** @brief Default lag search half-width, symbols. Generous on purpose: group
 * delay varies a lot with pulse shape and front end (RRC on the complex path
 * needed -34), and a clipped search reported SER 0.48 on a perfect receiver. */
#define BER_LAG_SPAN 200

/** @brief Default marker length when aligning on a stretch of truth. */
#define BER_SYNC_SYMS 256u

/** @brief Default whole-search false-alarm probability for the align gate. */
#define BER_SYNC_PFA 1e-6

/** @brief Largest lag search the fixed-size scratch supports. */
#define BER_MAX_LAGS 2048

  /* ── records ──────────────────────────────────────────────────────────── */

  /**
   * @brief A rate with its exact interval. Assert on @p lo, never on @p p_hat.
   *
   * Under inverse binomial sampling the trials `N` are the random variable, not
   * the errors, so the naive `r/N` is biased (the unbiased estimator is
   * `(r-1)/(N-1)`) and the interval is the Gamma/chi-square one,
   * `[chi2_{a/2}(2r)/2N, chi2_{1-a/2}(2r)/2N]`. Comparing @p lo against a spec
   * is the form that cannot flake on counting noise; comparing @p p_hat will.
   */
  typedef struct
  {
    double p_hat;   /**< Unbiased point estimate `(r-1)/(N-1)`.          */
    double lo;      /**< Lower confidence limit.                          */
    double hi;      /**< Upper confidence limit.                          */
    double rel;     /**< Relative standard error `1/sqrt(r)`.             */
    double conf;    /**< Confidence level used.                           */
    size_t errors;  /**< `r`.                                             */
    size_t symbols; /**< `N` (or bits, for a BER).                        */
  } ber_interval_t;

  /** @brief Where the recovered stream sits against truth, and how sure. */
  typedef struct
  {
    int    lag;         /**< `rx[i]` carries `truth[i + lag]`.            */
    double phase;       /**< Absolute residual constellation rotation.    */
    double stat;        /**< Detection statistic at the peak.             */
    double threshold;   /**< Pfa-derived threshold it had to beat.        */
    double margin_db;   /**< `20*log10(stat/threshold)` — headroom.       */
    double runner_db;   /**< Peak over runner-up, dB; ambiguity check.    */
    size_t occurrences; /**< Marker occurrences combined.                 */
    size_t slips;       /**< Occurrences whose phase disagreed: slips.    */
    int    saturated;   /**< Peak on a search edge: lag_span too small.   */
    int    ok;          /**< Detected, unambiguous, unsaturated.          */
  } ber_align_t;

  /* ── free functions: theory and windows ───────────────────────────────── */

  /** @brief Gaussian tail `Q(x) = P(N(0,1) > x)`. */
  double ber_qfunc (double x);

  /**
   * @brief Coherent M-PSK symbol error rate at matched-filter Es/N0 (LINEAR).
   *
   * `BPSK: Q(sqrt(2 Es/N0))`, `QPSK: 2 Q(sqrt(Es/N0))`,
   * `8PSK: 2 Q(sqrt(2 Es/N0) sin(pi/8))` — the nearest-neighbour union bound,
   * tight to well under a percent at any Es/N0 worth testing at.
   *
   * **This is a COHERENT bound.** A differentially-decoded rate is ~2x it,
   * because a differential decision fails when either of its two symbols is
   * wrong (measured 1.88-2.11 across M and both receiver paths). Pairing a
   * differential measurement with this curve invents a factor of two of
   * "implementation loss".
   */
  double ber_theory_ser (int m, double esn0);

  /** @brief Coherent GRAY-coded M-PSK bit error rate at Es/N0 (LINEAR).
   *  BPSK and Gray QPSK are exactly `Q(sqrt(2 Eb/N0))`; 8PSK uses `SER/log2 M`,
   *  exact in the high-Es/N0 limit where an error lands on a neighbour. */
  double ber_theory_ber (int m, double esn0);

  /**
   * @brief Es/N0 (dB) at which the coherent bound equals @p ser.
   *
   * How an implementation loss is quoted honestly: convert the MEASURED rate to
   * the Es/N0 theory would need to produce it, and subtract. A loss in dB is
   * comparable across M and across operating points; a ratio of rates is not.
   */
  double ber_esn0_db_for_ser (int m, double ser);

  /**
   * @brief EVM (dB) of an M-PSK constellation at a UNIFORMLY RANDOM rotation.
   *
   * The FLOOR of a self-referenced EVM: what a completely destroyed
   * constant-modulus constellation reads. Slicing a unit-modulus point at a
   * uniformly random phase to its nearest of M neighbours leaves
   * `E|e|^2 = 2 - 2 sin(pi/M)/(pi/M)`: **-1.4 dB at BPSK, -7.0 at QPSK,
   * -12.9 at 8PSK**.
   *
   * **Any fixed EVM threshold must be stated against this, never against
   * 0 dB.** "Scattered reads ~0 dB" is the BPSK limit only. At 8PSK a stream
   * with no carrier recovery at all reads -12.9 dB — which is also what a
   * perfectly healthy 13 dB link reads — so a `< -12.0` assertion is satisfied
   * by pure noise. That was live in this repo's own receiver tests until
   * 2026-07-27. The room between "on the bound at the SER=1e-3 anchor" and
   * "completely broken" collapses as M grows: 5.4 dB at BPSK, 3.3 at QPSK,
   * 2.8 at 8PSK, so at high M the EVM cannot carry a verdict by itself.
   */
  double ber_evm_scatter_floor_db (int m);

  /**
   * @brief Symbols to discard before a steady-state measurement means anything.
   *
   * `2 * (5/bn_timing + 5/bn_carrier)`. Three factors, and skipping any of them
   * produces a confident wrong number: 5/Bn per loop is the standard
   * second-order settling time (in symbols, because both `bn` are normalised to
   * the SYMBOL rate); the two budgets ADD because the loops are CASCADED (the
   * carrier discriminator reads the on-time strobe, so it cannot converge until
   * timing has); and the sum DOUBLES for joint tracking, where each loop sees
   * the other's transient as a disturbance.
   *
   * This is a floor, not the answer — take the max of it and every lock
   * indicator the receiver publishes, plus the handover instant again if one is
   * enabled. Pass a loop's `bn` as 0 if it is not running.
   */
  size_t ber_settle_syms (double bn_timing, double bn_carrier);

  /**
   * @brief First symbol from which a verify-counted flag is SUSTAINED.
   *
   * "Sustained" is @p sustain consecutive symbols high AND at least @p min_frac
   * of everything after that point high too. Both halves carry weight: the run
   * rejects a single lucky decision, the fraction rejects a detector that
   * declares early then flaps. Dating the lock by the FINAL contiguous run
   * instead is right with no noise and badly wrong with it — one late dip once
   * moved a reported lock from 415 to 2286 and left no measurement window.
   *
   * @return The symbol index, or -1 for "never locked" — the honest answer,
   *         which forces the caller to say so rather than measure a transient.
   */
  int ber_lock_symbol (const uint8_t *flags, size_t flags_len, size_t sustain,
                        double min_frac);

  /**
   * @brief Self-referenced EVM (dB) over an EXPLICIT window `[lo, hi)`.
   *
   * Scores each symbol against the stream's OWN hard decision, with the
   * constellation rotation estimated from the data — so it references neither
   * the transmitted symbols nor a lag, and cannot be fooled by an alignment
   * search. At a matched-filter output the error vector IS the complex noise,
   * so a locked stream reads `EVM[dB] ~ -(Es/N0)[dB]`. EVM is an I/Q-plane
   * quantity: there is no factor of two — that belongs to an I-only
   * measurement, and quoting it flatters the result by 3 dB.
   *
   * **Pass the real `m`**, and read the result against
   * ber_evm_scatter_floor_db(m), never against 0 dB.
   *
   * The window is EXPLICIT because BER and EVM must be measured on the SAME
   * one. A convenience "back half" default silently scores a different window
   * than the error rate did, and the two eventually disagree in a way that
   * reads as a receiver defect rather than the harness bug it is.
   *
   * @return EVM in dB, or 0.0 ("no lock") for a window under 20 symbols.
   */
  double ber_evm_db (const float complex *rx, size_t rx_len, size_t lo,
                     size_t hi, int m);

  /**
   * @brief Combine an analytic settling budget with measured lock instants.
   *
   * The POLICY for where a steady-state window may start, in one place:
   * `max(budget, timing lock, carrier lock, handover + budget)`. The analytic
   * budget and the receiver's own indicators are both fallible in the SAME
   * direction, so whichever settles last decides.
   *
   * **A handover settles last of all.** With `acq_to_track` on it fires on
   * carrier lock plus a warmup — strictly after the budget and after every
   * lock indicator — and the decision-directed loop then has its own
   * transient, so it contributes `its instant + the budget again`. Measured on
   * 8PSK at its SER=1e-3 anchor: handover at symbol 2525 against a
   * 2000-symbol budget, SER 5.95x the coherent bound measured from 2000 and
   * 1.68x from 4525.
   *
   * Pass -1 for any indicator the receiver does not publish (which is what
   * ber_lock_symbol() returns for "never locked"). **A -1 timing or carrier
   * lock means there is NO valid steady-state window** — check that yourself
   * before trusting the return; a -1 handover is not a failure, because a
   * pure-NDA receiver never publishes one.
   *
   * @param budget       ber_settle_syms() of the loops in use.
   * @param timing_lock  ber_lock_symbol() of the timing flag, or -1.
   * @param carrier_lock ber_lock_symbol() of the carrier flag, or -1.
   * @param handover     ber_lock_symbol() of the tracking flag, or -1.
   * @return             First symbol of the measurement window.
   */
  size_t ber_settle_from (size_t budget, int timing_lock, int carrier_lock,
                          int handover);

  /**
   * @brief Exact confidence interval for a run stopped on an ERROR count.
   *
   * Both quantiles come from doppler's own inverse regularized incomplete gamma
   * rather than a second copy of one: `det_threshold_noncoherent(q, r)` returns
   * the `b` with `marcum_q(r, 0, b) = q`, and `marcum_q(r, 0, b) = Q(r, b^2/2)`,
   * so `chi2_q(2r)/2 = 0.5 * det_threshold_noncoherent(1-q, r)^2`. At `r = 1`
   * that reduces to the closed form and the interval is `[-ln(1-a/2)/N,
   * -ln(a/2)/N]` — no normal approximation anywhere, so it stays honest at the
   * small error counts where a Wald interval is worst.
   *
   * With `r = 0` there is no point estimate, but the exact one-sided upper
   * limit `-ln(alpha)/N` still holds and is returned — the honest way to report
   * "no errors in N symbols".
   */
  ber_interval_t ber_confidence (size_t errors, size_t symbols, double conf);

#ifdef __cplusplus
}
#endif

#endif /* BER_CORE_H */
