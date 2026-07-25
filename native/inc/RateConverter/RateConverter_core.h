/**
 * @file RateConverter_core.h
 * @brief Optimal-speed rate conversion cascade.
 *
 * Selects the cheapest cascade of CIC, HalfbandDecimator, and/or polyphase
 * Resampler stages for a given output/input sample rate ratio at creation
 * time.  All sub-stage C objects are owned by the state struct.
 *
 * Stage selection (D = 1/rate):
 *
 *   rate >= 1.0 or D < 2           `[Resampler(rate)]`
 *   D ~= 2^1                        `[HalfbandDecimator]`
 *   D ~= 2^2                        `[HalfbandDecimator, HalfbandDecimator]`
 *   D ~= 2^n, n>=3, D<=4096         `[CIC(D)]`
 *   D >= 8, non-power-of-2          `[CIC(R*), Resampler correction]`
 *                                    R* = nearest power-of-2 to D
 *   otherwise (2 <= D < 8, non-int) `[Resampler(rate)]`
 *
 * **INPUT AMPLITUDE IS BOUNDED whenever the plan contains a CIC stage** —
 * that is, any decimation by 8 or more: |Re| and |Im| <= 1.0, clipped beyond
 * that, before any filtering.  `stages` is how you tell: a plan naming
 * `CIC(...)` is not scale-free, every other plan is.  This is the one
 * property of this object a caller cannot infer from an output that is finite
 * and looks plausible — an overdriven RRC-BPSK waveform (peak 1.29)
 * matched-filters to -25 dB EVM where the same waveform scaled to peak 0.32
 * reaches -50 dB.
 *
 * Lifecycle:
 * @code
 *   RateConverter_state_t *rc = RateConverter_create(0.1, 0);
 *   // rc->n_stages == 2: CIC(8) then Resampler(0.8)
 *   float _Complex out[512];
 *   size_t n = RateConverter_execute(rc, in, 4096, out, 512);
 *   RateConverter_destroy(rc);
 * @endcode
 */
#ifndef RATE_CONVERTER_CORE_H
#define RATE_CONVERTER_CORE_H

#include "clib_common.h"
#include "dp_state.h"

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "resamp/resamp_core.h"
#include "fir/fir_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Maximum number of visible cascade stages. */
#define RC_MAX_STAGES 3

/** Stage type tags. */
typedef enum
{
  RC_STAGE_HB     = 0, /**< HalfbandDecimator (2:1, CF32) */
  RC_STAGE_CIC    = 1, /**< CIC decimator (optionally with comp FIR) */
  RC_STAGE_RESAMP = 2, /**< Polyphase Resampler (any positive rate) */
} rc_stage_t;

/**
 * @brief Matched-filter pulse selection for the terminal stage.
 *
 * A cascade whose terminal stage carries a *pulse-shaped* bank instead of the
 * default Kaiser anti-alias bank IS the matched filter — the same dot product
 * does the rate conversion and the matched filtering, and its arm is the
 * fractional timing delay a downstream loop steers. Values 0/1 match
 * `MPSK_RX_PULSE_IANDD`/`_RRC` so one vocabulary covers the family.
 */
typedef enum
{
  RC_PULSE_IANDD = 0, /**< rectangular: integrate-and-dump boxcar.        */
  RC_PULSE_RRC   = 1, /**< root-raised cosine, roll-off `beta`.           */
  RC_PULSE_NONE  = 2, /**< default: Kaiser anti-alias (plain conversion). */
} rc_pulse_t;

/**
 * @brief Cascade state -- owns all sub-stage C objects.
 *
 * Do not initialise directly; use RateConverter_create().
 */
