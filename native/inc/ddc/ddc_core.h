/**
 * @file ddc_core.h
 * @brief Digital Down-Converter — composes LO + RateConverter cascade.
 *
 * Two types:
 *
 *   Ddc         — LO mix → RateConverter (the plain flavor)
 *   MatchedDDC  — the same, with the pulse on the cascade's terminal stage
 *
 * Streaming: any block size per execute call.  The real-input twin lives in
 * ddcr/ddcr_core.h (halfband R2C → LO mix → RateConverter); it is the same
 * chain behind a real-to-complex front end.
 *
 * RateConverter selects the cheapest cascade (CIC + optional halfband +
 * polyphase resampler) for the requested rate at create time.  This
 * makes large-ratio decimation (e.g., 100:1) significantly cheaper than
 * a single polyphase stage.
 *
 * ### Ddc signal chain
 *
 * ```
 * CF32 in (fs_in)  →  LO mix  →  RateConverter  →  CF32 out (fs_out)
 * ```
 *
 * norm_freq:  NCO normalised frequency (cycles/sample at fs_in).
 *             Set to -f_carrier to shift a carrier at f_carrier to DC.
 *
 * ### Pulse and the two control ports
 *
 * Both this type and its real-input twin have a matched *flavor*
 * (`ddc_create_matched` / `ddcr_create_matched`),
 * which is passed straight through to the cascade: the terminal stage carries
 * a matched-filter bank instead of the default Kaiser one, so the chain mixes,
 * decimates and matched-filters in the same dot products it was already doing
 * (see RateConverter_create_matched()).
 *
 * That makes a DDC steerable on **two** ports, which are duals of each other:
 *
 * ```
 *   freq_ctrl ──> LO phase accumulator      (carrier, at the INPUT rate)
 *   rate_ctrl ──> terminal stage accumulator (timing, at the OUTPUT rate)
 * ```
 *
 * Both are per-input deviations added on top of the configured centre value
 * for that sample only, so a tracking loop supplies its full filter output
 * every time and the DDC holds no loop state. A receiver therefore closes a
 * carrier loop and a timing loop with the same `loop_filter`, one per port —
 * the object itself contains no loop.
 *
 * The LO sits at the input rate (the intermediate rate fs_in/2 for DdcR),
 * which is where predetection de-rotation belongs: the carrier is wiped off
 * before any filter narrows the band around it.
 *
 * ### Retuning vs. rebuilding
 *
 * - **Retune** (centre-frequency change): call ddc_set_norm_freq /
 *   ddcr_set_norm_freq.  Cheap — updates the LO phase increment without
 *   disturbing the resampler history.  Seamless across block boundaries.
 * - **Rate change** (span / decimation change): destroy and recreate the
 *   DDC for the new rate.
 *
 * ### Usage
 *
 * @code
 * // Complex DDC: shift a carrier at +0.1·fs to DC, decimate by 4
 * ddc_state_t *ddc = ddc_create(-0.1, 0.25);
 * float _Complex out[4096];
 * size_t n = ddc_execute(ddc, in, 1024, out, 4096);
 * ddc_destroy(ddc);
 *
 * @endcode
 */
#ifndef DDC_CORE_H
#define DDC_CORE_H

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "lo/lo_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Ddc state — an LO and the cascade it feeds.
   *
   * Do not initialise directly; use ddc_create() or ddc_create_matched().
   */
  typedef struct ddc_state
  {
    lo_state_t            *lo; /**< carrier wipe-off, at the input rate */
    RateConverter_state_t *rc; /**< the cascade; matched when a pulse was
                                    selected at construction              */
    /** Set when the matched flavor was built with a rectangular pulse too
     *  narrow to be worth much — see ddc_create_matched(). Read by the
     *  binding, which turns it into a UserWarning at construction. */
    bool narrow_pulse;
  } ddc_state_t;

  /**
   * @brief Create a complex-input Digital Down-Converter.
   * Allocates internal state for the LO and RateConverter cascade.
   * The RateConverter selects the cheapest multi-stage decimation chain
   * (CIC + optional halfband + polyphase resampler) for the given rate.
   *
   * @param norm_freq  LO frequency in cycles/sample at the input rate.
   *                   Set to -f_carrier to shift a carrier at f_carrier
   *                   to DC.  Any real value is accepted.
   * @param rate       Output rate / input rate.  Must be > 0.  Values
   *                   >= 1 are up-sampling; typical use is decimation
   *                   (0 < rate < 1).
   * @return Non-NULL on success, NULL on OOM or invalid args.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> ddc.norm_freq
   * -0.1
   * >>> ddc.rate
   * 0.25
   * @endcode
   */
