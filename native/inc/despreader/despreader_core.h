/**
 * @file despreader_core.h
 * @brief Despreader component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * despreader_state_t *obj = despreader_create(NULL, 0, 1, 2, 0.0, 0.0, 0.01, 0.002);
 * float complex y = despreader_step(obj, 0.0f + 0.0f * I);
 * despreader_destroy(obj);
 * @endcode
 */
#ifndef DESPREADER_CORE_H
#define DESPREADER_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "loop_filter/loop_filter_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Despreader state.
 *
 * Allocate with despreader_create().
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

  /* ── status read-backs ── */
  double lock_metric; /**< EMA of |Re P|/|P|, ~1 when phase-locked.   */
  double snr_est;     /**< EMA SNR estimate from the prompt symbols.  */
} despreader_state_t;

/**
 * @brief Create a despreader instance.
 *
 * @param code  Data spreading code (0/1 chips), length @p code_len; copied.
 * @param code_len  Length of @p code in chips (>= sf).
 * @param sf  sf (default: 1).
 * @param sps  sps (default: 2).
 * @param init_norm_freq  init_norm_freq (default: 0.0).
 * @param init_chip_phase  init_chip_phase (default: 0.0).
 * @param bn_carrier  bn_carrier (default: 0.01).
 * @param bn_code  bn_code (default: 0.002).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call despreader_destroy() when done.
 */
despreader_state_t *despreader_create(const uint8_t *code, size_t code_len, size_t sf, size_t sps, double init_norm_freq, double init_chip_phase, double bn_carrier, double bn_code);

/**
 * @brief Enable preamble-aided pull-in with a distinct acquisition code.
 *
 * Track @p acq_reps periods of @p acq_code coherently (the unmodulated,
 * repeated acquisition preamble — a full ±pi phase discriminator, so the loops
 * pull in even a wide residual) before switching to the data code for the
 * payload. Call before feeding the burst; the acq mode clears automatically
 * once the preamble is consumed, and re-arms on despreader_reset().
 *
 * @param state         Must be non-NULL.
 * @param acq_code      Acquisition code (0/1), length acq_code_len; copied.
 * @param acq_code_len  Acquisition code length in chips.
 * @param acq_reps      Number of acq-code periods in the preamble.
 */
void despreader_set_acq(despreader_state_t *state, const uint8_t *acq_code,
                        size_t acq_code_len, size_t acq_reps);

/**
 * @brief Destroy a despreader instance and release all memory.
 * @param state  May be NULL.
 */
void despreader_destroy(despreader_state_t *state);

/**
 * @brief Reset Despreader to its post-create state.
 * @param state  Must be non-NULL.
 */
void despreader_reset(despreader_state_t *state);









/** @brief Upper bound on symbols `despreader_steps` can emit (0; the caller
 *  sizes the output buffer to the input length, which always suffices). */
size_t despreader_steps_max_out (despreader_state_t *state);

/**
 * @brief Despread a CF32 block; emit one complex prompt symbol per code period.
 *
 * Streams: a partial symbol is carried in state across calls. Each emitted
 * symbol is the complex prompt integrate-and-dump (carrier-wiped, code-stripped)
 * — its sign is the BPSK decision, its phase/magnitude the soft information.
 * During a `despreader_set_acq` preamble no symbols are emitted (the loops are
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
 * despreader_state_t *d = despreader_create(code, n, 32, 2, f0, chip, .05, .01);
 * float complex sym[256];
 * size_t k = despreader_steps(d, rx, rx_len, sym, 256);
 * // hard bit of sym[i] = crealf(sym[i]) >= 0
 * despreader_destroy(d);
 * @endcode
 */
size_t despreader_steps (despreader_state_t *state, const float complex *x,
                         size_t x_len, float complex *out, size_t max_out);

/** @brief Upper bound on bits `despreader_bits` can emit (0; see
 *  despreader_steps_max_out). */
size_t despreader_bits_max_out (despreader_state_t *state);

/**
 * @brief Despread a CF32 block; emit one hard BPSK bit (0/1) per code period.
 *
 * Same streaming kernel as despreader_steps(), but emits the hard decision
 * `crealf(prompt) >= 0` instead of the complex symbol.
 *
 * @param state    Must be non-NULL.
 * @param x        Input CF32 samples, length @p x_len.
 * @param x_len    Number of input samples.
 * @param out      Output buffer for bits (>= max_out).
 * @param max_out  Capacity of @p out in bits.
 * @return Number of bits written.
 */
size_t despreader_bits (despreader_state_t *state, const float complex *x,
                        size_t x_len, uint8_t *out, size_t max_out);

/** @brief Carrier (Costas) loop noise bandwidth, normalized to the symbol rate. */
double despreader_get_bn_carrier (const despreader_state_t *state);
/** @brief Set the carrier loop bandwidth (recomputes the loop gains). */
void despreader_set_bn_carrier (despreader_state_t *state, double val);
/** @brief Code (DLL) loop noise bandwidth, normalized to the symbol rate. */
double despreader_get_bn_code (const despreader_state_t *state);
/** @brief Set the code loop bandwidth (recomputes the loop gains). */
void despreader_set_bn_code (despreader_state_t *state, double val);
/** @brief Current carrier frequency estimate, cycles/sample. */
double despreader_get_norm_freq (const despreader_state_t *state);
/** @brief Override the carrier frequency estimate, cycles/sample (re-seed). */
void despreader_set_norm_freq (despreader_state_t *state, double val);
/** @brief Current tracked code phase within the symbol, chips. */
double despreader_get_code_phase (const despreader_state_t *state);
/** @brief Lock indicator in [0,1] (EMA of |Re prompt|/|prompt|; ~1 = locked). */
double despreader_get_lock_metric (const despreader_state_t *state);
/** @brief Post-despread SNR estimate (EMA of (Re prompt)^2 / (Im prompt)^2). */
double despreader_get_snr_est (const despreader_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DESPREADER_CORE_H */