typedef struct
{
  double         rate;                        /**< current rate ratio        */
  int            compensate;                  /**< CIC droop-comp flag       */
  int            n_stages;                    /**< active stage count        */
  rc_stage_t     stage_types[RC_MAX_STAGES];  /**< stage type per slot       */
  void          *stage_ptrs[RC_MAX_STAGES];   /**< sub-object per slot       */
  /** Ping-pong intermediate buffers, grown lazily on first execute. */
  float _Complex *bufs[2];
  size_t          buf_cap;
  /* Matched-filter configuration (RC_PULSE_NONE = plain Kaiser terminal
     bank, i.e. everything RateConverter_create() builds).  Kept so
     RateConverter_set_rate() can re-plan without losing the pulse. */
  int    pulse;      /**< rc_pulse_t; RC_PULSE_NONE when not matched  */
  double beta;       /**< RRC roll-off                                */
  size_t span;       /**< one-sided RRC span, symbols                 */
  double pulse_sps;  /**< symbol period in OUTPUT samples             */
  size_t num_phases; /**< terminal-stage arms (power of two)          */
  /** Set when a rectangular pulse was selected with fewer than four output
      samples per symbol, where its matched filter degenerates to a 2-3 tap
      sum.  Read by the binding, which turns it into a UserWarning. */
  bool narrow_pulse;
} RateConverter_state_t;

/**
 * @brief Create a rate converter for the given output/input rate ratio.
 * Selects the cheapest cascade of CIC, HalfbandDecimator, and/or
 * polyphase Resampler stages at construction time (see file header for
 * the selection table). Setting compensate=1 appends a closed-form
 * Molnar-Vucic CIC droop-compensating FIR after any CIC stage, which
 * improves passband flatness at the cost of one extra FIR stage.
 *
 * @param rate       Output-to-input sample rate ratio. Any positive float.
 * @param compensate Non-zero to append a CIC passband-droop compensating
 *                   FIR after any CIC stage.
 * @return Non-NULL on success; NULL if rate <= 0 or OOM.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> rc = RateConverter(rate=0.5, compensate=0)
 * >>> rc.rate
 * 0.5
 * @endcode
 */
RateConverter_state_t *RateConverter_create (double rate, int compensate);

/**
 * @brief Create a rate converter whose terminal stage IS a matched filter.
 *
 * Plans the same cheap cascade as RateConverter_create(), then puts a
 * pulse-shaped polyphase bank on the **terminal** stage instead of the default
 * Kaiser one. The cascade therefore does rate conversion and matched filtering
 * in a single dot product, and that stage's polyphase arm is the fractional
 * timing delay — which is what makes RateConverter_execute_ctrl() a timing
 * control port rather than just a Doppler knob.
 *
 * Three things this does that plain create() cannot:
 *
 * - **The terminal fractional stage always exists.** The ordinary planner drops
 *   it for an exact power-of-two decimation, and again when the correction
 *   lands within 1e-6 of 1.0 — so `rate = 2/64` plans a bare `CIC(32)` with
 *   nothing steerable at the end. Here the terminal stage is simultaneously the
 *   matched filter and the timing element, so it is appended (at rate 1.0 if
 *   there is no rate left to correct).
 * - **The bank is sized by the POST-decimation rate.** Matched-filtering at the
 *   input rate costs taps proportional to the input samples per symbol (4225
 *   taps/arm at 256 samples/symbol — 17 MB of bank); after the integer stages
 *   have done the bulk decimation it is ~`2*span*pulse_sps` taps, constant in
 *   the input rate.
 * - **CIC droop folds into the bank**, exactly rather than approximately: the
 *   Molnar-Vucic compensator (ciccompmf) runs at the decimated rate, which IS
 *   the terminal stage's tap grid, so the fold is a per-arm convolution and
 *   costs no extra stage. `compensate` therefore adds no FIR on this path.
 *
 * Measured on RRC-BPSK (beta 0.35, span 8, two outputs per symbol), best-case
 * timing phase, noiseless: a halfband cascade reaches -60 dB EVM; a CIC
 * cascade reaches **-50 dB with `compensate = 1` and only -22 dB without**, so
 * on this path compensation is not a refinement — it is 28 dB, for six extra
 * taps per arm and no extra pass over the data. Folded or appended agree to
 * within 0.6 dB, i.e. the fold gives up nothing to a separate comp FIR.
 *
 * @note Keep the INPUT inside +-1.0 whenever the plan contains a CIC stage —
 * see the file header.  The clip is silent, and it costs 25 dB of the EVM
 * quoted above for reasons that have nothing to do with the matched filter.
 *
 * @param rate       Output-to-input sample rate ratio (any positive float).
 *                   Rate-agnostic: this object never learns about symbols —
 *                   a caller wanting `m` samples per symbol asks for
 *                   `rate = m/sps`.
 * @param compensate Non-zero to correct CIC passband droop (folded into the
 *                   bank here, not appended as a stage).
 * @param pulse      RC_PULSE_RRC / RC_PULSE_IANDD. RC_PULSE_NONE is invalid
 *                   here — use RateConverter_create() for a plain conversion.
 * @param beta       RRC roll-off in `[0, 1]` (ignored for the rectangle).
 * @param span       One-sided RRC span in symbols (ignored for the rectangle,
 *                   whose support is always exactly one symbol).
 * @param pulse_sps  The pulse's period measured in **output** samples (2 =
 *                   two samples per symbol out). This is a shape parameter,
 *                   not a rate-planning one: a matched filter has a symbol
 *                   duration, and the planner still knows nothing of symbols.
 * @param num_phases Terminal-stage arms; power of two. Sets the fractional
 *                   timing resolution to `1/num_phases` of an output period.
 * @return Non-NULL on success; NULL on a bad parameter or OOM.
 */
