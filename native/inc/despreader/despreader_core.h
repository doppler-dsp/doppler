/**
 * @file despreader_core.h
 * @brief Continuous DSSS despreader — Costas carrier loop + DLL code loop.
 *
 * A complete continuous despreader for a DSSS-BPSK signal: it composes a
 * @ref costas_state_t carrier loop and a @ref dll_state_t code loop on a single
 * shared per-sample integrate-and-dump. Per sample it wipes the carrier
 * (costas_wipeoff, integer NCO) and feeds the de-rotated sample to the DLL's
 * early/prompt/late correlators (dll_accumulate); per code period it dumps the
 * prompt and updates both loops — the code loop on the early/late envelopes, the
 * carrier loop on the same prompt symbol. `steps()` emits one prompt per period;
 * `bits()` bit-syncs the prompts into hard data bits (a data bit spans
 * `periods_per_bit` code periods).
 *
 * It is seeded by acquisition (the FFT search supplies the coarse carrier
 * frequency + code phase); the loops then track the residual. Set
 * `bn_fll > 0` for FLL-assisted carrier pull-in.
 *
 * Lifecycle: despreader_create -> [steps / bits / reset]* -> despreader_destroy.
 *
 * @code
 * uint8_t code[127] = { ... };  // one code period, 0/1 chips
 * despreader_state_t *ch = despreader_create(code, 127, 8, 0.0, 0.0,
 *                                       0.05, 0.005, 0.0, 0.707, 0.5, 1);
 * float complex sym[64];
 * size_t k = despreader_steps(ch, rx, rx_len, sym, 64);  // prompt per period
 * despreader_destroy(ch);
 * @endcode
 */
#ifndef DESPREADER_CORE_H
#define DESPREADER_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "costas/costas_core.h"
#include "dll/dll_core.h"
#include "jm_perf.h"
#include <complex.h>
#include "lo/lo_core.h"
#include "loop_filter/loop_filter_core.h"
#include "telemetry/telemetry.h"
#include "detection/detection_core.h"
#include "lockdet/lockdet_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Despreader state.
 *
 * Allocate with despreader_create(). Embeds the carrier (`car`) and code (`code`)
 * loops by value; the despreader owns the copied spreading code and the
 * bit-sync histogram.
 */
typedef struct {
    costas_state_t car;   /**< carrier (Costas/FLL-assisted-PLL) loop.    */
    dll_state_t code;     /**< code (early/prompt/late DLL) loop.         */
    uint8_t *code_copy;   /**< owned copy of the spreading code.          */
    size_t periods_per_bit;    /**< code periods per data bit (>=1).           */
    /* bit-sync (used only when periods_per_bit > 1) */
    size_t *flip_hist;    /**< prompt sign-flip histogram, length np.     */
    size_t epoch_count;   /**< code periods processed so far.             */
    size_t bit_phase;     /**< detected bit boundary (argmax flip_hist).  */
    size_t epochs_in_bit; /**< periods accumulated in the current bit.    */
    double bit_acc;       /**< running sum of Re(prompt) over the bit.    */
    int prev_sign;        /**< previous prompt sign (+1/-1).              */
    int have_prev;        /**< prev_sign valid.                           */
    dp_tlm_t *tlm_ctx;    /**< telemetry gate: non-NULL when the embedded
                               loops are attached (despreader_set_telemetry);
                               the probes live on car.tlm / code.tlm. Not
                               serialized (field-wise triplet skips it).   */
} despreader_state_t;

/**
 * @brief Initialise a despreader in place; BORROWS @p code.
 *
 * The by-value counterpart to despreader_create(): the caller retains ownership of
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
 * @param periods_per_bit      Code periods per data bit (1 = one bit per period).
 */
void despreader_init(despreader_state_t *ch, const uint8_t *code, size_t code_len,
                  size_t sps, double init_norm_freq, double init_chip,
                  double bn_carrier, double bn_code, double bn_fll, double zeta,
                  double spacing, size_t periods_per_bit);

