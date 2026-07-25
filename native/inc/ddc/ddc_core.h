/**
 * @file ddc_core.h
 * @brief Digital Down-Converter — composes LO + RateConverter cascade.
 *
 * Two types:
 *
 *   Ddc   — complex (CF32) input.  Chain: LO mix → RateConverter.
 *   DdcR  — real (float32) input.  Chain: halfband R2C → LO mix → RateConverter.
 *
 * Both are streaming (variable block size per execute call).
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
 * ### DdcR signal chain
 *
 * ```
 * float in (fs_in)  →  halfband R2C (2:1, embedded fs/4 shift)
 *                   →  LO mix at intermediate rate (fs_in/2)
 *                   →  RateConverter  →  CF32 out (fs_out)
 * ```
 *
 * norm_freq:  Fine NCO frequency at the INTERMEDIATE rate (fs_in/2).
 *             To tune a real tone at f_carrier (input normalised) to DC:
 *             set norm_freq = -(2*f_carrier + 0.5).
 *             Total output rate: fs_out = rate * fs_in  (rate < 0.5).
 *
 * DdcR is approximately 2x cheaper than Ddc at equivalent total decimation
 * because the halfband R2C step has an fs/4 frequency shift baked in at
 * zero extra multiplications — the +/-1/0 coefficients multiply for free.
 *
 * ### Pulse and the two control ports
 *
 * Both constructors take a *pulse* (::RC_PULSE_NONE for none),
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
 * ddc_state_t *ddc = ddc_create(-0.1, 0.25, RC_PULSE_NONE, 0, 0, 0, 0);
 * float _Complex out[4096];
 * size_t n = ddc_execute(ddc, in, 1024, out, 4096);
 * ddc_destroy(ddc);
 *
 * // Real DDC: same carrier and decimation from a real ADC stream
 * // norm_freq at intermediate rate: -(2 * 0.1 + 0.5) = -0.7
 * ddcr_state_t *ddcr = ddcr_create(-0.7, 0.25, RC_PULSE_NONE, 0, 0, 0, 0);
 * size_t m = ddcr_execute(ddcr, real_in, 1024, out, 4096);
 * ddcr_destroy(ddcr);
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

  /* ================================================================== */
  /* Ddc — complex-input DDC                                            */
  /* ================================================================== */

  typedef struct ddc_state ddc_state_t;

  /**
   * @brief Create a complex-input Digital Down-Converter.
   *
   * Allocates the LO and the RateConverter cascade, which selects the cheapest
   * multi-stage decimation chain (CIC + optional halfband + polyphase
   * resampler) for the requested rate.
   *
   * @p pulse is the one knob that changes what the cascade IS.  With
   * ::RC_PULSE_NONE the terminal stage carries the default Kaiser anti-alias
   * bank — a plain down-converter, and the remaining arguments are unused.
   * With a pulse it carries a matched-filter bank instead, so the chain mixes,
   * decimates and matched-filters in the same dot products, and that stage's
   * polyphase arm becomes the fractional timing delay ddc_execute_ctrl()
   * steers.  Everything RateConverter_create_matched() documents holds here
   * unchanged: the terminal fractional stage always exists, the bank is sized
   * by the POST-decimation rate, and the CIC droop folds into the bank rather
   * than costing a stage.
   *
   * Droop compensation is not a parameter because it is unconditional on the
   * matched path: the fold is worth 28 dB of EVM for six taps per arm and no
   * extra pass over the data, so no operating point wants it off.  The plain
   * path is uncompensated, exactly as before.
   *
   * @param norm_freq  LO frequency in cycles/sample at the input rate.
   *                   Set to -f_carrier to shift a carrier at f_carrier
   *                   to DC.  Any real value is accepted.
   * @param rate       Output rate / input rate.  Must be > 0.  Values
   *                   >= 1 are up-sampling; typical use is decimation
   *                   (0 < rate < 1).  Rate-agnostic: a caller wanting `m`
   *                   outputs per symbol asks for `rate = m/sps`; the cascade
   *                   never learns about symbols.
   * @param pulse      RC_PULSE_RRC / RC_PULSE_IANDD for a matched terminal
   *                   stage, or RC_PULSE_NONE for a plain down-conversion.
   * @param beta       RRC roll-off in `[0, 1]` (ignored for the rectangle and
   *                   for RC_PULSE_NONE).
   * @param span       One-sided RRC span in symbols (ignored for the
   *                   rectangle, whose support is exactly one symbol).
   * @param pulse_sps  The pulse's period in **output** samples (2 = two
   *                   samples per symbol out).
   * @param num_phases Terminal-stage arms; a power of two.  Sets the timing
   *                   resolution to `1/num_phases` of an output period.
   * @return Non-NULL on success, NULL on a bad parameter or OOM.
   *
   * @code
   * >>> from doppler.ddc import DDC
   * >>> ddc = DDC(norm_freq=-0.1, rate=0.25)
   * >>> ddc.norm_freq
   * -0.1
   * >>> ddc.rate
   * 0.25
   * >>> DDC(norm_freq=-0.1, rate=2 / 16, pulse="rrc").rate == 0.125
   * True
   * @endcode
   */