RateConverter_state_t *
RateConverter_create_matched (double rate, int compensate, int pulse,
                              double beta, size_t span, double pulse_sps,
                              size_t num_phases);

/**
 * @brief Has any planned CIC stage clipped its input since the last reset?
 *
 * The cascade inherits cic_core's input bound (|Re|, |Im| <= 1.0) whenever the
 * plan contains a CIC — any decimation by 8 or more — and the clip does not
 * show up in the samples: the output stays finite and plausible, merely
 * distorted.  This is the only reliable way to find out, and it is free (the
 * boundary comparisons run on every sample regardless).
 *
 * Sticky, cleared by RateConverter_reset().  Always 0 for a cascade with no
 * CIC stage, which is the honest answer: those plans are scale-free.
 *
 * @param s  Pointer to a valid RateConverter_state_t.
 * @return 1 if any CIC stage has clipped, else 0.
 */
bool RateConverter_get_clipped (const RateConverter_state_t *s);

/**
 * @brief Is this converter's rectangular matched filter degenerately narrow?
 *
 * True only for a matched cascade built with RC_PULSE_IANDD and
 * `pulse_sps < 4`: the rectangle is exactly one symbol wide, so its matched
 * filter is a 2-3 tap sum there.  It works — it just barely opens the eye
 * (measured on the timing loop this feeds, a lock statistic of -0.34 at two
 * samples per symbol against +0.95 at four).  The RRC spans many symbols and
 * is never affected.  Construction also raises a UserWarning.
 */
bool RateConverter_get_narrow_pulse (const RateConverter_state_t *s);

/** @brief Number of planned cascade stages (backs the `stages` property). */
size_t RateConverter_num_stages (const RateConverter_state_t *s);
/**
 * @brief Label of stage @p i, e.g. "CIC(8)+FIR" or "Resampler(0.923,rrc)".
 *
 * Points at a per-thread scratch buffer valid until this thread's next call —
 * the binding converts it to a Python string immediately.  NULL if @p i is
 * out of range.
 */
const char *RateConverter_stages_value (const RateConverter_state_t *s,
                                        size_t i);

/**
 * @brief Terminal polyphase bank shape (backs the `bank_shape` property).
 * @return 2 when the cascade ends in a Resampler stage, else 0.
 */
size_t RateConverter_num_bank_shape (const RateConverter_state_t *s);
/** @brief Element @p i of the bank shape: 0 -> num_phases, 1 -> num_taps. */
size_t RateConverter_bank_shape_value (const RateConverter_state_t *s,
                                       size_t i);

/** @brief Free all resources.  NULL is a no-op. */
void RateConverter_destroy (RateConverter_state_t *s);

/**
 * @brief Zero all sub-stage filter memories.
 * Rate, stage count, and stage types are preserved. Processing from a
 * reset state produces the same output as a freshly created converter
 * fed the same input. Use between signal bursts to suppress transient
 * artefacts from prior filter memory.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> rc = RateConverter(rate=0.5, compensate=0)
 * >>> rc.reset()
 * >>> rc.rate
 * 0.5
 * @endcode
 */
