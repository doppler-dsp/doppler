/**
 * @file ddcr_core.h
 * @brief Real-input Digital Down-Converter — halfband R2C + LO + cascade.
 *
 * The real-input twin of ddc/ddc_core.h's Ddc: identical from the LO onwards,
 * behind a real-to-complex front end.
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
 * The halfband R2C step has an fs/4 frequency shift baked in at zero extra
 * multiplications — the +/-1/0 coefficients multiply for free — and everything
 * after it (the fine LO and the whole cascade) runs at fs_in/2.
 *
 * What that is worth, measured rather than assumed: for the FRONT END alone,
 * against Ddc fed the same stream promoted to complex, essentially nothing —
 * 1.04x to 1.40x end to end at total rates 0.25/0.125/0.0625, and 0.74x to
 * 1.13x once the real->complex promote is charged to Ddc, with the ratio
 * wandering by block size the way a memory-bound measurement does. The free
 * coefficients are real; multiplies are simply not what this path pays for.
 *
 * Where the half rate DOES pay is a whole receiver, because it halves the
 * sample rate ahead of the polyphase matched filter: MpskReceiverR against
 * MpskReceiver on the same stream measures 1.13x at sps=20/m_out=8, 1.50x at
 * sps=32/m_out=8 and 1.69x at sps=64/m_out=8. It rises toward 2x with sps
 * (more of the total cost is then pre-MF) but cannot reach it, since both
 * paths fire the same m_out terminal dot products per symbol and those
 * dominate at low sps. Choose DdcR because your input IS real, not for a
 * factor of two.
 *
 * Like Ddc it has a matched *flavor* (ddcr_create_matched, Python
 * `MatchedDdcr`) that puts the pulse on the cascade's terminal stage, and the
 * same two control ports — see ddc/ddc_core.h's file header for what the
 * ports are and why they are duals.
 *
 * @code
 * // Tune a real tone at +0.1*fs to DC, decimate by 4
 * // norm_freq at intermediate rate: -(2 * 0.1 + 0.5) = -0.7
 * ddcr_state_t *ddcr = ddcr_create(-0.7, 0.25);
 * float _Complex out[4096];
 * size_t m = ddcr_execute(ddcr, real_in, 1024, out, 4096);
 * ddcr_destroy(ddcr);
 * @endcode
 */
