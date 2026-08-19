/**
 * @file mpsk_receiver_core.h
 * @brief Pulse-shaped M-PSK receiver: a tuned matched front end and two loops.
 *
 * A complete inline modem for a continuous (unspread) M-PSK signal at **any**
 * input rate. It is the top of the polyphase family, and it is composition
 * rather than machinery — it owns no filter, no NCO and no interpolator of its
 * own:
 *
 * ```
 *   x ──> MatchedDDC ──────────────────────────────> y ──> loops ──> symbols
 *          LO mix · CIC/HB cascade · matched filter        │
 *            ^                            ^                │
 *            └── freq_ctrl ── carrier ────┴── rate_ctrl ───┘
 * ```
 *
 * ## One object, two front ends
 *
 * A **real** IF — the usual output of a single-ended ADC — is the same
 * receiver behind an R2C halfband, and it is a `real` flag on this state
 * rather than a second type:
 *
 * ```
 *   f32 in ──> MatchedDdcr ────────────────────────> y ──> the SAME loops
 *               halfband R2C (2:1) · LO mix · cascade · MF
 * ```
 *
 * Every loop, discriminator and demapper decision is one
 * implementation over one @ref mpsk_rx_loops_t. What the front end changes is
 * exactly three things, and each is a rate convention rather than an
 * algorithm:
 *
 * - **The LO runs at half the input rate.** The R2C halfband decimates 2:1
 *   (with the fs/4 shift baked in) *before* the mix, so the LO sees `sps/2`
 *   samples per symbol — which is why @ref mpsk_rx_loops_t takes `lo_sps`
 *   separately from `sps`. `norm_freq` stays caller-facing in cycles/sample at
 *   the **input** rate, so the real face halves it on the way in and doubles
 *   it on the way out. Ddcr's tuning law is `norm_freq = -(2*f_c + 0.5)`.
 * - **`sps` must exceed `2 * m_out`,** strictly, against `sps >= m_out` for
 *   the complex face: the cascade behind the halfband runs at twice the
 *   overall rate and Ddcr requires that below 0.5.
 * - **`init_norm_freq` means the real IF centre** rather than a baseband
 *   residual.
 *
 * The hot path is not tagged. There are two `step` entry points, each force-
 * inlined onto @ref mpsk_rx_fold, so the front end is a compile-time fact
 * inside the sample loop and `real` is read only on cold paths (destroy,
 * reset, telemetry, the frequency accessors and the state triplet).
 *
 *   - @ref ddc_state_t (the matched flavor) mixes, decimates and
 *     matched-filters in the dot products it was already doing. Its terminal
 *     polyphase stage IS the matched filter, and the arm that stage selects IS
 *     the fractional symbol-timing delay.
 *   - @ref mpsk_rx_loops_t closes a symbol-timing loop on the cascade's
 *     `rate_ctrl` port and a carrier loop on the LO's `freq_ctrl` port. The
 *     timing half is @ref ratesync_loop_t — literally RateSync's loop, not a
 *     copy of it.
 *
 * Carrier recovery follows the project rule, now structurally rather than by
 * convention: **predetection de-rotation** happens in the LO at the front of
 * the chain, and **postdetection discrimination** on the matched-filtered
 * symbols at the end of it. ONE discriminator steers the one LO: the NDA
 * M-th-power error on the on-time strobe, needing no data and no symbol
 * timing, running from the first symbol to the last.
 *
 * There is no acquisition/tracking handover to a decision-directed error.
 * There was one, opt-in, until doppler#877 measured what it bought: across
 * the ten paired cells where it engaged it moved 99% of the recovered symbols
 * and changed the symbol error rate by a mean factor of 0.9999 (t = 0.28).
 * See mpsk_rx_loops.h for why the strobe is the only sample either
 * discriminator could have read.
 *
 * ## What the cascade buys
 *
 * `sps` is a **double**, and the front end plans itself. At `sps = 8` the plan
 * is a halfband or two and a terminal stage; at `sps = 256` it is a CIC
 * followed by the same terminal stage, so the matched filter costs the same
 * bank either way (~34 taps/arm at both ends of a 64x span of input rates,
 * against the 4225 taps/arm a single-stage design would need). An
 * irrational `sps` — a free-running ADC clock against the symbol clock — is
 * no harder than an integer one, because the terminal accumulator is a double
 * and the loop only has to steer the strobe.
 *
 * The M-fold phase ambiguity is unchanged: resolve it with differential
 * demapping (`bits(..., differential=1)`) or a sync word downstream. A
 * DSSS-MPSK receiver is still `Dll(segments) -> MpskReceiver`.
 *
 * @warning **This object's outputs are not bit-identical to releases before
 * the cascade rebuild.** The matched filter became a polyphase bank instead of
 * a dense FIR and the interpolator became a bank arm instead of a Farrow, so
 * symbols move at the float level. `bn_carrier` also changed units: it is now
 * normalised to the **symbol rate**, like `bn_timing`, rather than to the
 * input sample rate — at the old default `sps = 8` the same number is now an
 * 8x wider loop. Detection performance is unchanged (the fused matched filter
 * measures on the Es/N0 bound); exact-output pins are not.
 *
 * Lifecycle: `mpsk_receiver_create -> (steps / bits / reset)* -> _destroy`.
 *
 * @code
 * // QPSK, 8 samples/symbol, I&D matched filter, NDA acquisition
 * mpsk_receiver_state_t *rx = mpsk_receiver_create (
 *     4, 8.0, 4, MPSK_RX_PULSE_IANDD, 0.35, 8,
 *     0.01, 0.707, 0.01, 0.5, 0.0, 100, 0, 1024,
 *     1, MPSK_RX_AGC_BW_RATIO);
 * float complex sym[256];
 * size_t k = mpsk_receiver_steps (rx, rx_in, rx_len, sym, 256);
 * double f = mpsk_receiver_get_norm_freq (rx);  // tracked residual carrier
 * mpsk_receiver_destroy (rx);
 * @endcode
 */
