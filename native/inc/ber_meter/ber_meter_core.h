/**
 * @file ber_meter_core.h
 * @brief BerMeter — the error-rate accumulator.
 *
 * Owns the transmitted reference, the running error counters, the marker-based
 * alignment detector and the exact confidence interval. A caller loops bursts
 * until the ERROR target is met and reads a defensible rate off the end:
 *
 * @code
 * ber_meter_state_t *m = ber_meter_create (4, 200, 0.99);
 * ber_meter_set_truth (m, truth, nsym);
 * while (!ber_meter_get_enough (m))
 *   {
 *     size_t n = run_receiver (rx);
 *     ber_align_t a = ber_meter_align (m, rx, n, t0, 0, 0, 0, 0.0);
 *     if (a.ok)
 *       ber_meter_score (m, rx, n, lo, n, a.lag, a.phase, t0, 0, 0,
 *                        a.occurrences);
 *   }
 * ber_interval_t ser = ber_meter_ser (m);
 * ber_meter_destroy (m);
 * @endcode
 *
 * The three gates a result has to pass, and why each exists, are on
 * ber/ber_core.h. The one rule this file enforces by construction: the
 * alignment handed to ber_meter_score() is DETECTED by ber_meter_align(),
 * never searched by minimising the error count.
 */
#ifndef BER_METER_CORE_H
#define BER_METER_CORE_H

