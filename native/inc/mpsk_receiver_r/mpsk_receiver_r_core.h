/**
 * @file mpsk_receiver_r_core.h
 * @brief Real-input M-PSK receiver: the complex twin behind an R2C front end.
 *
 * @ref mpsk_receiver_state_t's real-input counterpart, and the same object in
 * every way that matters — it owns a @ref ddcr_state_t (the matched flavor)
 * instead of a @ref ddc_state_t, and drives the identical
 * @ref mpsk_rx_loops_t. Everything the loops do — the symbol-timing loop on
 * the cascade's `rate_ctrl` port, the M-th-power NDA and decision-directed
 * carrier discriminators on the LO's `freq_ctrl` port, the two-way handover,
 * the demapper — is one shared implementation, not a copy. Read
 * mpsk_rx_loops.h for all of it; only what the front end changes is described
 * here.
 *
 * ```
 *   f32 in ──> MatchedDdcr ─────────────────────────────> y ──> loops ──> syms
 *               halfband R2C (2:1) · LO mix · cascade · MF
 * ```
 *
 * Two consequences follow from the halfband, and they are the whole
 * difference:
 *
 * **The LO runs at half the input rate.** The R2C halfband decimates 2:1 (with
 * the fs/4 shift baked in) *before* the mix, so the LO sees `sps/2` samples
 * per symbol. `freq_ctrl` is in cycles per sample at that intermediate rate,
 * which is why @ref mpsk_rx_loops_t takes `lo_sps` as a parameter rather than
 * assuming it equals `sps`. `norm_freq` stays caller-facing in cycles/sample
 * at the **input** rate, so it is halved on the way in and doubled on the way
 * out — the conversion lives in this file and nowhere else.
 *
 * **`sps` must exceed `2 * m_out`.** The cascade behind the halfband runs at
 * `2 * rate`, and Ddcr requires `rate < 0.5`; the receiver asks for
 * `rate = m_out/sps`, so `sps > 2 * m_out` (against `sps >= m_out` for the
 * complex type). At the default `m_out = 8` that is `sps > 16`, which is why
 * this type's `sps` default is 32.0 where the complex twin's is 8.0.
 *
 * A real-valued IF is the usual output of a single-ended ADC, so this is the
 * type that takes a digitiser's samples directly. Everything downstream —
 * symbols, bits, telemetry, serialization — is identical to the complex twin.
 *
 * Lifecycle: `mpsk_receiver_r_create -> (steps / bits / reset)* -> _destroy`.
 *
 * @code
 * // QPSK on a real IF at 0.2*fs, 16 samples/symbol, RRC matched filter
 * mpsk_receiver_r_state_t *rx = mpsk_receiver_r_create (
 *     4, 16.0, 4, MPSK_RX_PULSE_RRC, 0.35, 8,
 *     0.005, 0.707, 0.01, 0, 0.5, 0.2, 100, 0, 1024);
 * float complex sym[256];
 * size_t k = mpsk_receiver_r_steps (rx, rx_in, rx_len, sym, 256);
 * mpsk_receiver_r_destroy (rx);
 * @endcode
 */
#ifndef MPSK_RECEIVER_R_CORE_H
#define MPSK_RECEIVER_R_CORE_H