#ifndef MPSK_RECEIVER_CORE_H
#define MPSK_RECEIVER_CORE_H

#include "clib_common.h"
#include "ddc/ddc_core.h"
#include "ddcr/ddcr_core.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "mpsk_receiver/mpsk_rx_loops.h"
#include <complex.h>
#include "ratesync/ratesync_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"
#include "lo/lo_core.h"
#include "nco/nco_core.h"
#include "loop_filter/loop_filter_core.h"
#include "lockdet/lockdet_core.h"
#include "symsync/symsync_core.h"
#include "agc/agc_core.h"
#include "dp_tlm/dp_tlm_core.h"
#include "ber/ber_core.h"
#include "telemetry/telemetry_core.h"
#include "boxcar/boxcar_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief M-PSK receiver state.
   *
   * Allocate with mpsk_receiver_create() (complex input) or
   * mpsk_receiver_create_real() (real IF). Owns one matched front end (`fe`)
   * and embeds the loops by value. Treat all fields as internal (use the
   * getters); they are exposed for the inline sample loop.
   */
  typedef struct
  {
    /** The matched front end. Which arm is live is `real`, and nothing else
        reads it — the two step entry points each name their own arm. */
    union
    {
      ddc_state_t  *c; /**< matched DDC:  mix + cascade + MF.            */
      ddcr_state_t *r; /**< matched DDCR: R2C + mix + cascade + MF.      */
    } fe;
    mpsk_rx_loops_t l; /**< carrier + timing loops, demapper.           */

    /* ── config (restored by create(), never packed in a state blob) ── */
    /* The pulse geometry lives in the front end, which is the only thing
       that uses it; keeping a second copy here would be a shadow of the
       cascade's own configuration, free to drift out of step with it. */
    int    real;        /**< 0 = complex front end, 1 = real IF.         */
    double centre_freq; /**< create-time carrier offset (cycles/sample),
                             at the receiver's INPUT rate on both faces. */
  } mpsk_receiver_state_t;

  /**
   * @brief Create an M-PSK receiver.
   *
   * @param m              Constellation order M, 2/4/8 (default 4 = QPSK).
   * @param sps            Samples per symbol; any double >= @p m_out (8.0
   *                        by default, but 17.33389 is equally valid).
   * @param m_out          Terminal outputs per symbol: even, 2..8. **0 (the
   *                        default) derives it** — the largest even count in
   *                        2..8 the rate allows, via
   *                        @ref mpsk_rx_derive_m_out, which is `8` at the
   *                        default `sps = 8`; pass a value only to pin one.
   *                        Read it back with mpsk_receiver_get_m_out().
   *                        Gardner needs the half-symbol gate. The derived
   *                        answer reaches 8 for two reasons. The matched
   *                        filter: the rectangle is one symbol wide, so its
   *                        filter is an m_out-tap sum spanning it, and a
   *                        smaller m_out samples the same integral more
   *                        coarsely. Measured on QPSK at sps = 8 against
   *                        EVM_dB = -(Es/N0)_dB, at 18 dB Es/N0: 0.41 dB off
   *                        the bound at 8, 3.11 dB at 4.
   *                        And the M-th-power discriminator: `z^M`
   *                        auto-convolves the spectrum M times, spreading
   *                        energy over ~`M*Rs`, and whatever exceeds the
   *                        update rate folds back onto itself. A clean strobe
   *                        raises to a constant with nothing to fold, but
   *                        every departure from clean (ISI, timing error,
   *                        noise) is splattered M-fold and aliased — so the
   *                        nonlinearity's tolerance for a coarse matched
   *                        filter COLLAPSES as M grows. The first reason is
   *                        M-independent; the second is not. Measured
   *                        (halving m_out from 8 to 4, each M at its own
   *                        SER=1e-3 anchor): BPSK 1.7 dB, QPSK 1.6 dB, **8PSK
   *                        3.0 dB** — the last also sitting 0.87 dB from the
   *                        fully-scattered EVM floor, i.e. barely
   *                        distinguishable from noise. **So m_out = 8 is not
   *                        optional at M = 8.** **Never pair 2 with
   *                        MPSK_RX_PULSE_IANDD** — the filter degenerates to
   *                        a two-tap sum, the eye barely opens and
   *                        acquisition itself fails about half the time.
   *                        Replaces the old `n` (NDA arm dumps/symbol),
   *                        which the cascade's own outputs now serve.
   * @param pulse          Matched-filter shape (default MPSK_RX_PULSE_IANDD).
   * @param rrc_beta       RRC roll-off in `[0, 1]` (default 0.35; RRC only).
   * @param rrc_span       RRC one-sided span in symbols (default 8; RRC only).
   * @param bn_carrier     Carrier loop noise bandwidth, **normalised to the
   *                        symbol rate** (default 0.01). A carrier loop here
   *                        closes around the matched filter, so its dead time
   *                        is that filter's group delay — keep it a small
   *                        fraction of the symbol rate, as a real receiver
   *                        does.
   * @param zeta           Damping factor for both loops. **0 (the default)
   *                        derives it** as `1/sqrt(2)`
   *                        (@ref MPSK_RX_ZETA_DEFAULT) — a constant rather
   *                        than a computation, since nothing in this receiver
   *                        moves the optimal damping and both loops already
   *                        share one value. Read it back with
   *                        mpsk_receiver_get_zeta().
   * @param bn_timing      Symbol-timing loop noise bandwidth, normalised to
   *                        the symbol rate (default 0.01).
   * @param lock_thresh    Declare threshold for the carrier lock
   *                        indicator, on the carrier lock metric. **0 (the default) derives it** as
   *                        `sigma_H0 * eta(Pfa)` = `0.1132 * 4.4159` =
   *                        `0.4999` (@ref MPSK_RX_LOCK_THRESH_DEFAULT), which
   *                        is the 0.5 that used to be hand-picked — so the
   *                        derivation changed no behaviour, and is here
   *                        because a number that was picked and a number that
   *                        was derived look identical until one has to move.
   *                        Read it back with
   *                        mpsk_receiver_get_lock_thresh(). The drop
   *                        threshold sits at 0.8x for level hysteresis, and
   *                        both directions are verify-counted (8 symbols up /
   *                        32 down).
   *                        The metric is `Re((z/|z|)^M)` smoothed by an EMA,
   *                        whose noise-only sd is 0.1132 for **every** M, so
   *                        the threshold is 4.42 noise sigmas — a per-look
   *                        false-alarm probability of 5e-6. To pin your own,
   *                        divide your Pfa's z-score into 0.1132 rather than
   *                        picking by feel; see carrier_nda_core.h for the
   *                        derivation and the measured verification.
   * @param init_norm_freq Seed carrier frequency, cycles/sample at the input
   *                        rate (default 0.0). This is the centre the LO is
   *                        tuned to; the loop tracks the residual around it.
   * @param differential   bits(): differential (rotation-invariant) demap
   *                        (default 0 = coherent).
   * @param num_phases     Terminal-stage bank arms; a power of two. **0 (the
   *                        default) derives it** as 64
   *                        (@ref MPSK_RX_NUM_PHASES_DEFAULT), the measured
   *                        saturation point — against the 1024 that used to
   *                        be the default, a 16x bank for no measurable gain.
   *                        Read it back with
   *                        mpsk_receiver_get_num_phases(). Sets the timing
   *                        resolution to `1/num_phases` of an output period.
   * @param agc            Non-zero (default) puts the receiver's ONE AGC in
   *                        the front-end cascade, immediately before the
   *                        terminal matched stage. **It serves BOTH loops**
   *                        — carrier and timing both run on its output, so
   *                        it is a dynamic element inside both, which is
   *                        why @ref mpsk_rx_agc_bn sizes it against the
   *                        SLOWER of the two rather than against timing
   *                        alone. What differs is only why the level
   *                        matters to each: the timing detector normalises
   *                        by a slope computed at construction for a
   *                        unit-amplitude stream (@ref symsync_ted_slope),
   *                        so a level error is a loop-gain error there
   *                        directly; the carrier detector normalises by its
   *                        own `|z|^M` (@ref carrier_nda_disc), so it is
   *                        immune to the level itself but still sees the
   *                        AGC's transient. Pass 0 and the receiver is
   *                        un-levelled: the timing loop is under-driven by
   *                        `A^2`, which at an input amplitude of 0.25 is
   *                        16x. The reference is derived from the bank's own
   *                        pulse energy, not chosen.
   * @param bn_agc_ratio   That AGC's bandwidth as a fraction of the SLOWEST
   *                        loop it feeds, `min(bn_carrier, bn_timing)` — see
   *                        @ref mpsk_rx_agc_bn. Must be in (0, 1);
   *                        construction refuses 1 or above rather than
   *                        warning, because at 1 the AGC is exactly as fast
   *                        as a loop it feeds and past that it is faster,
   *                        and two level-correcting loops at the same speed
   *                        integrate against each other. **0 (the default)
   *                        derives it** as `MPSK_RX_AGC_BW_RATIO` = 0.05,
   *                        20x slower than the slowest loop it feeds
   *                        (@ref MPSK_RX_AGC_RATIO_DEFAULT); 0 is the one
   *                        value below 1 that is a request rather than a
   *                        rejection. Read it back with
   *                        mpsk_receiver_get_bn_agc_ratio().
   * @return Heap-allocated state, or NULL on invalid args / allocation
   * failure.
   *
   * @note **Zero means derive**, for `m_out`, `zeta`, `lock_thresh`,
   * `num_phases` and `bn_agc_ratio` (gh-644). Every one of those validators
   * previously REJECTED zero, so no working call site can be relying on it,
   * which is what makes the derivation additive rather than a break. The
   * derivation runs BEFORE the validation, so a derived answer faces the same
   * guards a supplied one does. Each is reported back by a getter — without
   * that, `0` would be an instruction whose result nobody can see. See
   * docs/design/mpsk.md §8.1.
   * @note Caller must call mpsk_receiver_destroy() when done.
   */
  mpsk_receiver_state_t *
  mpsk_receiver_create (int m, double sps, size_t m_out, int pulse,
                        double rrc_beta, int rrc_span, double bn_carrier,
                        double zeta, double bn_timing, double lock_thresh,
                        double init_norm_freq, int differential,
                        size_t num_phases, int agc, double bn_agc_ratio);

  /**
   * @brief Create the same receiver behind an R2C halfband: a real IF in.
   *
   * The real-input face. **Every parameter means what it means on
   * mpsk_receiver_create()** — same names, same order, same types, same
   * derivations, the same "zero means derive" rule — because this is the same
   * object and not a twin of it. Only the three rate conventions in this
   * file's header block differ, and each is named against the parameter it
   * touches below.
   *
   * A real-valued IF is the usual output of a single-ended ADC, so this is the
   * face that takes a digitiser's samples directly. Everything downstream —
   * symbols, bits, telemetry, serialization — is one implementation shared
   * with the complex face.
   *
   * @param m              As mpsk_receiver_create().
   * @param sps            Samples per symbol at the REAL input; any double
   *                        **strictly greater than `2 * m_out`**. The cascade
   *                        behind the halfband runs at twice the overall rate,
   *                        and Ddcr requires that rate below 0.5 — so where
   *                        the complex face accepts `sps >= m_out`, this one
   *                        needs twice the headroom. Derived `m_out` honours
   *                        the same bound (@ref mpsk_rx_derive_m_out takes the
   *                        constraint, not the rate), so a caller cannot pair
   *                        an `sps` and an `m_out` that will not construct.
   * @param m_out          As mpsk_receiver_create(); **0 derives** it against
   *                        the strict `sps/2` cap above rather than `sps`.
   * @param pulse          As mpsk_receiver_create().
   * @param rrc_beta       As mpsk_receiver_create().
   * @param rrc_span       As mpsk_receiver_create().
   * @param bn_carrier     As mpsk_receiver_create(). Still normalised to the
   *                        SYMBOL rate: the halfband moves the LO's clock, not
   *                        the loop's units.
   * @param zeta           As mpsk_receiver_create(); 0 derives.
   * @param bn_timing      As mpsk_receiver_create().
   * @param lock_thresh    As mpsk_receiver_create(); 0 derives.
   * @param init_norm_freq The real IF **centre**, cycles/sample at the real
   *                        input rate. An IF at `0.2 * fs` is `0.2`; the
   *                        halved value the LO actually uses is this object's
   *                        business, not the caller's. A real IF must be tuned
   *                        near — this face does not acquire from a cold zero
   *                        the way the complex one does, so the centre is
   *                        where the tap's pull-in range sits *around*.
   * @param differential   As mpsk_receiver_create().
   * @param num_phases     As mpsk_receiver_create(); 0 derives.
   * @param agc            As mpsk_receiver_create(). The AGC sits inside the
   *                        cascade BEHIND the halfband, so it levels the
   *                        analytic signal at the intermediate rate, which is
   *                        also where the noise has already been filtered.
   * @param bn_agc_ratio   As mpsk_receiver_create(); 0 derives.
   * @return Heap-allocated state, or NULL on invalid args / allocation
   *         failure. Destroy with mpsk_receiver_destroy() like any other.
   *
   * @code
   * // QPSK on a real IF at 0.2*fs, 32 samples/symbol, I&D matched filter
   * mpsk_receiver_state_t *rx = mpsk_receiver_create_real (
   *     4, 32.0, 0, MPSK_RX_PULSE_IANDD, 0.35, 8,
   *     0.01, 0.0, 0.01, 0.0, 0.2, 0, 0,
   *     1, 0.0);
   * float complex sym[256];
   * size_t k = mpsk_receiver_steps_real (rx, rx_in, rx_len, sym, 256);
   * mpsk_receiver_destroy (rx);
   * @endcode
   */
  mpsk_receiver_state_t *
  mpsk_receiver_create_real (int m, double sps, size_t m_out, int pulse,
                             double rrc_beta, int rrc_span, double bn_carrier,
                             double zeta, double bn_timing,
                             double lock_thresh, double init_norm_freq,
                             int differential, size_t num_phases, int agc,
                             double bn_agc_ratio);

  /**
   * @brief Gain the front end's AGC is applying, in dB; 0.0 when @c agc = 0.
   *
   * The cascade's own level correction, read back rather than inferred. This
   * is the diagnostic for a level problem: a receiver that will not lock with
   * a healthy `lock` statistic, or one whose timing loop behaves differently
   * at two input levels, is asking about this number. It settles at
   * `-10*log10(P_in / P_ref)` where `P_ref` is the power a unit-amplitude
   * symbol stream has where the AGC sits, so a reading far from 0 dB says the
   * input is far from the level the cascade was built for -- which is fine,
   * and is exactly what the AGC is for, but is worth knowing.
   *
   * Separate from the cascade's filter response (RateConverter_gain()), which
   * is computed from coefficients and stays 1.0; the two multiply.
   */
  double mpsk_receiver_get_agc_gain_db (const mpsk_receiver_state_t *state);

  /**
   * @brief A BPSK receiver stated in the units a caller actually holds: Hz.
   *
   * Same core, same loops, same methods — this differs from
   * mpsk_receiver_create() only in what it ASKS FOR, and that is the point.
   * A caller with a capture holds a sample rate, a symbol rate and a carrier
   * frequency, all in Hz. They do not hold `sps`: that is `fs / Rs`, a ratio
   * this library computes for its own use in selecting a cascade and in
   * costing it. Requiring it makes the caller derive an internal quantity,
   * and then it spreads — because `sps` is in the constructor,
   * `init_norm_freq` has to be cycles per SAMPLE, so stating a carrier
   * offset needs `sps` and `fs` both, while the loop bandwidth on the next
   * line is normalised to the SYMBOL rate. One constructor, two
   * normalisations, and the conversion between them is the caller's problem.
   *
   * So the conversion happens here, once: `sps = sample_rate_hz /
   * symbol_rate_hz` and the LO centre is `carrier_freq_hz / sample_rate_hz`.
   * Nothing on this signature is normalised to anything.
   *
   * **`m` is absent because the type says it.** That is the cheapest
   * parameter to remove and the easiest to miss: a fact carried by the class
   * name is not a parameter on that class.
   *
   * Every argument this does not take is a derive-request in the delegate
   * below — `m_out`, `zeta`, `lock_thresh`, `num_phases`, `bn_agc_ratio` all
   * ask create() for the derived answer, and the NDA tap is the one measured
   * to work at every battery point. They are absent because nobody has a use
   * for them here, not because they are unavailable: `MpskReceiver` still
   * takes every one.
   *
   * @param sample_rate_hz  ADC sample rate, Hz. Must be > 0.
   * @param symbol_rate_hz  Symbol rate, Hz. Must be > 0, and must leave
   *                         `sample_rate_hz / symbol_rate_hz` at or above the
   *                         derived `m_out` — a rate that cannot be strobed
   *                         is refused rather than approximated.
   * @param carrier_freq_hz Carrier centre, Hz (default 0 — complex
   *                         baseband). `|carrier_freq_hz|` must be under
   *                         `sample_rate_hz / 2`; a centre outside Nyquist is
   *                         a mis-stated capture, not a tuning request.
   * @param pulse           Matched-filter shape (default
   *                         MPSK_RX_PULSE_IANDD).
   * @param rrc_beta        RRC roll-off in `[0, 1]` (default 0.35; RRC only).
   * @param rrc_span        RRC one-sided span in symbols (default 8; RRC
   *                         only).
   * @param bn_carrier      Carrier loop noise bandwidth, normalised to the
   *                         symbol rate (default 0.01).
   * @param bn_timing       Symbol-timing loop noise bandwidth, normalised to
   *                         the symbol rate (default 0.01).
   * @param differential    bits(): differential (rotation-invariant) demap
   *                         (default 0, coherent).
   * @param agc             Front-end AGC (default 1).
   * @return Heap-allocated state, or NULL on invalid args / allocation
   * failure. Destroy with mpsk_receiver_destroy() like any other.
   *
   * @code
   * >>> from doppler.track import BpskReceiver
   * >>> rx = BpskReceiver(sample_rate_hz=8e6, symbol_rate_hz=1e6)
   * >>> rx.m                 # the type says it
   * 2
   * >>> rx.sps               # derived from the two rates, not asked for
   * 8.0
   * @endcode
   */
  mpsk_receiver_state_t *mpsk_receiver_create_bpsk (
      double sample_rate_hz, double symbol_rate_hz, double carrier_freq_hz,
      int pulse, double rrc_beta, int rrc_span, double bn_carrier,
      double bn_timing, int differential, int agc);

  /**
   * @brief Destroy an M-PSK receiver and release all memory.
   * @param state  May be NULL.
   */
  void mpsk_receiver_destroy (mpsk_receiver_state_t *state);

  /**
   * @brief Re-seed the front end and both loops to their create-time state.
   *
   * Clears the cascade's filter memory, the carrier and timing NCOs, the
   * loop-filter integrators and the lock detectors, and returns the carrier
   * estimate to @p init_norm_freq. The configuration (order, rate, pulse,
   * bandwidths) is untouched, so the same input fed twice around a reset
   * reproduces the same output bit-for-bit.
   *
   * @param state  Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiver
   * >>> rng = np.random.default_rng(0)
   * >>> idx = rng.integers(0, 4, 300)
   * >>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
   * >>> tx = tx.astype(np.complex64)
   * >>> rx = MpskReceiver(m=4, sps=8, m_out=4)
   * >>> first = rx.steps(tx)
   * >>> rx.reset()                                # back to the cold state
   * >>> np.array_equal(first, rx.steps(tx))       # same input, same output
   * True
   *
   * @endcode
   */
  void mpsk_receiver_reset (mpsk_receiver_state_t *state);

  /**
   * @brief Push one input sample; emit a symbol if it completed one.
   *
   * The composition API: mixes, decimates and matched-filters @p x through the
   * front end at the loops' current control values, then folds every output it
   * produced into both loops. The cascade rate is `m_out/sps <= 1`, so one
   * input can complete at most two output periods and therefore at most one
   * on-time strobe.
   *
   * @param s      State. Must be non-NULL.
   * @param x      One input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @param ted    RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL — pass a literal
   *               for a specialised (branch-free) instantiation.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  mpsk_receiver_step_ted (mpsk_receiver_state_t *s, float complex x,
                          float complex *y_out, int ted)
  {
    float complex ys[4];
    size_t        n     = ddc_execute_ctrl_push_tap2 (
        s->fe.c, x, s->l.timing.ctrl, s->l.freq_ctrl, ys,
        sizeof (ys) / sizeof (ys[0]), NULL, NULL, NULL, NULL);
    return mpsk_rx_fold (&s->l, ys, n, y_out, ted);
  }

  /**
   * @brief Push one REAL input sample; emit a symbol if it completed one.
   *
   * The real face's composition API — mpsk_receiver_step_ted() behind an R2C
   * halfband. Only the front end and the input type differ; everything after
   * the front end is @ref mpsk_rx_fold, shared verbatim, which is what makes
   * "the loops behave identically regardless of front end" a claim about one
   * body of code rather than about two.
   *
   * @param s      State, built by mpsk_receiver_create_real(). Non-NULL.
   * @param x      One real input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @param ted    RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL — pass a literal
   *               for a specialised (branch-free) instantiation.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  mpsk_receiver_step_real_ted (mpsk_receiver_state_t *s, float x,
                               float complex *y_out, int ted)
  {
    float complex ys[4];
    size_t        n     = ddcr_execute_ctrl_push_tap2 (
        s->fe.r, x, s->l.timing.ctrl, s->l.freq_ctrl, ys,
        sizeof (ys) / sizeof (ys[0]), NULL, NULL, NULL, NULL);
    return mpsk_rx_fold (&s->l, ys, n, y_out, ted);
  }

  size_t mpsk_receiver_steps_max_out (mpsk_receiver_state_t *state);
  /**
   * @brief Demodulate a cf32 block and emit the recovered symbols.
   *
   * Runs the per-sample loop (mix + cascade + matched filter, then the carrier
   * and timing loops) over @p x and writes one cf32 symbol per recovered
   * symbol period — roughly `x_len / sps` outputs. Read norm_freq for the
   * tracked carrier and lock for the carrier lock metric.
   *
   * @param state    Receiver state.  Must be non-NULL.
   * @param x        Input cf32 samples.
   * @param x_len    Number of input samples.
   * @param out      Output symbols; caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of symbols written.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiver
   * >>> rng = np.random.default_rng(0)
   * >>> idx = rng.integers(0, 4, 3000)                  # QPSK symbols
   * >>> tx = np.repeat(np.exp(1j * (2 * np.pi * idx / 4 + np.pi / 4)), 8)
   * >>> tx = tx.astype(np.complex64)                    # 8 samples/symbol
   * >>> rx = MpskReceiver(m=4, sps=8, m_out=4, bn_carrier=0.02)
   * >>> sym = rx.steps(tx)                              # blind NDA acquire
   * >>> sym.size                                        # ~ x_len / sps
   * 2998
   * >>> rx.lock > 0.8                                   # carrier locked
   * True
   *
   * @endcode
   */
  size_t mpsk_receiver_steps (mpsk_receiver_state_t *state,
                              const float complex *x, size_t x_len,
                              float complex *out, size_t max_out);

  size_t mpsk_receiver_bits_max_out (mpsk_receiver_state_t *state);
  /**
   * @brief Demodulate a cf32 block and emit hard Gray-coded bits.
   *
   * Like mpsk_receiver_steps(), but each recovered symbol is sliced to its
   * nearest M-PSK point and unpacked to log2(M) hard bits (LSB-first). With
   * the differential option set at create time, the Gray label is taken from
   * the phase *difference* between consecutive symbols (rotation-invariant —
   * it resolves the M-fold carrier ambiguity), else from the absolute
   * (coherent) decision.
   *
   * @param state    Receiver state.  Must be non-NULL.
   * @param x        Input cf32 samples.
   * @param x_len    Number of input samples.
   * @param out      Output bytes (0/1); caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of bits written.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiver
   * >>> rng = np.random.default_rng(3)
   * >>> idx = rng.integers(0, 2, 3000)                  # BPSK payload bits
   * >>> tx = np.repeat(np.exp(1j * np.pi * idx), 8).astype(np.complex64)
   * >>> rx = MpskReceiver(m=2, sps=8, m_out=4, bn_carrier=0.005)
   * >>> b = rx.bits(tx)                                 # 1 hard bit/symbol
   * >>> b.size
   * 2998
   * >>> # settled tail matches the payload, up to the BPSK
   * >>> # inversion ambiguity and the pipeline's one-symbol lead
   * >>> tail = np.mean(b[1001:2001] != idx[1000:2000])
   * >>> round(float(min(tail, 1 - tail)), 3)
   * 0.0
   *
   * @endcode
   */
  size_t mpsk_receiver_bits (mpsk_receiver_state_t *state,
                             const float complex *x, size_t x_len,
                             uint8_t *out, size_t max_out);

  size_t mpsk_receiver_steps_real_max_out (mpsk_receiver_state_t *state);
  /**
   * @brief Demodulate a real f32 block and emit the recovered symbols.
   *
   * mpsk_receiver_steps() taking real samples: the R2C halfband makes them
   * complex before anything else touches them, and the per-sample body is the
   * same one. Requires a state built by mpsk_receiver_create_real().
   *
   * @param state    Must be non-NULL.
   * @param x        Real f32 input samples.
   * @param x_len    Number of input samples.
   * @param out      Output symbols; caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of symbols written.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiverR
   * >>> rng = np.random.default_rng(3)
   * >>> idx = rng.integers(0, 4, 2400)                  # QPSK symbols
   * >>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 32)  # 32 sps
   * >>> n = np.arange(bb.size)
   * >>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4
   * >>> x = np.ascontiguousarray(x.astype(np.float32))
   * >>> rx = MpskReceiverR(m=4, sps=32, m_out=8, init_norm_freq=0.25)
   * >>> sym = rx.steps(x)
   * >>> sym.size                                        # ~ x_len / sps
   * 2398
   * >>> rx.lock > 0.8                                   # carrier locked
   * True
   *
   * @endcode
   */
  size_t mpsk_receiver_steps_real (mpsk_receiver_state_t *state,
                                   const float *x, size_t x_len,
                                   float complex *out, size_t max_out);

  size_t mpsk_receiver_bits_real_max_out (mpsk_receiver_state_t *state);
  /**
   * @brief Demodulate a real f32 block and emit hard Gray-coded bits.
   *
   * mpsk_receiver_bits() taking real samples. Requires a state built by
   * mpsk_receiver_create_real().
   *
   * @param state    Must be non-NULL.
   * @param x        Real f32 input samples.
   * @param x_len    Number of input samples.
   * @param out      Output bytes (0/1); caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of bits written.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiverR
   * >>> rng = np.random.default_rng(3)
   * >>> idx = rng.integers(0, 2, 2400)                  # BPSK payload bits
   * >>> bb = np.repeat(np.exp(1j * np.pi * idx), 32)
   * >>> n = np.arange(bb.size)
   * >>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4
   * >>> x = np.ascontiguousarray(x.astype(np.float32))
   * >>> rx = MpskReceiverR(m=2, sps=32, m_out=8, init_norm_freq=0.25,
   * ...                    bn_carrier=0.005)
   * >>> b = rx.bits(x)                                  # 1 hard bit/symbol
   * >>> b.size
   * 2398
   * >>> # settled tail matches the payload, up to the BPSK
   * >>> # inversion ambiguity
   * >>> tail = np.mean(b[1500:2300] != idx[1500:2300])
   * >>> round(float(min(tail, 1 - tail)), 3)
   * 0.0
   *
   * @endcode
   */
  size_t mpsk_receiver_bits_real (mpsk_receiver_state_t *state, const float *x,
                                  size_t x_len, uint8_t *out, size_t max_out);

  /** @brief Carrier frequency the receiver is tracking, cycles/sample at the
   *  input rate: the create-time centre plus the loop's own estimate. */
  double mpsk_receiver_get_norm_freq (const mpsk_receiver_state_t *state);
  /** @brief Instantaneous NCO frequency command (carrier loop filter output,
   * cycles/sample): mean tracks a ramp with no lag, variance is loop stress. */
  double mpsk_receiver_get_nco_freq (const mpsk_receiver_state_t *state);
  /** @brief Retune to @p val cycles/sample: moves the LO centre there and
   *  zeroes the loop's residual estimate, so norm_freq reads back exactly. */
  void mpsk_receiver_set_norm_freq (mpsk_receiver_state_t *state, double val);
  double mpsk_receiver_get_lock (const mpsk_receiver_state_t *state);
  /** @brief Binary carrier-lock flag from the loop's hysteretic (up/down
   * verify-counted) lock detector — de-chattered, unlike the raw metric. */
  int mpsk_receiver_get_locked (const mpsk_receiver_state_t *state);

  /**
   * @brief Symbols from reset to the FIRST carrier-lock declaration, or -1 if
   *        the receiver has not locked yet.
   *
   * The acquisition time, as a number a caller can read rather than infer by
   * polling `locked` in a loop. Dated by the same hysteretic detector
   * `mpsk_receiver_get_locked()` reports, so the two cannot disagree.
   *
   * In SYMBOLS, not seconds: `bn_carrier` and `bn_timing` are both normalised
   * to the symbol rate, so a settling budget quoted in symbols is comparable
   * across every input rate, and a caller with `Rs` divides once. Only the
   * first declaration is dated — a drop and re-acquire does not restamp it,
   * because the question this answers is "how long did this receiver take to
   * lock", not "when did it last hold". mpsk_receiver_reset() clears it to -1.
   */
  int64_t mpsk_receiver_get_lock_time (const mpsk_receiver_state_t *state);
  /** @brief Carrier loop phase discriminator (rad) — the residual phase the
   * loop is trying to null; loop stress. */
  double mpsk_receiver_get_last_error (const mpsk_receiver_state_t *state);

  /**
   * @brief Attach (or detach) a telemetry context across the receiver.
   * Registers the receiver's own "<prefix>.lock" probe (the carrier lock
   * EMA), then the carrier loop's "<prefix>.car.e" / ".freq" / ".locked" and
   * the symbol-timing loop's "<prefix>.sync.e" / ".ctrl" / ".rate" / ".lock" /
   * ".locked" / ".mu" -- ten probes emitted once per recovered symbol --
   * then the front end's AGC under "<prefix>.agc" ("<prefix>.agc.gain_db" and
   * "<prefix>.agc.level_db"; see agc_set_telemetry()).  Twelve probes total,
   * all thinned by @p decim.  Passing NULL detaches everything.
   *
   * @warning The two AGC probes are NOT at the symbol rate the other ten
   * are.  That AGC sits pre-terminal in the cascade (RateConverter's tap,
   * ahead of the stage the timing loop steers) and emits once per
   * gain-update event, i.e. every @c AGC_DECIM_DEFAULT samples of that
   * fixed-rate stream -- so it reports on a grid that depends on the planned
   * cascade, not on recovered symbols, and a run yields a different number of
   * AGC records than carrier records.  Compare the two by TIME, never by
   * record index.  This is deliberate: the AGC's bandwidth is quoted in the
   * pre-terminal stream's units precisely so it is not coupled to the loop
   * that is stretching the symbol grid (see RateConverter_enable_agc()).
   *
   * Instrumenting it matters because it is FIRST in the chain, and a level
   * error is the one kind no downstream loop can correct for itself: a TED
   * normalises by its own construct-time slope, so it reads a level error as
   * a loop-gain error (A^2 Gardner, A DTTL) with no other reference to catch
   * it.  This receiver also makes the AGC the slowest of its three loops by
   * construction -- mpsk_rx_agc_bn() derives its bandwidth as a fraction of
   * the slowest loop it feeds, and bn_agc_ratio is validated to (0, 1) -- but
   * that is a choice of THIS composition, and slowest does not by itself mean
   * longest: settling is set by the bandwidth AND by how far the level starts
   * from the reference, which is unknown at construction.  Which is exactly
   * why it has to be measured rather than inferred; the zero-referenced
   * "<prefix>.agc.level_db" is what makes that possible.
   *
   * With @c agc = 0 at construction there is no AGC to attach and the two
   * probes are simply absent (fourteen, not sixteen); this still returns
   * DP_OK.
   *
   * Setup path, never hot; the context is borrowed and must outlive the
   * attachment (SPSC rules in dp_tlm/dp_tlm_core.h).
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "rx".
   * @param decim  Emit every decim-th symbol (every decim-th gain update for
   *               the two AGC probes); >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take the
   *         probes (the attach fails whole; everything detached).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiver
   * >>> from doppler.telemetry import Telemetry
   * >>> tlm = Telemetry(1 << 14)   # 15 probes x ~512 syms + headroom
   * >>> rx = MpskReceiver(m=4, sps=4, m_out=2)
   * >>> rx.set_telemetry(tlm, "rx")
   * >>> len(tlm.probe_names)
   * 15
   * >>> rng = np.random.default_rng(7)
   * >>> syms = (1 - 2 * rng.integers(0, 2, 512)).astype(np.complex64)
   * >>> x = np.repeat(syms, 4)
   * >>> _ = rx.steps(x)
   * >>> recs = tlm.read()
   * >>> tlm.dropped        # size the ring, or the counts below diverge
   * 0
   * >>> n_sync = len(recs[recs["probe"] == tlm.probe_id("rx.sync.e")])
   * >>> n_car = len(recs[recs["probe"] == tlm.probe_id("rx.car.e")])
   * >>> n_sync > 0 and n_sync == n_car
   * True
   * >>> n_agc = len(recs[recs["probe"] == tlm.probe_id("rx.agc.gain_db")])
   * >>> n_agc > 0 and n_agc != n_sync   # cascade grid, not symbol grid
   * True
   *
   * @endcode
   */
  int mpsk_receiver_set_telemetry (mpsk_receiver_state_t *state, dp_tlm_t *tlm,
                                   const char *prefix, uint32_t decim);
  /** @brief Smoothed tracked samples per symbol — departs from the nominal
   *  `sps` by exactly the sample-clock offset the timing loop is tracking. */
  double mpsk_receiver_get_timing_rate (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_m (const mpsk_receiver_state_t *state);
  double mpsk_receiver_get_sps (const mpsk_receiver_state_t *state);
  /** @brief Terminal outputs per symbol (the old `n`, now the cascade's). */
  size_t mpsk_receiver_get_m_out (const mpsk_receiver_state_t *state);

  /** @brief Loop damping in use — derived `1/sqrt(2)` unless pinned (§8.1). */
  double mpsk_receiver_get_zeta (const mpsk_receiver_state_t *state);

  /** @brief AGC bandwidth ratio in use — derived unless pinned (§8.1). */
  double mpsk_receiver_get_bn_agc_ratio (const mpsk_receiver_state_t *state);

  /** @brief Carrier lock DECLARE threshold in use — derived unless pinned
   *         (§8.1). It gates no loop and no output; see mpsk_rx_loops.h. */
  double mpsk_receiver_get_lock_thresh (const mpsk_receiver_state_t *state);

  /**
   * @brief Carrier DROP threshold in use — `MPSK_RX_LOCK_DOWN` x the
   *        declare threshold, the level hysteresis the pair is stated with.
   *
   * Readable for the same reason the declare side is: a caller plotting the
   * lock statistic needs both edges to know what the decision was reading,
   * and deriving `0.8 *` in a plotting script puts a second copy of the
   * hysteresis rule outside the object that owns it.
   */
  double
  mpsk_receiver_get_lock_drop_thresh (const mpsk_receiver_state_t *state);

  /** @brief Timing DECLARE threshold on `sync.lock`, derived by symsync's
   *         own (rolloff, esno_min, pfa, pd) geometry rather than pinned. */
  double
  mpsk_receiver_get_sync_lock_thresh (const mpsk_receiver_state_t *state);

  /** @brief Timing DROP threshold on `sync.lock`. Equal to the declare
   *         threshold when the timing loop carries no level hysteresis. */
  double
  mpsk_receiver_get_sync_lock_drop_thresh (const mpsk_receiver_state_t *state);

  /** @brief Matched-filter bank arms in use — derived unless pinned (§8.1). */
  size_t mpsk_receiver_get_num_phases (const mpsk_receiver_state_t *state);

  /** @brief Has the cascade's CIC stage clipped its input since the last
   *  reset? A CIC bounds its input to +-1.0 and clips silently past that,
   *  which costs ~25 dB of EVM behind a perfectly healthy lock. */
  int mpsk_receiver_get_clipped (const mpsk_receiver_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: the front end's and the loops' self-validating child blobs.
 * Every scalar this object carries across inputs lives in one of them; the
 * cascade, its banks, the LO centre and the `real` tag are restored by create.
 *
 * The MAGIC is keyed on the face. The two faces hold genuinely different
 * state — a DDC's cascade against a DDCR's halfband plus cascade — so a blob
 * taken from one must be REFUSED by the other rather than reinterpreted, and
 * refused at the envelope by name rather than three levels down in a child.
 * The layouts are otherwise identical, which is why one triplet serves both.
 */
#define MPSK_RECEIVER_STATE_MAGIC DP_FOURCC ('M', 'P', 'S', 'K')
#define MPSK_RECEIVER_STATE_VERSION 6u /* v5: rebuilt on the matched DDC */
#define MPSK_RECEIVER_R_STATE_MAGIC DP_FOURCC ('M', 'P', 'S', 'R')
#define MPSK_RECEIVER_R_STATE_VERSION 2u
  size_t mpsk_receiver_state_bytes (const mpsk_receiver_state_t *state);
  void   mpsk_receiver_get_state (const mpsk_receiver_state_t *state,
                                  void                        *blob);
  int mpsk_receiver_set_state (mpsk_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RECEIVER_CORE_H */