#include "ber/ber_core.h" /* the records and the free functions */
#include "clib_common.h"
#include "detection/detection_core.h"
#include "dp_state.h"
#include "jm_perf.h"
#include <complex.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define BER_METER_STATE_MAGIC DP_FOURCC ('B', 'E', 'R', 'M')
#define BER_METER_STATE_VERSION 1u

  /**
   * @brief BerMeter state.
   *
   * `m` / `target_errors` / `conf` and the truth sequence are CONFIGURATION,
   * restored by create() and set_truth(); only the running counters are packed
   * into a state blob. That keeps a blob small and independent of however many
   * symbols the reference happens to be.
   */
  typedef struct
  {
    int      m;             /**< Constellation order (2, 4, 8).            */
    int      bps;           /**< Bits per symbol = log2(m).                */
    size_t   target_errors; /**< Inverse-binomial stop condition.          */
    double   conf;          /**< Two-sided confidence level.               */
    uint8_t *truth;         /**< Transmitted symbol indices (owned copy).  */
    size_t   truth_len;     /**< How many.                                 */
    /* running counters — this is what the state blob carries */
    size_t errors;     /**< Symbol errors counted.                         */
    size_t symbols;    /**< Symbols scored.                                */
    size_t bit_errors; /**< Gray-coded bit errors counted.                 */
    size_t bits;       /**< Bits scored.                                   */
    size_t skipped;    /**< Skipped: marker-covered or out of range.       */
    size_t bursts;     /**< score() calls that scored anything.            */
    /* the last alignment detected, and the marker geometry that found it —
       score() uses these rather than taking them from the caller, so a
       measurement cannot be handed an alignment that belongs to a different
       burst or a different marker */
    ber_align_t last;      /**< Result of the last ber_meter_align().      */
    size_t      mk_t0;     /**< Marker start used by that align.           */
    size_t      mk_n;      /**< Marker length used.                        */
    size_t      mk_period; /**< Marker period used.                        */
  } ber_meter_state_t;

  /**
   * @brief Exact confidence interval for a run stopped on an ERROR count.
   *
   * Not a module free function because jm free functions cannot return a
   * record; reach it from Python as BerMeter.ser() / .ber().
   *
   * Both quantiles come from doppler's own inverse regularized incomplete
   * gamma rather than a second copy of one: `det_threshold_noncoherent(q, r)`
   * returns the `b` with `marcum_q(r, 0, b) = q`, and
   * `marcum_q(r, 0, b) = Q(r, b^2/2)`, so
   * `chi2_q(2r)/2 = 0.5 * det_threshold_noncoherent(1-q, r)^2`. At `r = 1` that
   * reduces to the closed form and the interval is `[-ln(1-a/2)/N, -ln(a/2)/N]`
   * — no normal approximation anywhere, so it stays honest at the small error
   * counts where a Wald interval is worst. Verified bit-identical to SciPy's
   * `chi2.ppf` at r = 1, 2, 20, 200 and 1000.
   */
  /**
   * @brief Detect the (lag, phase) alignment of @p rx against a known marker.
   *
   * The free-function form, needing only the truth sequence — BerMeter.align()
   * is the stateful spelling. The marker is `truth[t0 .. t0 + n_marker)`,
   * optionally repeating every @p period symbols; repeats are combined
   * NON-COHERENTLY, which raises the processing gain and exposes cycle slips.
   * The noise floor is estimated from the off-peak lags themselves (a CFAR
   * reference), so nothing needs to know the Es/N0.
   *
   * The peak's phase is the ABSOLUTE constellation rotation, so there is no
   * residual M-fold ambiguity left to search: the marker resolves it. That is
   * what removes the `min over rotation` bias from an error count. The
   * processing gain is `sqrt(2*K*L)`, so a marker too short to identify an
   * alignment simply cannot clear the threshold and reports `ok = 0` — the
   * intended behaviour, and the opposite of returning a plausible wrong lag.
   *
   * @param rx         Recovered symbols.
   * @param rx_len      How many.
   * @param truth       Transmitted symbol indices (0..m-1).
   * @param truth_len   How many.
   * @param m           Constellation order.
   * @param t0          Truth index of the marker's first occurrence.
   * @param n_marker    Marker length in symbols; 0 selects BER_SYNC_SYMS.
   * @param period      Repeat period in symbols; 0 for a single occurrence.
   * @param lag_span    Search half-width; 0 selects BER_LAG_SPAN.
   * @param pfa         Whole-search false-alarm probability; 0 selects 1e-6.
   * @return            The alignment, with `ok` saying whether to believe it.
   */
  ber_align_t ber_align_detect (const float complex *rx, size_t rx_len,
                                const uint8_t *truth, size_t truth_len, int m,
                                size_t t0, size_t n_marker, size_t period,
                                int lag_span, double pfa);

  ber_interval_t ber_confidence (size_t errors, size_t symbols, double conf);

  /* ── the meter ────────────────────────────────────────────────────────── */

  /** @brief Error counters accumulated across as many bursts as it takes. */


  /**
   * @brief Create a meter for constellation @p m stopping at @p target_errors.
   *
   * @param m              Constellation order (2, 4, 8).
   * @param target_errors  Inverse-binomial stop condition; 0 selects 200.
   * @param conf           Two-sided confidence level; 0 selects 0.99.
   */
  ber_meter_state_t *ber_meter_create (int m, size_t target_errors,
                                       double conf);
  void               ber_meter_destroy (ber_meter_state_t *state);
  /**
   * @brief Zero the running counters; keep the configuration and the truth.
   *
   * Returns the meter to a fresh count while preserving @c m, the error
   * target, the confidence level and the installed truth sequence, so one
   * meter can measure independent captures back to back without reinstalling
   * truth. The last detected alignment is left untouched; call align() again
   * for the next capture before scoring it.
   *
   * @param state  Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.ber import BerMeter
   * >>> rng = np.random.default_rng(0)
   * >>> truth = rng.integers(0, 4, size=400).astype(np.uint8)
   * >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
   * >>> rx = np.exp(1j * ang).astype(np.complex64)
   * >>> met = BerMeter(m=4)
   * >>> met.set_truth(truth)
   * 0
   * >>> met.align(rx, n_marker=64)
   * 1
   * >>> met.score(rx, hi=truth.size)
   * 336
   * >>> met.symbols
   * 336
   * >>> met.reset()               # reuse the meter for the next capture
   * >>> (met.errors, met.symbols)
   * (0, 0)
   *
   * @endcode
   */
  void ber_meter_reset (ber_meter_state_t *state);

  /**
   * @brief Install the transmitted symbol sequence this meter scores against.
   *
   * Copied, so the caller's buffer need not outlive the call, and reused across
   * every burst. Values are symbol INDICES in `0..m-1` (not Gray labels): the
   * meter Gray-encodes each side itself when it counts bit errors, so handing
   * it Gray labels would double-encode and inflate the rate.
   *
   * @param state      Must be non-NULL.
   * @param truth      Transmitted symbol indices, each in `0..m-1`.
   * @param truth_len  How many symbols the reference holds.
   * @return DP_OK, or DP_ERR_INVALID if any index is outside `0..m-1`.
   * @code
   * >>> import numpy as np
   * >>> from doppler.ber import BerMeter
   * >>> met = BerMeter(m=4)
   * >>> truth = np.array([0, 3, 1, 2, 2, 0], dtype=np.uint8)  # indices, 0..3
   * >>> met.set_truth(truth)
   * 0
   *
   * @endcode
   */
  int ber_meter_set_truth (ber_meter_state_t *state, const uint8_t *truth,
                           size_t truth_len);

  /**
   * @brief Pure detection: returns the alignment without touching state.
   *
   * ber_meter_align() is the stateful spelling the Python binding uses. The
   * marker comes from the truth installed by ber_meter_set_truth().
   *
   * @param state     Must be non-NULL, with truth installed.
   * @param rx        Recovered symbols.
   * @param rx_len    How many.
   * @param t0        Truth index of the marker's first occurrence.
   * @param n_marker  Marker length in symbols; 0 selects BER_SYNC_SYMS.
   * @param period    Repeat period in symbols; 0 for a single occurrence.
   * @param lag_span  Search half-width; 0 selects BER_LAG_SPAN.
   * @param pfa       Whole-search false-alarm probability; 0 selects 1e-6.
   * @return          The alignment, with `ok` saying whether to believe it.
   */
  ber_align_t ber_meter_detect (const ber_meter_state_t *state,
                                const float complex *rx, size_t rx_len,
                                size_t t0, size_t n_marker, size_t period,
                                int lag_span, double pfa);

  /**
   * @brief Detect where @p rx sits against truth and REMEMBER that alignment.
   *
   * Correlates the known marker `truth[t0 .. t0 + n_marker)` against @p rx
   * over a span of lags, gates the peak with a false-alarm probability, and
   * stores the winning lag, absolute carrier phase and marker geometry on the
   * meter so score() later uses exactly this detection — never a lag searched to
   * minimise the error count. The peak's phase is the ABSOLUTE constellation
   * rotation, so no M-fold ambiguity is left to resolve; a marker too short to
   * clear the gate reports failure rather than a plausible wrong lag. Read the
   * outcome through @c align_ok, @c lag, @c phase and @c align_margin_db.
   *
   * @param state     Must be non-NULL, with truth installed by set_truth().
   * @param rx        Recovered symbols to align against the truth.
   * @param rx_len    How many recovered symbols.
   * @param t0        Truth index of the marker's first occurrence.
   * @param n_marker  Marker length in symbols; 0 selects BER_SYNC_SYMS.
   * @param period    Repeat period in symbols; 0 for a single occurrence.
   * @param lag_span  Search half-width in symbols; 0 selects BER_LAG_SPAN.
   * @param pfa       Whole-search false-alarm probability; 0 selects 1e-6.
   * @return 1 when the detection passed its false-alarm gate, else 0.
   * @code
   * >>> import numpy as np
   * >>> from doppler.ber import BerMeter
   * >>> rng = np.random.default_rng(0)
   * >>> truth = rng.integers(0, 4, size=600).astype(np.uint8)
   * >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
   * >>> rx = np.exp(1j * ang).astype(np.complex64)
   * >>> met = BerMeter(m=4)
   * >>> met.set_truth(truth)
   * 0
   * >>> met.align(rx, n_marker=64)     # correlate a 64-symbol marker
   * 1
   * >>> met.lag, met.align_ok          # detected, so score() may be trusted
   * (0, 1)
   *
   * @endcode
   */
  int ber_meter_align (ber_meter_state_t *state, const float complex *rx,
                       size_t rx_len, size_t t0, size_t n_marker,
                       size_t period, int lag_span, double pfa);

  /**
   * @brief Score `rx[lo .. hi)` against the truth and accumulate the counters.
   *
   * Demodulates each symbol in the window under the alignment the last align()
   * detected — its lag and absolute phase — and tallies symbol and Gray-coded
   * bit errors against the installed truth. The alignment is used VERBATIM: no
   * lag search, no rotation search, no minimisation of any kind over the answer.
   * Symbols covered by a marker occurrence are excluded, as are any whose truth
   * index falls outside the installed sequence; both land in @c skipped.
   *
   * @param state   Must be non-NULL, with an alignment set by align().
   * @param rx      Recovered symbols to score.
   * @param rx_len  How many recovered symbols.
   * @param lo      First symbol index to score (inclusive).
   * @param hi      One past the last symbol index to score; clamped to rx_len.
   *               `hi = 0` scores nothing, so pass the window's true end.
   * @return Symbols actually scored (window length minus skipped symbols).
   * @code
   * >>> import numpy as np
   * >>> from doppler.ber import BerMeter
   * >>> rng = np.random.default_rng(1)
   * >>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
   * >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
   * >>> rx = np.exp(1j * ang).astype(np.complex64)
   * >>> met = BerMeter(m=4)
   * >>> met.set_truth(truth)
   * 0
   * >>> met.align(rx, n_marker=64)
   * 1
   * >>> met.score(rx, hi=truth.size)   # the 64 marker symbols are excluded
   * 736
   * >>> met.errors, met.skipped
   * (0, 64)
   *
   * @endcode
   */
  size_t ber_meter_score (ber_meter_state_t *state, const float complex *rx,
                          size_t rx_len, size_t lo, size_t hi);

  /**
   * @brief Install an alignment detected elsewhere (e.g. by
   * ber_align_detect() on a different buffer), with the marker geometry that
   * produced it, so ber_meter_score() can use it.
   *
   * The stateful ber_meter_align() is the usual path; this exists for the case
   * where detection and scoring run over different buffers. It is deliberately
   * the ONLY way to set an alignment other than detecting one — score() never
   * takes a lag from its caller, because a lag that was passed in is a lag that
   * could have been searched for.
   */
  void ber_meter_set_align (ber_meter_state_t *state, ber_align_t align,
                            size_t t0, size_t n_marker, size_t period);

  /** @brief Has the error target been reached? The inverse-binomial stop. */
  int ber_meter_get_enough (const ber_meter_state_t *state);

  /**
   * @brief Exact confidence interval for error/trial counts from ELSEWHERE.
   *
   * The pure-function face of the meter's statistics, at this meter's own
   * confidence level: hand it any error and trial counts and it returns the
   * same exact Gamma/chi-square interval ser() would, with quantiles from
   * doppler's own inverse regularized incomplete gamma rather than a normal
   * approximation, so it stays honest at the small error counts where a Wald
   * interval is worst. Assert on @c lo, never on @c p_hat.
   *
   * @param state    Must be non-NULL.
   * @param errors   Errors counted, `r`.
   * @param symbols  Trials counted, `N` (symbols, or bits for a BER).
   * @return A BerInterval record — `(p_hat, lo, hi, rel, conf, errors,
   *         symbols)` — the unbiased rate with its exact two-sided limits.
   * @code
   * >>> from doppler.ber import BerMeter
   * >>> met = BerMeter(m=4, conf=0.99)
   * >>> ci = met.interval(errors=8, symbols=20000)   # counts from elsewhere
   * >>> round(ci.p_hat, 6), round(ci.lo, 6), round(ci.hi, 6)
   * (0.00035, 0.000129, 0.000857)
   *
   * @endcode
   */
  ber_interval_t ber_meter_interval (const ber_meter_state_t *state,
                                     size_t errors, size_t symbols);

  /**
   * @brief Symbol error rate over the scored symbols, with its exact interval.
   *
   * Divides the accumulated symbol-error count by the symbols scored and wraps
   * it in the exact Gamma/chi-square interval for inverse binomial sampling —
   * no normal approximation anywhere. Assert on @c lo, never on @c p_hat:
   * comparing the lower limit against a spec is the form that cannot flake on
   * counting noise.
   *
   * @param state  Must be non-NULL.
   * @return A BerInterval record — `(p_hat, lo, hi, rel, conf, errors,
   *         symbols)` — the symbol error rate with its exact two-sided limits.
   * @code
   * >>> import numpy as np
   * >>> from doppler.ber import BerMeter
   * >>> rng = np.random.default_rng(1)
   * >>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
   * >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
   * >>> rx = np.exp(1j * ang).astype(np.complex64)
   * >>> rx[200:260:5] *= -1            # corrupt 12 symbols (pi rotation)
   * >>> met = BerMeter(m=4)
   * >>> met.set_truth(truth)
   * 0
   * >>> met.align(rx, n_marker=64)
   * 1
   * >>> _ = met.score(rx, hi=truth.size)
   * >>> r = met.ser()
   * >>> r.errors, r.symbols
   * (12, 736)
   * >>> round(r.lo, 4)                 # assert on lo, never on p_hat
   * 0.0067
   *
   * @endcode
   */
  ber_interval_t ber_meter_ser (const ber_meter_state_t *state);

  /**
   * @brief Gray-coded bit error rate over the scored bits, with its interval.
   *
   * The same exact statistics as ser(), counted over Gray-coded bits rather
   * than symbols, so a QPSK/8PSK symbol error contributes as many bit errors
   * as its Gray labels differ by. Assert on @c lo, never on @c p_hat.
   *
   * @param state  Must be non-NULL.
   * @return A BerInterval record — `(p_hat, lo, hi, rel, conf, errors,
   *         symbols)` — the bit error rate with its exact two-sided limits
   *         (`errors` and `symbols` are bit counts here).
   * @code
   * >>> import numpy as np
   * >>> from doppler.ber import BerMeter
   * >>> rng = np.random.default_rng(1)
   * >>> truth = rng.integers(0, 4, size=800).astype(np.uint8)
   * >>> ang = 2 * np.pi * truth / 4 + np.pi / 4
   * >>> rx = np.exp(1j * ang).astype(np.complex64)
   * >>> rx[200:260:5] *= -1            # corrupt 12 symbols
   * >>> met = BerMeter(m=4)
   * >>> met.set_truth(truth)
   * 0
   * >>> met.align(rx, n_marker=64)
   * 1
   * >>> _ = met.score(rx, hi=truth.size)
   * >>> r = met.ber()                  # same statistics as ser(), over bits
   * >>> r.errors, r.symbols
   * (24, 1472)
   * >>> round(r.lo, 4)
   * 0.009
   *
   * @endcode
   */
  ber_interval_t ber_meter_ber (const ber_meter_state_t *state);

  size_t ber_meter_get_errors (const ber_meter_state_t *state);
  size_t ber_meter_get_symbols (const ber_meter_state_t *state);
  size_t ber_meter_get_bit_errors (const ber_meter_state_t *state);
  size_t ber_meter_get_bits (const ber_meter_state_t *state);
  size_t ber_meter_get_skipped (const ber_meter_state_t *state);
  int    ber_meter_get_m (const ber_meter_state_t *state);
  size_t ber_meter_get_target_errors (const ber_meter_state_t *state);
  int    ber_meter_get_lag (const ber_meter_state_t *state);
  double ber_meter_get_phase (const ber_meter_state_t *state);
  double ber_meter_get_align_stat (const ber_meter_state_t *state);
  double ber_meter_get_align_margin_db (const ber_meter_state_t *state);
  double ber_meter_get_align_runner_db (const ber_meter_state_t *state);
  size_t ber_meter_get_align_occurrences (const ber_meter_state_t *state);
  size_t ber_meter_get_align_slips (const ber_meter_state_t *state);
  int    ber_meter_get_align_saturated (const ber_meter_state_t *state);
  int    ber_meter_get_align_ok (const ber_meter_state_t *state);
  double ber_meter_get_conf (const ber_meter_state_t *state);

  size_t ber_meter_state_bytes (const ber_meter_state_t *state);
  void   ber_meter_get_state (const ber_meter_state_t *state, void *blob);
  int    ber_meter_set_state (ber_meter_state_t *state, const void *blob);

double theory_ser(int m, double esn0);
double theory_ber(int m, double esn0);
double esn0_db_for_ser(int m, double ser);
double evm_scatter_floor_db(int m);
size_t settle_syms(double bn_timing, double bn_carrier);
long lock_symbol(const uint8_t *flags, size_t flags_len, size_t sustain, double min_frac);

#ifdef __cplusplus
}
#endif

#endif /* BER_METER_CORE_H */
