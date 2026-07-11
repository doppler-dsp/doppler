/**
 * @file burst_despreader_core.h
 * @brief BurstDespreader component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * burst_despreader_state_t *obj = burst_despreader_create(NULL, 0, 1, 2, 0.0, 0.0, 0.05, 0.01);
 * float complex y = burst_despreader_step(obj, 0.0f + 0.0f * I);
 * burst_despreader_destroy(obj);
 * @endcode
 */
#ifndef BURST_DESPREADER_CORE_H
#define BURST_DESPREADER_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "loop_filter/loop_filter_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BurstDespreader state.
 *
 * Allocate with burst_despreader_create().
 */
typedef struct
{
  /* ── configuration (immutable after create) ── */
  uint8_t *code;   /**< owned spreading code, 0/1, length sf.          */
  size_t   sf;     /**< spreading factor = code length, chips/symbol.  */
  size_t   sps;    /**< samples per chip (>= 2).                       */
  size_t   tsamps; /**< sf*sps, symbol period in samples.             */
  double   seed_w; /**< create-time carrier angular freq, rad/sample. */
  double   seed_chip; /**< create-time code phase, chips, for reset.  */

  /* ── optional acquisition preamble (distinct acq code) ── */
  uint8_t *acq_code; /**< owned acq code, NULL if payload-only.        */
  size_t   acq_sf;   /**< acq code length, chips/period.               */
  size_t   acq_reps; /**< preamble periods to track before payload.    */
  size_t   preamble_left; /**< preamble periods still to consume.      */

  /* ── tracking loops (embedded by value, shared engine) ── */
  loop_filter_state_t lf_car;  /**< carrier (Costas) loop.            */
  loop_filter_state_t lf_code; /**< code (DLL) loop.                  */

  /* ── carrier NCO (inline, radians) ── */
  double car_phase; /**< current carrier phase, radians.             */
  double car_w;     /**< current carrier angular freq, rad/sample.   */

  /* ── code phase / integrate-and-dump ── */
  double        chip_pos;  /**< prompt code position within symbol, chips. */
  double        code_rate; /**< chips advanced per nominal chip (~1.0).    */
  float complex acc_e;     /**< early correlator accumulator.         */
  float complex acc_p;     /**< prompt correlator accumulator.        */
  float complex acc_l;     /**< late correlator accumulator.          */

  /* ── status read-backs: cumulative over the burst (reset re-arms) ── */
  double lock_metric; /**< mean of |Re P|/|P| over the burst (~1 locked,
                           ~2/pi with no carrier).                     */
  double snr_est;     /**< accumulate-then-ratio post-despread SNR:
                           (sum Re^2 - sum Im^2)/sum Im^2, clamped >= 0. */
  double sum_lock;    /**< running sum of |Re P|/|P|.                  */
  double sum_re2;     /**< running sum of Re(P)^2.                     */
  double sum_im2;     /**< running sum of Im(P)^2.                     */
  size_t stat_n;      /**< prompts folded into the burst statistics.   */
} burst_despreader_state_t;

/**
 * @brief Create a burst despreader instance.
 *
 * @param code  Data spreading code (0/1 chips), length @p code_len; copied.
 * @param code_len  Length of @p code in chips (>= sf).
 * @param sf  Spreading factor: chips integrated per prompt symbol
 *            (default: 1).
 * @param sps  Samples per chip (default: 2).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample — the
 *            acquisition estimate (default: 0.0).
 * @param init_chip_phase  Seed code phase, chips (default: 0.0).
 * @param bn_carrier  Carrier (Costas) loop noise bandwidth, normalized to
 *              the symbol rate (default: 0.05).
 * @param bn_code  Code (DLL) loop noise bandwidth, normalized to the
 *              symbol rate (default: 0.01).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call burst_despreader_destroy() when done.
 */
burst_despreader_state_t *burst_despreader_create(const uint8_t *code, size_t code_len, size_t sf, size_t sps, double init_norm_freq, double init_chip_phase, double bn_carrier, double bn_code);