ddc_state_t *ddc_create(double norm_freq, double rate);

  /**
   * @brief Create a DDC whose cascade's terminal stage IS a matched filter.
   *
   * The matched *flavor* of the same object — same state, same methods, one
   * different constructor (Python: `MatchedDDC`).  The pulse is a straight
   * passthrough to the cascade, so everything RateConverter_create_matched()
   * documents holds here unchanged: the terminal fractional stage always
   * exists, the bank is sized by the POST-decimation rate, and the CIC droop
   * folds into the bank rather than costing a stage.  What this layer adds is
   * the mix in front of it, and with it the second control port —
   * ddc_execute_ctrl() steers the matched filter's polyphase arm (timing) and
   * the LO's phase accumulator (carrier) together.
   *
   * Droop compensation is not a parameter because it is unconditional here:
   * the fold is worth 28 dB of EVM for six taps per arm and no extra pass
   * over the data, so no operating point wants it off.  (The plain
   * ddc_create() path is unchanged and uncompensated.)
   *
   * @param norm_freq  LO frequency in cycles/sample at the input rate, as
   *                   ddc_create().
   * @param rate       Output-to-input sample rate ratio.  Rate-agnostic: a
   *                   caller wanting `m` outputs per symbol asks for
   *                   `rate = m/sps`; the cascade never learns about symbols.
   * @param pulse      RC_PULSE_RRC / RC_PULSE_IANDD.  RC_PULSE_NONE is
   *                   invalid here — use ddc_create() for a plain
   *                   down-conversion.
   * @param beta       RRC roll-off in `[0, 1]` (ignored for the rectangle).
   * @param span       One-sided RRC span in symbols (ignored for the
   *                   rectangle, whose support is exactly one symbol).
   * @param pulse_sps  The pulse's period in **output** samples (2 = two
   *                   samples per symbol out).
   * @param num_phases Terminal-stage arms; a power of two.  Sets the timing
   *                   resolution to `1/num_phases` of an output period.
   * @return Non-NULL on success, NULL on a bad parameter or OOM.
   *
   * @code
   * >>> from doppler.ddc import MatchedDDC
   * >>> rx = MatchedDDC(norm_freq=-0.1, rate=2 / 16, pulse="rrc")
   * >>> rx.rate
   * 0.125
   * @endcode
   */
  ddc_state_t *ddc_create_matched (double norm_freq, double rate, int pulse,
                                   double beta, size_t span, double pulse_sps,
                                   size_t num_phases);

  /**
   * @brief Free all resources held by a DDC instance.
   * Releases the RateConverter and LO substructures, then the struct
   * itself.  Passing NULL is a no-op.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=0.0, rate=0.25)
   * >>> ddc.destroy()   # releases C memory immediately
   * @endcode
   */
void ddc_destroy(ddc_state_t *state);

  /**
   * @brief Zero LO phase and resampler history.
   * After reset, the next execute call produces the same output as the
   * first execute after create — useful for reproducible block-by-block
   * processing or looped test fixtures.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> import numpy as np
   * >>> ddc = DDC(norm_freq=0.0, rate=0.25)
   * >>> x = np.ones(64, dtype=np.complex64)
   * >>> y1 = ddc.execute(x)
   * >>> ddc.reset()
   * >>> y2 = ddc.execute(x)
   * >>> bool(np.array_equal(y1, y2))
   * True
   * @endcode
   */
void ddc_reset(ddc_state_t *state);

  /**
   * @brief Return the current LO normalised frequency (cycles/sample).
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> ddc.norm_freq
   * -0.1
   * @endcode
   */
double ddc_get_norm_freq(const ddc_state_t *state);

  /**
   * @brief Retune the LO without resetting phase or resampler history.
   * Updates the NCO phase increment atomically so the carrier shift
   * changes seamlessly across block boundaries.  The resampler history
   * and LO phase accumulator are left intact, avoiding the transient
   * that a full reset would cause.
   *
   * @param state  Must be non-NULL.
   * @param val    New normalised frequency (cycles/sample at input rate).
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> ddc.norm_freq = -0.2
   * >>> ddc.norm_freq
   * -0.2
   * @endcode
   */
void ddc_set_norm_freq(ddc_state_t *state, double val);

  /**
   * @brief Return the configured output/input rate ratio (read-only).
   * The rate is fixed at create time; change it by destroying and
   * recreating the DDC with the new value.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=0.0, rate=0.25)
   * >>> ddc.rate
   * 0.25
   * @endcode
   */