/**
 * @brief Create a despreader (COPIES @p code).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call despreader_destroy() when done.
 */
despreader_state_t *despreader_create(const uint8_t *code, size_t code_len, size_t sps, double init_norm_freq, double init_chip, double bn_carrier, double bn_code, double bn_fll, double zeta, double spacing, size_t periods_per_bit);

/**
 * @brief Destroy a despreader and release all memory.
 * @param state  May be NULL.
 */
void despreader_destroy(despreader_state_t *state);

/**
 * @brief Re-seed both loops to the create-time frequency/phase; keep config.
 * @param state  Must be non-NULL.
 */
void despreader_reset(despreader_state_t *state);

size_t despreader_steps_max_out(despreader_state_t *state);
size_t despreader_steps(despreader_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
size_t despreader_bits_max_out(despreader_state_t *state);
size_t despreader_bits(despreader_state_t *state, const float complex *x, size_t x_len, uint8_t *out, size_t max_out);
double despreader_get_norm_freq(const despreader_state_t *state);
void despreader_set_norm_freq(despreader_state_t *state, double val);
double despreader_get_code_phase(const despreader_state_t *state);
double despreader_get_code_rate(const despreader_state_t *state);
double despreader_get_lock_metric(const despreader_state_t *state);
size_t despreader_get_bit_phase(const despreader_state_t *state);
double despreader_get_bn_carrier(const despreader_state_t *state);
void despreader_set_bn_carrier(despreader_state_t *state, double val);
double despreader_get_bn_code(const despreader_state_t *state);
void despreader_set_bn_code(despreader_state_t *state, double val);

/**
 * @brief Attach (or detach) a telemetry context across the despreader.
 * Pure forwarder — the despreader registers no probes of its own: the
 * carrier loop registers "<prefix>.car.lock" / ".e" / ".freq" and the
 * code loop registers "<prefix>.code.e" / ".rate" / ".lock" — six
 * probes, all thinned by @p decim and emitted once per code period (the
 * despreader flushes both loops at its per-period update).  Passing NULL
 * detaches both loops.  Setup path, never hot; the context is borrowed
 * and must outlive the attachment (SPSC rules in telemetry/telemetry.h).
 * @param state  Must be non-NULL.
 * @param tlm    Telemetry context to attach, or NULL to detach.
 * @param prefix Probe-name prefix, e.g. "ch0".
 * @param decim  Emit every decim-th code period; >= 1.
 * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
 *         six probes (the attach fails whole; everything detached).
 * @code
 * >>> import numpy as np
 * >>> from doppler.dsss import Despreader
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> code = (np.arange(31) % 2).astype(np.uint8)
 * >>> ch = Despreader(code=code, sps=4)
 * >>> ch.set_telemetry(tlm, "ch0")
 * >>> sorted(tlm.probe_names())
 * ['ch0.car.e', 'ch0.car.freq', 'ch0.car.lock', 'ch0.code.e', 'ch0.code.lock', 'ch0.code.rate']
 * >>> chips = 1.0 - 2.0 * (np.arange(31) % 2)
 * >>> x = np.tile(np.repeat(chips, 4), 40).astype(np.complex64)
 * >>> _ = ch.steps(x)
 * >>> recs = tlm.read()   # six records per code period
 * >>> len(recs) > 0 and len(recs) % 6 == 0
 * True
 *
 * @endcode
 */
int despreader_set_telemetry(despreader_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: costas + dll children + running bit-sync histogram/state;
 * the owned code copy is restored by create. */
#define DESPREADER_STATE_MAGIC DP_FOURCC ('D','S','P','R')
#define DESPREADER_STATE_VERSION 3u /* v3: dll child grew (lockdet rule) */
size_t despreader_state_bytes (const despreader_state_t *state);
void despreader_get_state (const despreader_state_t *state, void *blob);
int despreader_set_state (despreader_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DESPREADER_CORE_H */