/**
 * @brief Enable preamble-aided pull-in with a distinct acquisition code.
 *
 * Track @p acq_reps periods of @p acq_code coherently (the unmodulated,
 * repeated acquisition preamble — a full ±pi phase discriminator, so the loops
 * pull in even a wide residual) before switching to the data code for the
 * payload. Call before feeding the burst; the acq mode clears automatically
 * once the preamble is consumed, and re-arms on burst_despreader_reset().
 * NB: set_acq re-arms the PREAMBLE only — the cumulative burst statistics
 * (lock_metric / snr_est / lock_stat / stat_n) are re-armed by
 * burst_despreader_reset(); call it between bursts.
 *
 * @param state         Must be non-NULL.
 * @param acq_code      Acquisition code (0/1), length acq_code_len; copied.
 * @param acq_code_len  Acquisition code length in chips.
 * @param acq_reps      Number of acq-code periods in the preamble.
 */
void burst_despreader_set_acq(burst_despreader_state_t *state, const uint8_t *acq_code,
                        size_t acq_code_len, size_t acq_reps);

/**
 * @brief Destroy a burst despreader instance and release all memory.
 * @param state  May be NULL.
 */
void burst_despreader_destroy(burst_despreader_state_t *state);

/**
 * @brief Reset BurstDespreader to its post-create state.
 * @param state  Must be non-NULL.
 */
void burst_despreader_reset(burst_despreader_state_t *state);









/** @brief Upper bound on symbols `burst_despreader_steps` can emit (0; the caller
 *  sizes the output buffer to the input length, which always suffices). */
size_t burst_despreader_steps_max_out (burst_despreader_state_t *state);

/**
 * @brief Despread a CF32 block; emit one complex prompt symbol per code period.
 *
 * Streams: a partial symbol is carried in state across calls. Each emitted
 * symbol is the complex prompt integrate-and-dump (carrier-wiped, code-stripped)
 * — its sign is the BPSK decision, its phase/magnitude the soft information.
 * During a `burst_despreader_set_acq` preamble no symbols are emitted (the loops are
 * pulling in); payload symbols follow.
 *
 * @param state    Must be non-NULL.
 * @param x        Input CF32 samples, length @p x_len.
 * @param x_len    Number of input samples.
 * @param out      Output buffer for prompt symbols (>= max_out).
 * @param max_out  Capacity of @p out in symbols.
 * @return Number of symbols written.
 *
 * @code
 * // seed from acquisition (norm_freq cyc/sample, chip phase in chips):
 * burst_despreader_state_t *d = burst_despreader_create(code, n, 32, 2, f0, chip, .05, .01);
 * float complex sym[256];
 * size_t k = burst_despreader_steps(d, rx, rx_len, sym, 256);
 * // hard bit of sym[i] = crealf(sym[i]) >= 0
 * burst_despreader_destroy(d);
 * @endcode
 */
size_t burst_despreader_steps (burst_despreader_state_t *state, const float complex *x,
                         size_t x_len, float complex *out, size_t max_out);

/** @brief Upper bound on bits `burst_despreader_bits` can emit (0; see
 *  burst_despreader_steps_max_out). */
size_t burst_despreader_bits_max_out (burst_despreader_state_t *state);

/**
 * @brief Despread a CF32 block; emit one hard BPSK bit (0/1) per code period.
 *
 * Same streaming kernel as burst_despreader_steps(), but emits the hard decision
 * `crealf(prompt) >= 0` instead of the complex symbol.
 *
 * @param state    Must be non-NULL.
 * @param x        Input CF32 samples, length @p x_len.
 * @param x_len    Number of input samples.
 * @param out      Output buffer for bits (>= max_out).
 * @param max_out  Capacity of @p out in bits.
 * @return Number of bits written.
 */
size_t burst_despreader_bits (burst_despreader_state_t *state, const float complex *x,
                        size_t x_len, uint8_t *out, size_t max_out);