ddc_state_t *ddc_create(double norm_freq, double rate, int pulse, double beta, size_t span, double pulse_sps, size_t num_phases);

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

  /* ================================================================== */
  /* DdcR — real-input DDC (Architecture D2)                           */
  /* ================================================================== */

  typedef struct ddcr_state ddcr_state_t;

  /**
   * @brief Create a real-input Digital Down-Converter (Architecture D2).
   *
   * The signal chain is: halfband R2C (2:1, bakes in +fs/4 shift) -> fine LO
   * mix at the intermediate rate (fs_in/2) -> RateConverter -> CF32 output.
   * The halfband stage uses +-1/0 coefficients (no multiplications), making
   * DDCR roughly 2x cheaper than DDC at the same total decimation ratio.
   *
   * @p pulse means exactly what it means in ddc_create() — the halfband front
   * end is a fixed integer stage, so the pulse still lands on the cascade's
   * terminal stage and both control ports behave identically.  Note the rate
   * arithmetic the halfband imposes: the cascade behind it runs at `2*rate`,
   * and this function does that on the caller's behalf, so a caller wanting
   * `m` outputs per symbol still passes the TOTAL `rate = m/sps`.
   * `pulse_sps` is in **output** samples, so the front end does not affect it.
   *
   * @param norm_freq  Fine NCO frequency at the intermediate rate
   *                   (fs_in/2, cycles/sample).  To tune a real tone at
   *                   normalised input frequency f_c to DC, set
   *                   norm_freq = -(2*f_c + 0.5).
   * @param rate       Total output/input rate.  Must be in (0, 0.5) because
   *                   the halfband pre-decimates by 2.
   * @param pulse      RC_PULSE_RRC / RC_PULSE_IANDD, or RC_PULSE_NONE for a
   *                   plain down-conversion (remaining arguments unused).
   * @param beta       RRC roll-off in `[0, 1]` (ignored for the rectangle).
   * @param span       One-sided RRC span in symbols (ignored for the
   *                   rectangle).
   * @param pulse_sps  The pulse's period in **output** samples.
   * @param num_phases Terminal-stage arms; a power of two.
   * @return Non-NULL on success, NULL on a bad parameter or OOM.
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> ddcr.norm_freq
   * -0.7
   * >>> ddcr.rate
   * 0.25
   * @endcode
   */
  ddcr_state_t *ddcr_create (double norm_freq, double rate, int pulse,
                             double beta, size_t span, double pulse_sps,
                             size_t num_phases);

  /**
   * @brief Free all resources held by a DDCR instance.
   * Releases the halfband, RateConverter, and LO substructures, then
   * the struct itself.  Passing NULL is a no-op.
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
   * >>> ddcr.close()   # releases C memory immediately
   * @endcode
   */
  void ddcr_destroy (ddcr_state_t *s);

  /**
   * @brief Zero halfband filter history, LO phase, and resampler history.
   * After reset, the next execute call reproduces the output of the
   * first call after create, enabling repeatable block-by-block tests.
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> import numpy as np
   * >>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
   * >>> x = np.ones(64, dtype=np.float32)
   * >>> out = np.empty(64, dtype=np.complex64)
   * >>> y1 = ddcr.execute(x, out).copy()
   * >>> ddcr.reset()
   * >>> y2 = ddcr.execute(x, out)
   * >>> bool(np.array_equal(y1, y2))
   * True
   * @endcode
   */
  void ddcr_reset (ddcr_state_t *s);

  /* ── Serializable state — the elastic / pure-transducer face ───────────────
   *
   * Composes the leaf serializers of the whole chain (hbdecim_r2c -> LO ->
   * RateConverter) into one flat POD, so a fresh DDCR built from the same
   * (norm_freq, rate) descriptor resumes a stream bit-exactly on any
   * thread/process/pod.  Standard bytes interface (see dp_state.h): the blob is
   * `[dp_state_hdr_t][ddcr_extra_t][r2c][lo][rc]`, each child a self-contained
   * sub-blob with its own envelope.  `rate` is the layout key. */

  typedef struct
  {
    double rate; /**< Total rate; must equal the engine's (layout key). */
  } ddcr_extra_t;