double ddc_get_rate(const ddc_state_t *state);

  /**
   * @brief Mix and resample a block of CF32 samples.
   * Multiplies each input sample by the current LO phasor (advancing the
   * NCO phase per sample), then feeds the mixed block into the
   * RateConverter.  The resampler maintains history across calls, so
   * arbitrary block sizes produce contiguous output with no edge
   * artefacts.  Output length ≈ x_len * rate (varies by ±1 due to
   * polyphase indexing).
   *
   * @param state    Must be non-NULL.
   * @param x        CF32 input block; accepted as float32 (auto-cast).
   * @param x_len    Number of input samples (C-only, hidden from Python).
   * @param out      CF32 output buffer (C-only, hidden from Python).
   * @param max_out  Output buffer capacity (C-only, hidden from Python).
   * @return Number of output samples written (C-only).
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> import numpy as np
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> t = np.arange(4096)
   * >>> x = np.exp(1j * 2 * np.pi * 0.1 * t).astype(np.complex64)
   * >>> y = ddc.execute(x)
   * >>> y.shape
   * (1024,)
   * >>> y.dtype
   * dtype('complex64')
   * >>> round(float(abs(y[500])), 2)   # shifted to DC; amplitude ≈ 1
   * 1.0
   * @endcode
   */
size_t ddc_execute(ddc_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);

  /**
   * @brief Mix and resample a block, steering both control ports.
   *
   * The control-port form of ddc_execute(): the LO advances by
   * `phase_inc + freq_ctrl` on every sample of this block, and the cascade's
   * terminal stage runs at `stage_rate + rate_ctrl`. Neither deviation is
   * persisted — the centre norm_freq and rate are untouched — so a tracking
   * loop passes its full filter output on every call and the DDC holds no loop
   * state of its own.
   *
   * Feeding a stream through ddc_execute_ctrl_push() one sample at a time
   * reproduces this call bit-for-bit when both controls are held constant, so
   * the cheap block form stays correct for open-loop use (a fixed Doppler
   * offset, a rate trim) and the push form is what a closed loop uses.
   *
   * @param state     Must be non-NULL.
   * @param x         CF32 input block.
   * @param x_len     Number of input samples.
   * @param rate_ctrl Rate deviation added to the terminal Resampler stage's
   *                  rate. Referenced to the terminal (post-decimation) rate,
   *                  not the overall rate; ignored by a plan whose last stage
   *                  is an integer HB/CIC with nothing to steer.
   * @param freq_ctrl Frequency deviation added to the LO, in cycles/sample at
   *                  the INPUT rate (any sign).
   * @param out       CF32 output buffer.
   * @param max_out   Capacity of @p out in samples.
   * @return Number of output samples written.
   */
  size_t ddc_execute_ctrl (ddc_state_t *state, const float complex *x,
                           size_t x_len, double rate_ctrl, double freq_ctrl,
                           float complex *out, size_t max_out);

  /**
   * @brief Push ONE input sample; emit whatever outputs it completes.
   *
   * The per-input streaming form of ddc_execute_ctrl(), and the only form a
   * closed loop can use: a block call has to know its whole control history up
   * front, whereas a carrier or timing loop computes each correction *from*
   * the outputs already emitted. Both loops close once per symbol, so both
   * ports need this form.
   *
   * The mix costs one LO step per input; the cascade then emits 0 outputs (the
   * common decimating case, between strobes), 1, or several.
   *
   * @param state     Must be non-NULL.
   * @param x         One CF32 input sample.
   * @param rate_ctrl Rate deviation for this input (terminal-stage rate).
   * @param freq_ctrl Frequency deviation for this input, cycles/sample at the
   *                  input rate.
   * @param out       Output buffer for any emitted samples.
   * @param max_out   Capacity of @p out (emission stops at this bound).
   * @return Number of outputs written (0, 1, or more).
   */
  size_t ddc_execute_ctrl_push (ddc_state_t *state, float complex x,
                                double rate_ctrl, double freq_ctrl,
                                float complex *out, size_t max_out);

  /**
   * @brief ddc_execute_ctrl_push() that also hands back the post-LO sample.
   *
   * Identical in every respect, plus a tap on the signal *between* the mix and
   * the cascade — de-rotated, but not yet decimated or matched-filtered.
   *
   * The tap exists because a carrier discriminator's unambiguous frequency
   * range is set by the rate it UPDATES at: an M-th-power detector running at
   * rate `F` can only see `|df| < F/(2M)`. Take it from the terminal stage's
   * on-time strobe and that rate is the symbol rate, which is the cleanest
   * possible input and the narrowest possible pull-in. Take it here and the
   * rate is the full input rate — `sps` times wider — at the cost of no
   * matched filtering, so a caller wanting SNR back must run its own arm
   * filter over this stream. That trade is the caller's to make, which is why
   * this is a tap rather than a mode.
   *
   * @param state     Must be non-NULL.
   * @param x         One CF32 input sample.
   * @param rate_ctrl Rate deviation for this input (terminal-stage rate).
   * @param freq_ctrl Frequency deviation for this input, cycles/sample at the
   *                  input rate.
   * @param out       Output buffer for any emitted outputs.
   * @param max_out   Capacity of @p out (emission stops at this bound).
   * @param lo_out    Receives the post-LO, pre-cascade sample when @p n_lo
   *                  comes back 1. May be NULL.
   * @param n_lo      Receives 1 (this front end mixes every input, so always
   *                  1 here; the real-input twin gates on its halfband and can
   *                  return 0). May be NULL.
   * @return Number of terminal outputs written (0, 1, or more).
   */
  size_t ddc_execute_ctrl_push_tap (ddc_state_t *state, float complex x,
                                    double rate_ctrl, double freq_ctrl,
                                    float complex *out, size_t max_out,
                                    float complex *lo_out, int *n_lo);

  /**
   * @brief Is this object's rectangular matched filter degenerately narrow?
   *
   * True only for the matched flavor built with `pulse = RC_PULSE_IANDD` and
   * fewer than four output samples per symbol: the rectangle is exactly one
   * symbol wide, so its matched filter is a 2-3 tap sum there. It works, it
   * just barely opens the eye — measured on the timing loop this feeds, a
   * lock statistic of -0.34 at two samples per symbol against +0.95 at four.
   * The RRC spans many symbols and is never affected. Construction also
   * raises a UserWarning, so this is the pull half of the same diagnostic.
   */
  bool ddc_get_narrow_pulse (const ddc_state_t *state);

  /**
   * @brief Has the cascade's CIC clipped its input since the last reset?
   *
   * Forwarded from RateConverter_get_clipped(): a CIC bounds its input to
   * `|Re|, |Im| <= 1.0` and clips silently past it — the output stays finite
   * and plausible, merely distorted, at a cost of ~25 dB of EVM that no
   * downstream metric attributes to the front end. Sticky until ddc_reset();
   * always false for a plan with no CIC stage, which is the honest answer since
   * those plans are scale-free.
   */