void RateConverter_reset (RateConverter_state_t *s);

/* Serializable state (standard bytes interface; see dp_state.h): the standard
 * envelope followed by the concatenated mutable state of the active cascade
 * stages (HB / CIC[+comp FIR] / Resampler), in cascade order — each a
 * self-contained sub-blob with its own leaf envelope.  The stage plan is config
 * (rebuilt from rate), so a same-rate RateConverter round-trips exactly. */
#define RC_STATE_MAGIC DP_FOURCC ('R', 'C', 'V', 'T')
#define RC_STATE_VERSION 1u

/** @brief Bytes RateConverter_get_state() writes for @p s (envelope + stages). */
size_t RateConverter_state_bytes (const RateConverter_state_t *s);
/** @brief Serialize @p s's active-stage state into @p blob. */
void RateConverter_get_state (const RateConverter_state_t *s, void *blob);
/** @brief Restore active-stage state from @p blob (same rate).
 *  @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects. */
int RateConverter_set_state (RateConverter_state_t *s, const void *blob);

/**
 * @brief Convert a block of CF32 samples through the cascade.
 * Passes input through each stage in order, ping-ponging between two
 * intermediate buffers. State persists between calls, so contiguous
 * calls on sequential blocks give the same result as one large call.
 * Output length is approximately n_in * rate.
 *
 * @param s        Pointer to a valid RateConverter_state_t.
 * @param in       CF32 input block.
 * @param n_in     Number of input samples.
 * @param out      Output buffer; must hold at least max_out samples.
 * @param max_out  Capacity of out in samples.
 * @return         CF32 output array; length is approximately n_in * rate.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> import numpy as np
 * >>> rc = RateConverter(rate=0.5, compensate=0)
 * >>> y = rc.execute(np.zeros(1024, dtype=np.complex64))
 * >>> y.shape, y.dtype
 * ((512,), dtype('complex64'))
 * @endcode
 */