#define DDCR_STATE_MAGIC DP_FOURCC ('D', 'D', 'C', 'R')
#define DDCR_STATE_VERSION 1u

  /** @brief Byte size of @p s's state blob (envelope + extra + chain). */
  size_t ddcr_state_bytes (const ddcr_state_t *s);
  /** @brief Serialize @p s's full-chain state into @p blob. */
  void ddcr_get_state (const ddcr_state_t *s, void *blob);
  /**
   * @brief Restore full-chain state from @p blob into @p s.
   * @return DP_OK, or DP_ERR_INVALID if the envelope/rate disagree with @p s
   *         (rebuild the engine from the matching descriptor first).
   */
  int ddcr_set_state (ddcr_state_t *s, const void *blob);

  /**
   * @brief Pure run: inject @p state_in, process @p in, export @p state_out —
   *        `(state_in, input) -> (state_out, output)` over an engine treated as
   *        immutable config.  Either state may be NULL (NULL in = use current;
   *        NULL out = discard).  @p state_in / @p state_out may alias.
   * @return Number of CF32 output samples written.
   */
  size_t ddcr_run (ddcr_state_t *s, const void *state_in, void *state_out,
                   const float *in, size_t n_in, float _Complex *out,
                   size_t max_out);

  /**
   * @brief Return the current fine NCO normalised frequency at the
   * intermediate rate (fs_in/2, cycles/sample).
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> ddcr.norm_freq
   * -0.7
   * @endcode
   */
  double ddcr_get_norm_freq (const ddcr_state_t *s);

  /**
   * @brief Retune the fine NCO without resetting halfband or resampler
   * history.  Updates the LO phase increment only; state is preserved
   * for seamless tuning across block boundaries.
   *
   * @param s         Must be non-NULL.
   * @param norm_freq New frequency at the intermediate rate (fs_in/2).
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> ddcr.norm_freq = -0.5
   * >>> ddcr.norm_freq
   * -0.5
   * @endcode
   */
  void ddcr_set_norm_freq (ddcr_state_t *s, double norm_freq);

  /**
   * @brief Return the total configured rate (fs_out / fs_in, read-only).
   * This is the end-to-end ratio from ADC input to CF32 output.  Change
   * it by destroying and recreating the DDCR.
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> ddcr = Ddcr(norm_freq=0.0, rate=0.25)
   * >>> ddcr.rate
   * 0.25
   * @endcode
   */
  double ddcr_get_rate (const ddcr_state_t *s);

  /**
   * @brief Process a block of real float32 samples through the full
   * DDCR signal chain: halfband R2C → LO mix → RateConverter → CF32.
   * The halfband decimates by 2 and applies a built-in +fs/4 frequency
   * shift; the fine NCO then completes the tuning.  State is maintained
   * across calls for contiguous streaming.  Output length ≈ n_in * rate
   * (±1 from polyphase indexing).  A real tone at input normalised
   * frequency f_c has amplitude 0.5 in the baseband output (one-sided
   * spectrum), consistent with analytic signal theory.
   *
   * @param s        Must be non-NULL.
   * @param in       Real float32 input block.
   * @param n_in     Number of input samples (C-only, hidden from Python).
   * @param out      CF32 output buffer (C-only, hidden from Python).
   * @param max_out  Output buffer capacity (C-only, hidden from Python).
   * @return Number of output samples written (C-only).
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> import numpy as np
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> t = np.arange(4096)
   * >>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
   * >>> out = np.empty(len(x), dtype=np.complex64)
   * >>> y = ddcr.execute(x, out)
   * >>> y.shape
   * (1024,)
   * >>> y.dtype
   * dtype('complex64')
   * >>> round(float(abs(y[500])), 2)   # one-sided cosine amplitude ≈ 0.5
   * 0.5
   * @endcode
   */
  size_t ddcr_execute (ddcr_state_t *s, const float *in, size_t n_in,
                       float _Complex *out, size_t max_out);

  /**
   * @brief Process a real block, steering both control ports.
   *
   * The control-port form of ddcr_execute(); see ddc_execute_ctrl() for the
   * semantics, which are identical except for where the LO lives.
   *
   * @param s         Must be non-NULL.
   * @param in        Real float32 input block.
   * @param n_in      Number of input samples.
   * @param rate_ctrl Rate deviation added to the terminal Resampler stage's
   *                  rate (referenced to the terminal, post-decimation rate).
   * @param freq_ctrl Frequency deviation added to the fine LO, in
   *                  cycles/sample at the INTERMEDIATE rate (fs_in/2) — the
   *                  halfband has already decimated by two by the time the
   *                  mix happens, so a discriminator working in cycles per
   *                  ADC sample must be doubled before it lands here.
   * @param out       CF32 output buffer.
   * @param max_out   Capacity of @p out in samples.
   * @return Number of output samples written.
   */
  size_t ddcr_execute_ctrl (ddcr_state_t *s, const float *in, size_t n_in,
                            double rate_ctrl, double freq_ctrl,
                            float _Complex *out, size_t max_out);

  /**
   * @brief Push ONE real input sample; emit whatever outputs it completes.
   *
   * The per-input streaming form of ddcr_execute_ctrl(), for a closed loop.
   * The halfband consumes two inputs per intermediate sample, so every other
   * push does no mixing and emits nothing at all — the LO advances (and its
   * control is applied) once per *intermediate* sample, which is the rate the
   * LO runs at.
   *
   * @param s         Must be non-NULL.
   * @param x         One real float32 input sample.
   * @param rate_ctrl Rate deviation for this input (terminal-stage rate).
   * @param freq_ctrl Frequency deviation, cycles/sample at fs_in/2.
   * @param out       Output buffer for any emitted samples.
   * @param max_out   Capacity of @p out.
   * @return Number of outputs written (0, 1, or more).
   */
  size_t ddcr_execute_ctrl_push (ddcr_state_t *s, float x, double rate_ctrl,
                                 double freq_ctrl, float _Complex *out,
                                 size_t max_out);

  /**
   * @brief Has the cascade's CIC clipped its input since the last reset?
   *
   * Forwarded from RateConverter_get_clipped(); see ddc_get_clipped(). The
   * halfband R2C front end has unity passband gain and a real tone lands at
   * amplitude 0.5 in the analytic output, so a full-scale ADC stream sits
   * comfortably inside the CIC's bound — but a scaled-up input does not.
   */
  bool ddcr_get_clipped (const ddcr_state_t *s);

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