#ifndef DDCR_CORE_H
#define DDCR_CORE_H

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "lo/lo_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "hbdecim/hbdecim_r2c_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"
#include "agc/agc_core.h"
#include "dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief DdcR state — the real-to-complex front end, an LO and a cascade.
   *
   * Do not initialise directly; use ddcr_create() or ddcr_create_matched().
   */
  typedef struct ddcr_state
  {
    hbdecim_r2c_state_t   *r2c;  /**< 2:1 real->complex, fs/4 shift baked in */
    lo_state_t            *lo;   /**< fine tune, at the intermediate rate    */
    RateConverter_state_t *rc;   /**< the cascade, running at 2*rate         */
    double                 rate; /**< total fs_out / fs_in                   */
    /** As ddc_state_t::narrow_pulse — a rectangular pulse too narrow to be
     *  worth much, surfaced by the binding as a construction UserWarning. */
    bool narrow_pulse;
  } ddcr_state_t;

  /**
   * @brief Create a real-input Digital Down-Converter (Architecture D2).
   * The signal chain is: halfband R2C (2:1, bakes in +fs/4 shift) ->
   * fine LO mix at the intermediate rate (fs_in/2) -> RateConverter ->
   * CF32 output.  The halfband stage uses +-1/0 coefficients (no
   * multiplications) and puts the fine LO and the cascade at fs_in/2.  That
   * is worth ~1.1-1.7x in a whole receiver (it halves the rate ahead of the
   * polyphase matched filter, so the gain grows with samples/symbol) and
   * close to nothing for the front end alone -- see the file header for the
   * measurements.  Use it because the input IS real.
   *
   * @param norm_freq  Fine NCO frequency at the intermediate rate
   *                   (fs_in/2, cycles/sample).  To tune a real tone at
   *                   normalised input frequency f_c to DC, set
   *                   norm_freq = -(2*f_c + 0.5).
   * @param rate       Total output/input rate.  Must be in (0, 0.5)
   *                   because the halfband pre-decimates by 2.
   * @return Non-NULL on success, NULL on OOM or invalid args.
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
  ddcr_state_t *ddcr_create (double norm_freq, double rate);

  /**
   * @brief Create a real-input DDC whose terminal stage IS a matched filter.
   *
   * The matched flavor of DdcR (Python: `MatchedDdcr`), and identical to
   * ddc_create_matched() from the LO onwards — the halfband R2C front end is
   * a fixed 2:1 integer stage, so the pulse still lands on the cascade's
   * terminal stage and both control ports mean exactly what they mean there.
   *
   * Note the rate arithmetic the halfband imposes: the cascade behind it runs
   * at `2*rate`, and this function does that on the caller's behalf, so a
   * caller wanting `m` outputs per symbol still passes the TOTAL
   * `rate = m/sps`.  `pulse_sps` is in **output** samples, so the front end
   * does not affect it.
   *
   * @param norm_freq  Fine NCO frequency at the INTERMEDIATE rate (fs_in/2) —
   *                   the same reference ddcr_create() uses.
   * @param rate       Total output/input rate; must be in (0, 0.5).
   * @param pulse      RC_PULSE_RRC / RC_PULSE_IANDD (RC_PULSE_NONE is invalid
   *                   here — use ddcr_create()).
   * @param beta       RRC roll-off in `[0, 1]` (ignored for the rectangle).
   * @param span       One-sided RRC span in symbols (ignored for the
   *                   rectangle).
   * @param pulse_sps  The pulse's period in **output** samples.
   * @param num_phases Terminal-stage arms; a power of two.
   * @return Non-NULL on success, NULL on a bad parameter or OOM.
   *
   * @code
   * >>> from doppler.ddc import MatchedDdcr
   * >>> rx = MatchedDdcr(norm_freq=-0.6875, rate=2 / 16, pulse="rrc")
   * >>> rx.rate
   * 0.125
   * @endcode
   */
  ddcr_state_t *ddcr_create_matched (double norm_freq, double rate, int pulse,
                                     double beta, size_t span,
                                     double pulse_sps, size_t num_phases);

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
   * >>> round(float(abs(y[500])), 2)   # analytic signal of a unit cosine
   * 1.0
   * @endcode
   */
  size_t ddcr_execute (ddcr_state_t *s, const float *in, size_t n_in,
                       float _Complex *out, size_t max_out);

  /** @brief Upper bound on one execute call's output, or 0 to let the caller
   *  size it from the input block (a decimator never exceeds its input). */
  size_t ddcr_execute_max_out (ddcr_state_t *s);
  /** @brief As ddcr_execute_max_out(), for the block control-port form. */
  size_t ddcr_execute_ctrl_max_out (ddcr_state_t *s);
  /** @brief Bound for ONE pushed input: `ceil(rate) + 1` output periods.
   *  Non-zero because the push form has no input block to size from. */
  size_t ddcr_execute_ctrl_push_max_out (ddcr_state_t *s);

  /**
   * @brief Process a real block, steering both control ports.
   *
   * The control-port form of ddcr_execute(); see ddc_execute_ctrl() for the
   * semantics, which are identical except for where the LO lives.
   *
   * @param s         Must be non-NULL.
   * @param x         Real float32 input block.
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
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> import numpy as np
   * >>> ddcr = Ddcr(norm_freq=-0.5, rate=0.25)  # LO 0.2 short of tune
   * >>> t = np.arange(4096)
   * >>> x = np.cos(2 * np.pi * 0.1 * t).astype(np.float32)
   * >>> y = ddcr.execute_ctrl(x, 0.0, -0.2)     # ctrl completes the tune
   * >>> y.shape
   * (1024,)
   * >>> round(float(abs(y[100:].mean())), 2)    # real tone -> DC, amp 1.0
   * 1.0
   *
   * @endcode
   */
  size_t ddcr_execute_ctrl (ddcr_state_t *s, const float *x, size_t n_in,
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
   *
   * @code
   * >>> from doppler.ddc import Ddcr
   * >>> import numpy as np
   * >>> ddcr = Ddcr(norm_freq=-0.7, rate=0.25)
   * >>> x = np.cos(2 * np.pi * 0.1 * np.arange(128)).astype(np.float32)
   * >>> outs = [ddcr.execute_ctrl_push(float(s), 0.0, 0.0) for s in x]
   * >>> int(sum(len(o) for o in outs))  # 128 real inputs, rate 1/4 -> 32
   * 32
   * >>> [len(o) for o in outs[:4]]      # halfband: 0 until a strobe
   * [0, 0, 0, 1]
   *
   * @endcode
   */
  size_t ddcr_execute_ctrl_push (ddcr_state_t *s, float x, double rate_ctrl,
                                 double freq_ctrl, float _Complex *out,
                                 size_t max_out);

  /**
   * @brief ddcr_execute_ctrl_push() that also hands back the post-LO sample.
   *
   * The real-input twin of ddc_execute_ctrl_push_tap(); see that function for
   * why the tap exists (a carrier discriminator's unambiguous range is set by
   * the rate it updates at, so a caller may want the widest, least-filtered
   * stream rather than the cleanest one).
   *
   * The one difference is that this front end does NOT mix every input: the
   * 2:1 halfband consumes two real inputs per intermediate sample, so @p n_lo
   * comes back 0 on every other push and @p lo_out is untouched. The tapped
   * stream therefore runs at `fs_in/2`, the LO's own rate — half as fast as
   * the complex twin's for the same nominal `sps`, which halves this tap's
   * frequency range in input-referred terms exactly as it halves everything
   * else the LO sees.
   *
   * @param s         Must be non-NULL.
   * @param x         One real float32 input sample.
   * @param rate_ctrl Rate deviation for this input (terminal-stage rate).
   * @param freq_ctrl Frequency deviation, cycles/sample at fs_in/2.
   * @param out       Output buffer for any emitted outputs.
   * @param max_out   Capacity of @p out.
   * @param lo_out    Receives the post-LO, pre-cascade sample when @p n_lo
   *                  comes back 1. May be NULL.
   * @param n_lo      Receives 1 when the halfband fired for this input and the
   *                  LO stepped, 0 otherwise. May be NULL.
   * @return Number of terminal outputs written (0, 1, or more).
   */
  size_t ddcr_execute_ctrl_push_tap (ddcr_state_t *s, float x,
                                     double rate_ctrl, double freq_ctrl,
                                     float _Complex *out, size_t max_out,
                                     float _Complex *lo_out, int *n_lo);

  /**
   * @brief ddcr_execute_ctrl_push_tap(), plus the MFR-INPUT tap.
   *
   * The real-input twin of ddc_execute_ctrl_push_tap2(). @p pre_out receives
   * the cascade's output after every integer stage and after the AGC but
   * ahead of the terminal matched filter — the node an NDA carrier
   * discriminator can read with no symbol timing. Its rate is
   * ddcr_get_bank_sps() samples per symbol.
   *
   * The halfband gates the whole call: on the inputs it swallows there is no
   * LO step and no cascade push, so @p n_lo and @p n_pre both come back 0.
   *
   * @param s         Must be non-NULL.
   * @param x          One real input sample.
   * @param rate_ctrl  Rate deviation for this input (terminal-stage rate).
   * @param freq_ctrl  Frequency deviation, cycles/sample at the LO's own
   *                   (halved) intermediate rate.
   * @param out        Output buffer for any emitted outputs.
   * @param max_out    Capacity of @p out.
   * @param lo_out     Receives the post-LO, pre-cascade sample when @p n_lo
   *                   comes back 1. May be NULL.
   * @param n_lo       Receives 1 when the halfband emitted and the LO
   *                   stepped, else 0. May be NULL.
   * @param pre_out    Receives the MFR-input sample; may be NULL.
   * @param n_pre      Receives 1 if @p pre_out was written, else 0; may be
   *                   NULL.
   * @return Number of terminal outputs written.
   */
  size_t ddcr_execute_ctrl_push_tap2 (ddcr_state_t *s, float x,
                                      double rate_ctrl, double freq_ctrl,
                                      float _Complex *out, size_t max_out,
                                      float _Complex *lo_out, int *n_lo,
                                      float _Complex *pre_out, int *n_pre);

  /** @brief Samples per symbol of the MFR-input tap; a planner outcome.
   *  Identical to the complex twin's at every rate ratio — `bank_sps` is
   *  symbol-relative, so the halfband's 2:1 is absorbed by the plan. */
  double ddcr_get_bank_sps (const ddcr_state_t *s);

  /**
   * @brief Is this object's rectangular matched filter degenerately narrow?
   *
   * The real chain's copy of ddc_get_narrow_pulse(): true only for the
   * matched flavor with `pulse = RC_PULSE_IANDD` and fewer than four output
   * samples per symbol, where the one-symbol-wide rectangle's matched filter
   * is a 2-3 tap sum. Construction also raises a UserWarning.
   */
  bool ddcr_get_narrow_pulse (const ddcr_state_t *s);

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
   * @brief Attach (or detach) a telemetry context on the cascade's AGC.
   *
   * The twin of ddc_set_telemetry(), forwarded to the same
   * RateConverter_set_telemetry() over the same cascade: the R2C front end and
   * the fixed stages have no loop to report, so the one instrumented child is
   * the pre-terminal AGC ("<prefix>.gain_db" and "<prefix>.level_db"). DP_OK
   * with no probes when the cascade has no AGC enabled.
   *
   * @param s      Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "rx.agc".
   * @param decim  Emit every decim-th gain update; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take the
   *         AGC's probes (the attach fails whole).
   */
  int ddcr_set_telemetry (ddcr_state_t *s, dp_tlm_t *tlm, const char *prefix,
                          uint32_t decim);


#ifdef __cplusplus
}
#endif

#endif /* DDCR_CORE_H */