#include "clib_common.h"
#include "ddcr/ddcr_core.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "mpsk_receiver/mpsk_rx_loops.h"
#include <complex.h>
#include "ddc/ddc_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
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
   * @brief Real-input M-PSK receiver state.
   *
   * Allocate with mpsk_receiver_r_create(). Owns the matched DDCR (`fe`) and
   * embeds the shared loops by value.
   */
  typedef struct
  {
    ddcr_state_t   *fe; /**< matched DDCR: R2C + mix + cascade + MF.     */
    mpsk_rx_loops_t l;  /**< carrier + timing loops, handover, demapper. */
    double centre_freq; /**< create-time carrier offset at the INPUT rate. */
  } mpsk_receiver_r_state_t;

  /**
   * @brief Create a real-input M-PSK receiver.
   *
   * Parameters match mpsk_receiver_create() exactly; only two behave
   * differently, both because of the R2C halfband:
   *
   * @param m              Constellation order M, 2/4/8 (default 4 = QPSK).
   * @param sps            Samples per symbol at the REAL input; any double
   *                        **strictly greater than `2 * m_out`** (default
   *                        32.0). The cascade behind the halfband runs at
   *                        twice the overall rate, and Ddcr requires that
   *                        rate below 0.5.
   * @param m_out          Terminal outputs per symbol: even, 2..8 (default
   *                        8) — where an I&D matched filter reaches the
   *                        coherent bound; see the complex twin's create()
   *                        for the measurements. It is this default that
   *                        forces `sps`'s to 32.0.
   * @param pulse          Matched-filter shape (default MPSK_RX_PULSE_IANDD).
   * @param rrc_beta       RRC roll-off in `[0, 1]` (default 0.35; RRC only).
   * @param rrc_span       RRC one-sided span in symbols (default 8; RRC only).
   * @param bn_carrier     Carrier loop noise bandwidth, normalised to the
   *                        symbol rate (default 0.005).
   * @param zeta           Damping factor for both loops (default 0.707).
   * @param bn_timing      Timing loop noise bandwidth, per symbol (0.01).
   * @param acq_to_track   Enable the two-way handover (default 0).
   * @param lock_thresh    Handover declare threshold (default 0.5).
   * @param init_norm_freq Carrier frequency to tune to, cycles/sample **at
   *                        the real input rate** (default 0.0). A real IF at
   *                        `0.2 * fs` is `0.2`; the halved value the LO
   *                        actually uses is this object's business, not the
   *                        caller's.
   * @param differential   bits(): differential demap (default 0).
   * @param num_phases     Terminal-stage bank arms, a power of two (1024).
   * @param nda_tap        MPSK_RX_NDA_TAP_* — where the NDA carrier
   *                        discriminator reads, and so its pull-in range:
   *                        `_STROBE` (0) at `Rs` and the only tap
   *                        needing symbol timing, or `_MF_OUT` (1) at
   *                        `m_out*Rs`. See mpsk_receiver_create() for the
   *                        full trade and the measured ranges.
   *
   *                        `_MF_IN` is NOT accepted here yet: it reads the
   *                        cascade's `bank_sps` rate, which this front end
   *                        does not publish (its `ddcr` carries the same
   *                        RateConverter, so wiring it is small — measured,
   *                        `bank_sps` comes out identical on both types).
   *                        Construction refuses it rather than falling back
   *                        to a rate that would mis-size the loop.
   *
   *                        One further difference: this type does not acquire
   *                        from a cold zero the way the complex twin does — a
   *                        real IF must be tuned near, so @p init_norm_freq
   *                        is the centre and a tap buys pull-in *around* it,
   *                        not from nothing.
   * @param agc            Non-zero (default) puts this receiver's ONE AGC in
   *                        the front-end cascade, before the terminal
   *                        matched stage — the same placement and the same
   *                        reason as the complex twin
   *                        (@ref mpsk_receiver_create): it serves BOTH
   *                        loops, since carrier and timing both run on its
   *                        output. The timing detector is the one whose
   *                        gain depends on the level (its slope is a
   *                        construct-time constant for a unit-amplitude
   *                        stream); the carrier detector normalises itself
   *                        but still sees the AGC's transient. Pass 0 and
   *                        the timing loop is under-driven by `A^2`.
   * @param bn_agc_ratio   That AGC's bandwidth as a fraction of the SLOWEST
   *                        loop it feeds, `min(bn_carrier, bn_timing)` — see
   *                        @ref mpsk_rx_agc_bn. In (0, 1), refused at 1 or
   *                        above; `MPSK_RX_AGC_BW_RATIO` (0.05) by default.
   * @return Heap-allocated state, or NULL on invalid args / allocation
   *         failure.
   * @note Caller must call mpsk_receiver_r_destroy() when done.
   */
  mpsk_receiver_r_state_t *
  mpsk_receiver_r_create (int m, double sps, size_t m_out, int pulse,
                          double rrc_beta, int rrc_span, double bn_carrier,
                          double zeta, double bn_timing, int acq_to_track,
                          double lock_thresh, double init_norm_freq,
                          int differential,
                          size_t num_phases, int nda_tap, int agc,
                          double bn_agc_ratio);

  /**
   * @brief Gain the front end's AGC is applying, in dB; 0.0 when @c agc = 0.
   *
   * The twin of mpsk_receiver_get_agc_gain_db(), and the same diagnostic: a
   * receiver that will not lock with a healthy `lock` statistic, or one whose
   * timing loop behaves differently at two input levels, is asking about this
   * number. Note the AGC sits inside the cascade BEHIND the R2C halfband, so
   * it levels the analytic signal at the intermediate rate, not the real
   * input -- which is what makes its reference the same derived
   * `bank_e0 / bank_sps` the complex twin uses.
   */
  double
  mpsk_receiver_r_get_agc_gain_db (const mpsk_receiver_r_state_t *state);

  /** @brief Destroy and release all memory. @param state May be NULL. */
  void mpsk_receiver_r_destroy (mpsk_receiver_r_state_t *state);

  /**
   * @brief Re-seed the front end and both loops to their create-time state.
   *
   * Identical in effect to mpsk_receiver_reset() — clears the R2C halfband and
   * cascade memory, the carrier and timing NCOs, the loop integrators and the
   * lock detectors, and returns the carrier estimate to @p init_norm_freq.
   * Configuration is untouched, so a burst fed twice around a reset reproduces
   * bit-for-bit.
   *
   * @param state  Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiverR
   * >>> rng = np.random.default_rng(0)
   * >>> idx = rng.integers(0, 4, 300)
   * >>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 32)
   * >>> n = np.arange(bb.size)
   * >>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real  # IF at fs/4
   * >>> x = np.ascontiguousarray(x.astype(np.float32))
   * >>> rx = MpskReceiverR(m=4, sps=32, m_out=8, init_norm_freq=0.25)
   * >>> first = rx.steps(x)
   * >>> rx.reset()                                # back to the cold state
   * >>> np.array_equal(first, rx.steps(x))        # same input, same output
   * True
   *
   * @endcode
   */
  void mpsk_receiver_r_reset (mpsk_receiver_r_state_t *state);

  /**
   * @brief Push one real input sample; emit a symbol if it completed one.
   *
   * The composition API, identical in shape to mpsk_receiver_step_ted() —
   * only the front end and the input type differ.
   *
   * @param s      State. Must be non-NULL.
   * @param x      One real input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @param ted    RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL — pass a literal
   *               for a specialised (branch-free) instantiation.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  mpsk_receiver_r_step_ted (mpsk_receiver_r_state_t *s, float x,
                            float complex *y_out, int ted)
  {
    float complex ys[4];
    float complex zpre;
    int           n_pre = 0;
    size_t        n     = ddcr_execute_ctrl_push_tap2 (
        s->fe, x, s->l.timing.ctrl, s->l.freq_ctrl, ys,
        sizeof (ys) / sizeof (ys[0]), NULL, NULL, &zpre, &n_pre);
    /* The timing-independent NDA tap reads here, at the MFR's input. A no-op
       unless MF_IN is the configured tap. */
    if (n_pre)
      mpsk_rx_push_mf_in (&s->l, zpre);
    int           emitted = 0;
    for (size_t oi = 0; oi < n; oi++)
      emitted |= mpsk_rx_take_output (&s->l, ys[oi], y_out, ted);
    return emitted;
  }

  size_t mpsk_receiver_r_steps_max_out (mpsk_receiver_r_state_t *state);
  /**
   * @brief Demodulate a real f32 block and emit the recovered symbols.
   *
   * As mpsk_receiver_steps(), taking real samples: the R2C halfband makes them
   * complex before anything else touches them.
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
  size_t mpsk_receiver_r_steps (mpsk_receiver_r_state_t *state, const float *x,
                                size_t x_len, float complex *out,
                                size_t max_out);

  size_t mpsk_receiver_r_bits_max_out (mpsk_receiver_r_state_t *state);
  /**
   * @brief Demodulate a real f32 block and emit hard Gray-coded bits.
   *
   * As mpsk_receiver_bits(), taking real samples.
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
  size_t mpsk_receiver_r_bits (mpsk_receiver_r_state_t *state, const float *x,
                               size_t x_len, uint8_t *out, size_t max_out);

  /** @brief Tracked carrier, cycles/sample at the REAL input rate. */
  double mpsk_receiver_r_get_norm_freq (const mpsk_receiver_r_state_t *state);
  /** @brief Instantaneous NCO frequency command at the real input rate. */
  double mpsk_receiver_r_get_nco_freq (const mpsk_receiver_r_state_t *state);
  /** @brief Retune to @p val cycles/sample at the real input rate. */
  void   mpsk_receiver_r_set_norm_freq (mpsk_receiver_r_state_t *state,
                                        double                   val);
  double mpsk_receiver_r_get_lock (const mpsk_receiver_r_state_t *state);
  int    mpsk_receiver_r_get_locked (const mpsk_receiver_r_state_t *state);

  /**
   * @brief Symbols from reset to the FIRST carrier-lock declaration, or -1 if
   *        the receiver has not locked yet.
   *
   * The acquisition time, as a number a caller can read rather than infer by
   * polling `locked` in a loop. Dated by the same hysteretic detector
   * `mpsk_receiver_r_get_locked()` reports, so the two cannot disagree.
   *
   * In SYMBOLS, not seconds: `bn_carrier` and `bn_timing` are both normalised
   * to the symbol rate, so a settling budget quoted in symbols is comparable
   * across every input rate, and a caller with `Rs` divides once. Only the
   * first declaration is dated — a drop and re-acquire does not restamp it,
   * because the question this answers is "how long did this receiver take to
   * lock", not "when did it last hold". mpsk_receiver_r_reset() clears it to -1.
   */
  int64_t mpsk_receiver_r_get_lock_time (const mpsk_receiver_r_state_t *state);
  double mpsk_receiver_r_get_last_error (const mpsk_receiver_r_state_t *state);
  /**
   * @brief Re-tune the acquisition<->tracking handover detector directly.
   *
   * The real-input twin of mpsk_receiver_configure_lock(), whose contract it
   * shares exactly: a split declare/drop threshold pair on the carrier lock EMA
   * (level hysteresis) plus both verify counts (time hysteresis). A live
   * handover survives the re-tune; the in-flight verify run restarts.
   *
   * @param state        Must be non-NULL.
   * @param up_thresh    Declare threshold on the carrier lock EMA.
   * @param down_thresh  Drop threshold; choose <= up_thresh for level
   *                     hysteresis.
   * @param n_up         Consecutive above-threshold symbols to hand over to
   *                     the decision-directed discriminator; clamped >= 1.
   * @param n_down       Consecutive below-threshold symbols to fall back to
   *                     NDA acquisition; clamped >= 1.
   * @code
   * >>> from doppler.track import MpskReceiverR
   * >>> rx = MpskReceiverR(m=4, sps=10, m_out=2, acq_to_track=1)
   * >>> rx.tracking
   * 0
   * >>> rx.configure_lock(0.9, 0.72, 4, 16)   # tighter declare, fast drop
   *
   * @endcode
   */
  void mpsk_receiver_r_configure_lock (mpsk_receiver_r_state_t *state,
                                       double up_thresh, double down_thresh,
                                       uint32_t n_up, uint32_t n_down);
  /**
   * @brief Attach (or detach) a telemetry context across the receiver.
   *
   * Registers the same thirteen probes as mpsk_receiver_set_telemetry(), whose
   * contract it shares in full: the receiver's own "<prefix>.lock" and
   * "<prefix>.tracking", the carrier loop's "<prefix>.car.e" / ".freq" /
   * ".locked", and the symbol-timing loop's "<prefix>.sync.e" / ".ctrl" /
   * ".rate" / ".lock" / ".locked" / ".mu" — eleven emitted once per recovered
   * symbol — then the front end's AGC under "<prefix>.agc" (".gain_db" and
   * ".level_db"), forwarded through ddcr_set_telemetry(). All thinned by
   * @p decim. Passing NULL detaches everything. Setup path, never hot; the
   * context is borrowed and must outlive the attachment.
   *
   * @warning As on the complex twin, the two AGC probes are on the cascade's
   * MFR-input grid rather than the symbol grid, so their record count
   * differs from the other eleven — compare by time, not by index. See
   * mpsk_receiver_set_telemetry() for why, and for why that AGC is the
   * slowest loop in the receiver.
   *
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "rx".
   * @param decim  Emit every decim-th symbol (every decim-th gain update for
   *               the two AGC probes); >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take the
   *         probes (the attach fails whole; everything detached).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import MpskReceiverR
   * >>> from doppler.telemetry import Telemetry
   * >>> tlm = Telemetry(1 << 14)
   * >>> rx = MpskReceiverR(m=4, sps=10, m_out=2, init_norm_freq=0.25)
   * >>> rx.set_telemetry(tlm, "rx")
   * >>> len(tlm.probe_names)
   * 14
   * >>> rng = np.random.default_rng(7)
   * >>> idx = rng.integers(0, 4, 512)
   * >>> bb = np.repeat(np.exp(2j * np.pi * idx / 4), 10)
   * >>> n = np.arange(bb.size)
   * >>> x = (0.4 * bb * np.exp(2j * np.pi * 0.25 * n)).real
   * >>> x = np.ascontiguousarray(x.astype(np.float32))
   * >>> _ = rx.steps(x)
   * >>> recs = tlm.read()
   * >>> tlm.dropped            # size the ring, or the counts below diverge
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
  int mpsk_receiver_r_set_telemetry (mpsk_receiver_r_state_t *state,
                                     dp_tlm_t *tlm, const char *prefix,
                                     uint32_t decim);
  double mpsk_receiver_r_get_timing_rate (const mpsk_receiver_r_state_t *s);
  int    mpsk_receiver_r_get_tracking (const mpsk_receiver_r_state_t *state);
  int    mpsk_receiver_r_get_m (const mpsk_receiver_r_state_t *state);
  double mpsk_receiver_r_get_sps (const mpsk_receiver_r_state_t *state);
  size_t mpsk_receiver_r_get_m_out (const mpsk_receiver_r_state_t *state);
  /** @brief Has the cascade's CIC clipped its input since the last reset? */
  int mpsk_receiver_r_get_clipped (const mpsk_receiver_r_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: the front end's and the loops' self-validating child blobs,
 * exactly as the complex twin. */
#define MPSK_RECEIVER_R_STATE_MAGIC DP_FOURCC ('M', 'P', 'S', 'R')
#define MPSK_RECEIVER_R_STATE_VERSION 2u
  size_t mpsk_receiver_r_state_bytes (const mpsk_receiver_r_state_t *state);
  void   mpsk_receiver_r_get_state (const mpsk_receiver_r_state_t *state,
                                    void                          *blob);
  int    mpsk_receiver_r_set_state (mpsk_receiver_r_state_t *state,
                                    const void              *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RECEIVER_R_CORE_H */