/** @brief Carrier (Costas) loop noise bandwidth, normalized to the symbol rate. */
double burst_despreader_get_bn_carrier (const burst_despreader_state_t *state);
/** @brief Set the carrier loop bandwidth (recomputes the loop gains). */
void burst_despreader_set_bn_carrier (burst_despreader_state_t *state, double val);
/** @brief Code (DLL) loop noise bandwidth, normalized to the symbol rate. */
double burst_despreader_get_bn_code (const burst_despreader_state_t *state);
/** @brief Set the code loop bandwidth (recomputes the loop gains). */
void burst_despreader_set_bn_code (burst_despreader_state_t *state, double val);
/** @brief Current carrier frequency estimate, cycles/sample. */
double burst_despreader_get_norm_freq (const burst_despreader_state_t *state);
/** @brief Override the carrier frequency estimate, cycles/sample (re-seed). */
void burst_despreader_set_norm_freq (burst_despreader_state_t *state, double val);
/** @brief Current tracked code phase within the symbol, chips. */
double burst_despreader_get_code_phase (const burst_despreader_state_t *state);
/** @brief Lock indicator in [0,1]: the mean of |Re prompt|/|prompt| over
 *          every prompt of the burst (cumulative, not EMA — a one-shot
 *          burst gives each prompt equal weight instead of spending the
 *          whole burst warming a smoother up). ~1 when phase-locked;
 *          ~2/pi (0.637) with no carrier (|cos theta|, uniform theta). */
double burst_despreader_get_lock_metric (const burst_despreader_state_t *state);
/** @brief Post-despread SNR estimate over the burst, accumulate-then-ratio:
 *          (sum Re^2 - sum Im^2) / sum Im^2, clamped >= 0.  For BPSK the
 *          signal lives in Re and the noise splits evenly, so this estimates
 *          A^2/sigma^2 (per-component) directly — unlike a per-symbol
 *          Re^2/Im^2 ratio, whose heavy-tailed reciprocal chi-square makes
 *          the estimate biased high with enormous variance.  This is the
 *          EFFECTIVE post-loop SNR: residual tracking-loop phase jitter
 *          rotates signal energy into Im, so the estimate sits below the
 *          AWGN-only value by the jitter term (converging as bn -> 0) —
 *          the quantity that actually predicts demodulation performance. */
double burst_despreader_get_snr_est (const burst_despreader_state_t *state);

/**
 * @brief Calibrated whole-burst lock statistic (the one-shot analog of the
 *        tracking loops' verify-counted detectors).
 *
 * R = sqrt(stat_n * sum Re^2 / sum Im^2): the burst's coherent (in-phase)
 * energy normalised by the noise power estimated from the quadrature arm.
 * Because the noise reference is estimated from the SAME number of samples
 * as the signal sum, the exact H0 law is R^2 = stat_n * F(stat_n, stat_n)
 * — NOT chi-square (a chi-square gate assumes a known noise power and
 * realizes tens of times the priced pfa here).  The closed-form gate is
 *
 *   locked_burst = R > sqrt(stat_n * det_threshold_f(pfa, stat_n))
 *
 * exact for every stat_n (odd included).  Only payload prompts fold into
 * the statistics — preamble prompts (different code length, pull-in
 * transients) are excluded so the H0 law and the SNR calibration hold.
 * Returns 0 before any payload prompt has been folded.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import BurstDespreader
 * >>> from doppler.detection import det_threshold_f
 * >>> code = (np.arange(31) % 2).astype(np.uint8)
 * >>> b = BurstDespreader(code=code, sf=31, sps=2)
 * >>> chips = 1.0 - 2.0 * (code % 2)
 * >>> x = np.tile(np.repeat(chips, 2), 64).astype(np.complex64)
 * >>> _ = b.steps(x)
 * >>> eta = np.sqrt(b.stat_n * det_threshold_f(1e-3, b.stat_n))
 * >>> bool(b.lock_stat > eta)   # a clean burst passes the pfa=1e-3 gate
 * True
 *
 * @endcode
 */
double burst_despreader_get_lock_stat (const burst_despreader_state_t *state);

/** @brief Number of prompts folded into the burst statistics so far. */
size_t burst_despreader_get_stat_n (const burst_despreader_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * whole-struct snapshot (loop_filter children POD-embedded);
 * the owned code + acq_code pointers are config, restored by create. */
#define BURST_DESPREADER_STATE_MAGIC DP_FOURCC ('B','D','S','P')
#define BURST_DESPREADER_STATE_VERSION 2u /* v2: cumulative burst statistics */
size_t burst_despreader_state_bytes (const burst_despreader_state_t *state);
void burst_despreader_get_state (const burst_despreader_state_t *state, void *blob);
int burst_despreader_set_state (burst_despreader_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BURST_DESPREADER_CORE_H */
