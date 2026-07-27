/**
 * @file mpsk_receiver_core.h
 * @brief Pulse-shaped M-PSK receiver: a tuned matched DDC and two loops.
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
 * symbols at the end of it. Two discriminators steer the one LO:
 *   - **acquisition** — the NDA M-th-power error on every terminal output,
 *     needing no data and no symbol timing (`tracking == 0`).
 *   - **tracking** — a decision-directed error `e = Im(y·conj(â))/|y|` on the
 *     recovered symbols (lower jitter, at the symbol rate).
 * The handover is opt-in (`acq_to_track`) and two-way, and the shared loop
 * filter carries the frequency estimate across it in both directions, so a
 * drop-back is a discriminator swap rather than a cold re-acquisition. See
 * mpsk_rx_loops.h for why the two discriminators run at different rates and
 * how the estimate survives the change.
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
 * demapping (`bits(..., differential=1)`) or a sync word downstream. The
 * real-input twin lives in mpsk_receiver_r/mpsk_receiver_r_core.h; a
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
 *     0.01, 0.707, 0.01, 0, 0.5, 0.0, 100, 0, 1024);
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
#include "boxcar/boxcar_core.h"
#include "telemetry/telemetry.h"
#include "ber/ber_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief M-PSK receiver state.
   *
   * Allocate with mpsk_receiver_create(). Owns the matched DDC (`fe`) and
   * embeds the loops by value. Treat all fields as internal (use the
   * getters); they are exposed for the inline sample loop.
   */
  typedef struct
  {
    ddc_state_t    *fe; /**< matched DDC: mix + cascade + matched filter. */
    mpsk_rx_loops_t l;  /**< carrier + timing loops, handover, demapper.  */

    /* ── config (restored by create(), never packed in a state blob) ── */
    /* The pulse geometry lives in the front end, which is the only thing
       that uses it; keeping a second copy here would be a shadow of the
       cascade's own configuration, free to drift out of step with it. */
    double centre_freq; /**< create-time carrier offset (cycles/sample).  */
  } mpsk_receiver_state_t;

  /**
   * @brief Create an M-PSK receiver.
   *
   * @param m              Constellation order M, 2/4/8 (default 4 = QPSK).
   * @param sps            Samples per symbol; any double >= @p m_out (8.0
   *                        by default, but 17.33389 is equally valid).
   * @param m_out          Terminal outputs per symbol: even, 2..8 (default
   *                        8). Gardner needs the half-symbol gate. The
   *                        default is 8 for two reasons. The matched
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
   *                        optional at M = 8.** At `MPSK_RX_NDA_TAP_MF_ALL`,
   *                        where the M-th power runs on the oversampled pulse
   *                        rather than the strobe, the requirement is the
   *                        blunt `m_out >= M`; since m_out maxes at 8, 8PSK
   *                        there is exactly critically sampled. **Never pair 2 with
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
   * @param zeta           Damping factor for both loops (default 0.707).
   * @param bn_timing      Symbol-timing loop noise bandwidth, normalised to
   *                        the symbol rate (default 0.01).
   * @param acq_to_track   Enable the two-way NDA<->decision-directed
   *                        handover (default 0).
   * @param lock_thresh    Handover declare threshold on the carrier lock
   *                        metric (default 0.5); the drop threshold sits at
   *                        0.8x for level hysteresis, and both directions
   *                        are verify-counted (8 symbols up / 32 down).
   *                        The metric is `Re((z/|z|)^M)` smoothed by an EMA,
   *                        whose noise-only sd is 0.1132 for **every** M, so
   *                        the threshold is `0.5 / 0.1132` = 4.42 noise sigmas
   *                        — a per-look false-alarm probability of 5e-6. Pick
   *                        a value by dividing your Pfa's z-score into 0.1132
   *                        rather than by feel; see carrier_nda_core.h for the
   *                        derivation and the measured verification.
   * @param init_norm_freq Seed carrier frequency, cycles/sample at the input
   *                        rate (default 0.0). This is the centre the LO is
   *                        tuned to; the loop tracks the residual around it.
   * @param warmup_syms    Symbols before the acq-to-track switch is allowed
   *                        (default 100).
   * @param differential   bits(): differential (rotation-invariant) demap
   *                        (default 0 = coherent).
   * @param num_phases     Terminal-stage bank arms; a power of two (default
   *                        1024). Sets the timing resolution to
   *                        `1/num_phases` of an output period.
   * @param nda_tap        MPSK_RX_NDA_TAP_* — where the NDA carrier
   *                        discriminator reads, which sets its pull-in range
   *                        and whether it needs symbol timing at all. An
   *                        M-th-power detector updating at rate `F` can only
   *                        observe `|df| < F/(2M)`, so the tap point IS the
   *                        range:
   *                        - `MPSK_RX_NDA_TAP_STROBE` (0, default) — the
   *                          on-time strobe, at `Rs`. Cleanest input,
   *                          narrowest range, and the ONLY tap whose input
   *                          quality depends on the timing loop — it steers
   *                          from the first strobe regardless, so pick
   *                          another tap if the carrier must acquire first.
   *                        - `MPSK_RX_NDA_TAP_MF_ALL` (1) — every terminal
   *                          output, at `m_out*Rs`. No timing dependence,
   *                          paid for with the ISI the between-symbol
   *                          outputs carry (worst at 8PSK, where the
   *                          decision margin is smallest).
   *                        - `MPSK_RX_NDA_TAP_LO_ARM` (2) — ahead of the
   *                          cascade, through a free-running half-symbol
   *                          boxcar at the LO rate. Widest range and fully
   *                          timing-independent, but unmatched, so it pays
   *                          squaring loss; it does NOT work at 8PSK (the
   *                          8th-power gain over a boxcar arm collapses).
   *                        Measured unaided, QPSK at `sps = 8, m_out = 8`,
   *                        each at its own best `bn_carrier`: `0.050*Rs`
   *                        (strobe), `0.033*Rs` (mf_all), `0.090*Rs`
   *                        (lo_arm). Fixed at construction — nothing
   *                        switches underneath the caller. Note `df = k*F/M`
   *                        is a stable FALSE lock at every tap, reporting a
   *                        healthy lock statistic that no self-referenced
   *                        metric can flag. For more range than any tap
   *                        gives, put a coarse frequency estimate in front
   *                        and pass it as @p init_norm_freq.
   * @return Heap-allocated state, or NULL on invalid args / allocation
   * failure.
   * @note Caller must call mpsk_receiver_destroy() when done.
   */
  mpsk_receiver_state_t *
  mpsk_receiver_create (int m, double sps, size_t m_out, int pulse,
                        double rrc_beta, int rrc_span, double bn_carrier,
                        double zeta, double bn_timing, int acq_to_track,
                        double lock_thresh, double init_norm_freq,
                        size_t warmup_syms, int differential,
                        size_t num_phases, int nda_tap);

  /**
   * @brief Destroy an M-PSK receiver and release all memory.
   * @param state  May be NULL.
   */
  void mpsk_receiver_destroy (mpsk_receiver_state_t *state);

  /**
   * @brief Re-seed the front end and both loops to their create-time state.
   * @param state  Must be non-NULL.
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
    float complex zlo;
    int           n_lo = 0;
    size_t        n    = ddc_execute_ctrl_push_tap (
        s->fe, x, s->l.timing.ctrl, s->l.freq_ctrl, ys,
        sizeof (ys) / sizeof (ys[0]), &zlo, &n_lo);
    /* The widest NDA tap reads here — ahead of the cascade, so it needs no
       symbol timing. A no-op for every other tap. */
    if (n_lo)
      mpsk_rx_push_lo (&s->l, zlo);
    int           emitted = 0;
    for (size_t oi = 0; oi < n; oi++)
      emitted |= mpsk_rx_take_output (&s->l, ys[oi], y_out, ted);
    return emitted;
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
   */
  size_t mpsk_receiver_bits (mpsk_receiver_state_t *state,
                             const float complex *x, size_t x_len,
                             uint8_t *out, size_t max_out);

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
  /** @brief Carrier loop phase discriminator (rad) — the residual phase the
   * loop is trying to null; loop stress. */
  double mpsk_receiver_get_last_error (const mpsk_receiver_state_t *state);

  /**
   * @brief Re-tune the acquisition<->tracking handover detector directly.
   *
   * Full lockdet control over the handover, mirroring costas_configure_lock():
   * a split declare/drop threshold pair on the carrier lock EMA (level
   * hysteresis) and both verify counts (time hysteresis). A live handover
   * survives the re-tune; the in-flight verify run restarts.
   *
   * @param state        Must be non-NULL.
   * @param up_thresh    Declare threshold on the carrier lock EMA.
   * @param down_thresh  Drop threshold; choose <= up_thresh for level
   *                     hysteresis.
   * @param n_up         Consecutive above-threshold symbols to hand over
   *                     to the decision-directed discriminator; clamped
   *                     >= 1.
   * @param n_down       Consecutive below-threshold symbols to fall back
   *                     to NDA acquisition; clamped >= 1.
   * @code
   * >>> from doppler.track import MpskReceiver
   * >>> rx = MpskReceiver(m=4, sps=4, m_out=2, acq_to_track=1)
   * >>> rx.tracking
   * 0
   * >>> rx.configure_lock(0.9, 0.72, 4, 16)   # tighter declare, faster drop
   *
   * @endcode
   */
  void mpsk_receiver_configure_lock (mpsk_receiver_state_t *state,
                                     double up_thresh, double down_thresh,
                                     uint32_t n_up, uint32_t n_down);

  /**
   * @brief Attach (or detach) a telemetry context across the receiver.
   * Registers the receiver's own "<prefix>.lock" probe (the carrier lock
   * EMA) and "<prefix>.tracking" (the two-way handover decision, 0/1 —
   * so a consumer sees exactly when the carrier was handed to the
   * decision-directed discriminator or dropped back to NDA), then the
   * carrier loop's "<prefix>.car.e" / ".freq" / ".locked" and the
   * symbol-timing loop's "<prefix>.sync.e" / ".ctrl" / ".rate" / ".lock" /
   * ".locked" / ".mu" -- eleven probes total, all thinned by @p decim and all
   * emitted once per recovered symbol.  Passing NULL detaches everything.
   * Setup path, never hot; the context is borrowed and must outlive the
   * attachment (SPSC rules in telemetry/telemetry.h).
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "rx".
   * @param decim  Emit every decim-th symbol; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take the
   *         eleven probes (the attach fails whole; everything detached).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiver
   * >>> from doppler.telemetry import Telemetry
   * >>> tlm = Telemetry(1 << 14)   # 11 probes x ~512 symbols, with headroom
   * >>> rx = MpskReceiver(m=4, sps=4, m_out=2)
   * >>> rx.set_telemetry(tlm, "rx")
   * >>> len(tlm.probe_names())
   * 11
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
   *
   * @endcode
   */
  int mpsk_receiver_set_telemetry (mpsk_receiver_state_t *state, dp_tlm_t *tlm,
                                   const char *prefix, uint32_t decim);
  /** @brief Smoothed tracked samples per symbol — departs from the nominal
   *  `sps` by exactly the sample-clock offset the timing loop is tracking. */
  double mpsk_receiver_get_timing_rate (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_tracking (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_m (const mpsk_receiver_state_t *state);
  double mpsk_receiver_get_sps (const mpsk_receiver_state_t *state);
  /** @brief Terminal outputs per symbol (the old `n`, now the cascade's). */
  size_t mpsk_receiver_get_m_out (const mpsk_receiver_state_t *state);
  /** @brief Has the cascade's CIC stage clipped its input since the last
   *  reset? A CIC bounds its input to +-1.0 and clips silently past that,
   *  which costs ~25 dB of EVM behind a perfectly healthy lock. */
  int mpsk_receiver_get_clipped (const mpsk_receiver_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: the front end's and the loops' self-validating child blobs.
 * Every scalar this object carries across inputs lives in one of them; the
 * cascade, its banks and the LO centre are restored by create. */
#define MPSK_RECEIVER_STATE_MAGIC DP_FOURCC ('M', 'P', 'S', 'K')
#define MPSK_RECEIVER_STATE_VERSION 6u /* v5: rebuilt on the matched DDC */
  size_t mpsk_receiver_state_bytes (const mpsk_receiver_state_t *state);
  void   mpsk_receiver_get_state (const mpsk_receiver_state_t *state,
                                  void                        *blob);
  int mpsk_receiver_set_state (mpsk_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RECEIVER_CORE_H */