size_t RateConverter_execute (RateConverter_state_t *s,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

/**
 * @brief Upper bound on execute output for a standard 65536-sample block.
 *
 * Returns (size_t)(65536 * max(rate, 1.0)) + 2.  The Python extension uses
 * this to pre-allocate the output buffer on the first execute call.
 */
size_t RateConverter_execute_max_out (RateConverter_state_t *s);

/** @brief As RateConverter_execute_max_out(), for the block control form. */
size_t RateConverter_execute_ctrl_max_out (RateConverter_state_t *s);
/** @brief Bound for ONE pushed input: `ceil(rate) + 1` output periods.
 *  Non-zero because the push form has no input block to size from. */
size_t RateConverter_execute_ctrl_push_max_out (RateConverter_state_t *s);

/**
 * @brief Convert a block, steering the cascade's fractional stage by @p ctrl.
 *
 * The control-port form of RateConverter_execute(): the fixed integer stages
 * (HalfbandDecimator / CIC) run unchanged, and the scalar rate deviation
 * @p ctrl is forwarded to the **terminal polyphase Resampler stage's**
 * accumulator (via resamp_execute_ctrl_push) — so its effective rate becomes
 * `stage_rate + ctrl` for this call. This exposes the fractional tail's control
 * port that RateConverter_execute() hides: a timing/rate-tracking loop can
 * decimate a high input rate cheaply through the HB/CIC stages and then
 * arbitrary-rate + strobe-align in the last stage, updating @p ctrl per block.
 *
 * `ctrl` is referenced to the terminal stage's (post-decimation) rate, not the
 * overall rate. It is meaningful only when the cascade actually ends in a
 * Resampler stage; a pure integer HB/CIC cascade has no fractional stage to
 * steer, so this **falls through to RateConverter_execute()** (ctrl ignored).
 *
 * @param s        Pointer to a valid RateConverter_state_t.
 * @param in       CF32 input block.
 * @param n_in     Number of input samples.
 * @param ctrl     Rate deviation added to the terminal Resampler stage's rate.
 * @param out      Output buffer; must hold at least max_out samples.
 * @param max_out  Capacity of out in samples.
 * @return CF32 output count.
 */
size_t RateConverter_execute_ctrl (RateConverter_state_t *s,
                                   const float _Complex *in, size_t n_in,
                                   double ctrl, float _Complex *out,
                                   size_t max_out);

/**
 * @brief Push ONE input sample; emit whatever outputs it completes.
 *
 * The per-input streaming form of RateConverter_execute_ctrl(), and the only
 * form a closed loop can use: a block call must know its whole `ctrl` history
 * up front, whereas a timing loop computes each correction *from* the outputs
 * already emitted. Feeding a stream one sample at a time through this
 * reproduces RateConverter_execute_ctrl() on the same block bit-for-bit when
 * @p ctrl is held constant (the cascade is block-boundary invariant), so the
 * cheap block form stays correct for open-loop use.
 *
 * The integer HB/CIC stages consume the sample and emit at most one
 * intermediate sample each; the terminal Resampler stage then emits 0 outputs
 * (a decimator between strobes — the common case), 1, or several (an
 * interpolator). A cascade with no terminal Resampler ignores @p ctrl.
 *
 * @param s        Pointer to a valid RateConverter_state_t.
 * @param x        One CF32 input sample.
 * @param ctrl     Rate deviation added to the terminal stage's rate for this
 *                 input (referenced to the terminal, post-decimation rate).
 * @param out      Output buffer for any emitted samples.
 * @param max_out  Capacity of @p out (emission stops at this bound).
 * @return Number of outputs written to @p out (0, 1, or more).
 */
size_t RateConverter_execute_ctrl_push (RateConverter_state_t *s,
                                        float _Complex x, double ctrl,
                                        float _Complex *out, size_t max_out);

/**
 * @brief Get / set the output-to-input sample rate ratio.
 * The setter rebuilds the entire cascade (new stage selection, new
 * sub-objects) and resets all filter memories — equivalent to
 * destroying and recreating with the new rate. Setting rate <= 0 is
 * silently ignored.
 *
 * @code
 * >>> from doppler.resample import RateConverter
 * >>> rc = RateConverter(rate=0.5, compensate=0)
 * >>> rc.rate
 * 0.5
 * >>> rc.rate = 2.0
 * >>> rc.rate
 * 2.0
 * @endcode
 */
double RateConverter_get_rate (const RateConverter_state_t *s);

/**
 * @brief Change the rate; rebuilds the cascade and resets all filter state.
 * Silently ignores rate <= 0.
 *
 * @param s     Pointer to a valid RateConverter_state_t.
 * @param rate  New output/input rate ratio.
 */
void RateConverter_set_rate (RateConverter_state_t *s, double rate);

/**
 * @brief Write a human-readable label for stage i into buf.
 *
 * Examples: "HalfbandDecimator", "CIC(8)", "CIC(8)+FIR", "Resampler(0.8)".
 *
 * @param s    Must be non-NULL.
 * @param i    Stage index in `[0, s->n_stages)`.
 * @param buf  Output buffer.
 * @param len  Capacity of buf in bytes.
 * @return 1 on success, 0 if i is out of range.
 */
int RateConverter_stage_label (RateConverter_state_t *s, int i,
                               char *buf, size_t len);

/**
 * @brief One-shot rate conversion — no persistent state required.
 *
 * Creates a temporary converter, converts n_in samples, destroys it.
 * Equivalent to:
 * @code
 * RateConverter_state_t *rc = RateConverter_create(rate, compensate);
 * size_t n = RateConverter_execute(rc, in, n_in, out, max_out);
 * RateConverter_destroy(rc);
 * @endcode
 *
 * Use RateConverter_create() directly when processing multiple blocks at
 * the same rate — the one-shot form resets filter memory on every call.
 *
 * @param rate       Output-to-input sample rate ratio.
 * @param compensate Non-zero to enable CIC droop compensation.
 * @param in         CF32 input samples.
 * @param n_in       Number of input samples.
 * @param out        Output buffer.
 * @param max_out    Output buffer capacity in samples.
 * @return Number of output samples written; 0 only if OOM or n_in == 0.
 */
size_t RateConverter_convert (double rate, int compensate,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* RATE_CONVERTER_CORE_H */
