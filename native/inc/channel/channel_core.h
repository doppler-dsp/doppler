/**
 * @file channel_core.h
 * @brief GPS-style tracking channel — Costas carrier loop + DLL code loop.
 *
 * A complete tracking channel for a continuous DSSS-BPSK signal: it composes a
 * @ref costas_state_t carrier loop and a @ref dll_state_t code loop on a single
 * shared per-sample integrate-and-dump. Per sample it wipes the carrier
 * (costas_wipeoff, integer NCO) and feeds the de-rotated sample to the DLL's
 * early/prompt/late correlators (dll_accumulate); per code period it dumps the
 * prompt and updates both loops — the code loop on the early/late envelopes, the
 * carrier loop on the same prompt symbol. `steps()` emits one prompt per period;
 * `bits()` bit-syncs the prompts into hard data bits (a data bit spans
 * `nav_period` code periods).
 *
 * It is seeded by acquisition (the FFT search supplies the coarse carrier
 * frequency + code phase); the loops then track the residual. Set
 * `bn_fll > 0` for FLL-assisted carrier pull-in.
 *
 * Lifecycle: channel_create -> [steps / bits / reset]* -> channel_destroy.
 *
 * @code
 * uint8_t code[127] = { ... };  // one code period, 0/1 chips
 * channel_state_t *ch = channel_create(code, 127, 8, 0.0, 0.0,
 *                                       0.05, 0.005, 0.0, 0.707, 0.5, 1);
 * float complex sym[64];
 * size_t k = channel_steps(ch, rx, rx_len, sym, 64);  // prompt per period
 * channel_destroy(ch);
 * @endcode
 */
#ifndef CHANNEL_CORE_H
#define CHANNEL_CORE_H

#include "clib_common.h"
#include "costas/costas_core.h"
#include "dll/dll_core.h"
#include "jm_perf.h"
#include <complex.h>
#include "lo/lo_core.h"
#include "loop_filter/loop_filter_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tracking-channel state.
 *
 * Allocate with channel_create(). Embeds the carrier (`car`) and code (`code`)
 * loops by value; the channel owns the copied spreading code and the bit-sync
 * histogram.
 */
typedef struct {
    costas_state_t car;   /**< carrier (Costas/FLL-assisted-PLL) loop.    */
    dll_state_t code;     /**< code (early/prompt/late DLL) loop.         */
    uint8_t *code_copy;   /**< owned copy of the spreading code.          */
    size_t nav_period;    /**< code periods per data bit (>=1).           */
    /* bit-sync (used only when nav_period > 1) */
    size_t *flip_hist;    /**< prompt sign-flip histogram, length np.     */
    size_t epoch_count;   /**< code periods processed so far.             */
    size_t bit_phase;     /**< detected bit boundary (argmax flip_hist).  */
    size_t epochs_in_bit; /**< periods accumulated in the current bit.    */
    double bit_acc;       /**< running sum of Re(prompt) over the bit.    */
    int prev_sign;        /**< previous prompt sign (+1/-1).              */
    int have_prev;        /**< prev_sign valid.                           */
} channel_state_t;

/**
 * @brief Initialise a tracking channel in place; BORROWS @p code.
 *
 * The by-value counterpart to channel_create(): the caller retains ownership of
 * @p code (it is not copied or freed). Seeds the carrier NCO at
 * @p init_norm_freq and the code phase at @p init_chip (the acquisition
 * estimate). The carrier loop's update period is one code period
 * (`code_len * sps` samples).
 *
 * @param ch              State to initialise.  Must be non-NULL.
 * @param code            Spreading code (0/1 chips), one period; borrowed.
 * @param code_len        Code length (chips per period); >= 1.
 * @param sps             Samples per chip.
 * @param init_norm_freq  Seed carrier frequency, cycles/sample.
 * @param init_chip       Seed code phase, chips.
 * @param bn_carrier      Carrier loop noise bandwidth.
 * @param bn_code         Code loop noise bandwidth.
 * @param bn_fll          Carrier FLL-assist bandwidth (0 = pure PLL).
 * @param zeta            Damping factor for both loops.
 * @param spacing         DLL early/late tap offset, chips.
 * @param nav_period      Code periods per data bit (1 = one bit per period).
 */
void channel_init(channel_state_t *ch, const uint8_t *code, size_t code_len,
                  size_t sps, double init_norm_freq, double init_chip,
                  double bn_carrier, double bn_code, double bn_fll, double zeta,
                  double spacing, size_t nav_period);

/**
 * @brief Create a tracking channel (COPIES @p code).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call channel_destroy() when done.
 */
channel_state_t *channel_create(const uint8_t *code, size_t code_len, size_t sps, double init_norm_freq, double init_chip, double bn_carrier, double bn_code, double bn_fll, double zeta, double spacing, size_t nav_period);

/**
 * @brief Destroy a tracking channel and release all memory.
 * @param state  May be NULL.
 */
void channel_destroy(channel_state_t *state);

/**
 * @brief Re-seed both loops to the create-time frequency/phase; keep config.
 * @param state  Must be non-NULL.
 */
void channel_reset(channel_state_t *state);

size_t channel_steps_max_out(channel_state_t *state);
size_t channel_steps(channel_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
size_t channel_bits_max_out(channel_state_t *state);
size_t channel_bits(channel_state_t *state, const float complex *x, size_t x_len, uint8_t *out, size_t max_out);
double channel_get_norm_freq(const channel_state_t *state);
void channel_set_norm_freq(channel_state_t *state, double val);
double channel_get_code_phase(const channel_state_t *state);
double channel_get_code_rate(const channel_state_t *state);
double channel_get_lock_metric(const channel_state_t *state);
size_t channel_get_bit_phase(const channel_state_t *state);
double channel_get_bn_carrier(const channel_state_t *state);
void channel_set_bn_carrier(channel_state_t *state, double val);
double channel_get_bn_code(const channel_state_t *state);
void channel_set_bn_code(channel_state_t *state, double val);
#ifdef __cplusplus
}
#endif

#endif /* CHANNEL_CORE_H */