bool ddc_get_clipped(const ddc_state_t *state);

  /**
   * @brief Return the maximum output samples for one execute call.
   *
   * Returns 0, signalling the Python extension to fall back to
   * allocating n_in samples — always sufficient for a decimating DDC.
   */
size_t ddc_execute_max_out(ddc_state_t *state);

  /* ── Serializable state — complex DDC (LO + RateConverter) ─────────────────
   * Standard bytes interface (see dp_state.h):
   * `[dp_state_hdr_t][ddc_extra_t][lo][rc]`.  Like ddcr without the real-input
   * halfband front end; `rate` is the layout key. */

  typedef struct
  {
    double rate; /**< Total rate; must equal the engine's (layout key). */
  } ddc_extra_t;

#define DDC_STATE_MAGIC DP_FOURCC ('D', 'D', 'C', '_')
#define DDC_STATE_VERSION 1u

  /** @brief Byte size of @p state's blob (envelope + extra + lo + rc). */
  size_t ddc_state_bytes (const ddc_state_t *state);
  /** @brief Serialize @p state's LO + RateConverter state into @p blob. */
  void ddc_get_state (const ddc_state_t *state, void *blob);
  /** @brief Restore LO + RateConverter state from @p blob.
   *  @return DP_OK, or DP_ERR_INVALID if the envelope/rate rejects. */
  int ddc_set_state (ddc_state_t *state, const void *blob);
  /** @brief Pure run: `(state_in, input) -> (state_out, output)`; either blob
   *  may be NULL (NULL in = current; NULL out = discard). */
  size_t ddc_run (ddc_state_t *state, const void *state_in, void *state_out,
                  const float complex *in, size_t n_in, float complex *out,
                  size_t max_out);

size_t ddc_execute_ctrl_max_out(ddc_state_t *state);
size_t ddc_execute_ctrl_push_max_out(ddc_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DDC_CORE_H */
