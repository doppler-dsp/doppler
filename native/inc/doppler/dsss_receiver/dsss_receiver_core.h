/**
 * @file dsss_receiver_core.h
 * @brief Composed continuous DSSS receiver: Acquisition -> Costas(bn_fll)
 *        pre-despread carrier wipeoff -> Dll(segments) -> RateConverter ->
 *        MpskReceiver, one object.
 *
 * The single-object form of the chain validated across this repo's
 * "continuous async-DSSS receiver" story
 * (`docs/gallery/async-dsss-receiver-spec.md`,
 * `docs/gallery/dsss-acq-characterization.md`,
 * `docs/gallery/dsss-receiver.md`): a continuous, non-bursty
 * spreading code whose data-symbol clock need not be synchronous to the
 * code-epoch clock. `steps()` streams raw samples through whichever
 * child is currently active:
 *
 *   - **searching** (`tracking() == 0`): samples feed the embedded
 *     `Acquisition`. Nothing is emitted. On a hit, the carrier loop/
 *     `Dll`/`RateConverter`/`MpskReceiver` are built from the hit's code
 *     phase and Doppler estimate (the exact `acq_result_t::chip_phase`
 *     hand-off phase and `RateConverter`-bridged sample-rate hand-off
 *     this repo's gallery pages validated by hand), and the **unconsumed
 *     tail** of the same `steps()` call is handed straight to them — no
 *     samples are dropped at the transition.
 *   - **tracking** (`tracking() == 1`): samples are first derotated by a
 *     pre-despread carrier loop (`costas_wipeoff`/`costas_update`, one
 *     update per code period, `bn_fll`-assisted -- removes BULK Doppler
 *     and its RATE OF CHANGE before the code loop ever sees it; a fixed/
 *     bounded residual alone is fine downstream-only, per
 *     `docs/design/async-dsss-receiver.md` §3.4, but an unbounded
 *     Doppler RATE is not), one code
 *     period at a time (a small internal carry buffer holds any leftover
 *     partial-period tail across calls -- `steps()` still accepts any
 *     block size), then feed `Dll -> RateConverter -> MpskReceiver` in
 *     sequence -- the C-level equivalent of hand-composing those four
 *     objects (plus the new carrier stage) -- and demodulated symbols are
 *     emitted. This is a NEW
 *     composition living entirely in this object -- deliberately NOT a
 *     swap to the existing `Despreader` object (which fuses Costas+Dll
 *     per-sample), because `Despreader` embeds `Dll` via `dp_dll_init()`,
 *     hardcoded to `segments==1`; it cannot carry this object's own
 *     `segments>1` async-lookback tracking. `dp_dll_steps()` itself is
 *     called completely unmodified.
 *
 * Per `[[feedback_despread_resample_demod_separation]]` (this story's
 * own hard-won lesson): `segments` (the despreader's own tracking
 * parameter) and `sps` (the demodulator's own sample-rate need) are
 * independently configurable and bridged by an explicit
 * `RateConverter`, never coupled to each other.
 *
 * @code
 * // "Just works": only the signal's own physical parameters are required.
 * dp_dsss_receiver_state_t *rx = dp_dsss_receiver_create(
 *     code, code_len, 3.0e6, 2100.0,   // chip_rate, symbol_rate
 *     2, 2,                            // spc, m (BPSK)
 *     55.0, 1e-3, 0.9, 100.0,          // cn0_dbhz, pfa, pd,
 *                                      // doppler_uncertainty
 *     4, 8,                            // segments, sps
 *     0);                              // differential
 * float _Complex syms[4096];
 * size_t n = dp_dsss_receiver_steps(rx, x, x_len, syms, 4096);
 * dp_dsss_receiver_destroy(rx);
 * @endcode
 */
#ifndef DP_DSSS_RECEIVER_CORE_H
#define DP_DSSS_RECEIVER_CORE_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/acq/acq_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/dp_state.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/mpsk_receiver/mpsk_receiver_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/dp_complex.h"
#include <stddef.h>
#include "doppler/costas/costas_core.h"
#include "doppler/snr/snr_core.h"
#include "doppler/ber/ber_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Composed receiver state.
   *
   * Owns all four children for the object's entire lifetime -- `dll`/
   * `rc`/`rx` are allocated at create() with a placeholder seed (phase
   * 0, no Doppler) and REBUILT (not freed to NULL) the moment a real
   * hit fires, on every configure_chain_raw(), and back to the
   * placeholder seed on reset(). This keeps every child's pointer
   * always valid and the serialized blob's layout independent of
   * `tracking` -- a fixed shape, not a conditionally-present one.
   * `tracking` is purely a routing flag for steps() (search vs. the
   * despread/resample/demod chain), not a lifecycle state. Treat all
   * fields as internal (use the getters).
   */
  typedef struct
  {
    dp_acq_state_t           *acq; /**< Owned continuous acquisition engine;
                                      fed while `tracking` is 0. */
    dp_dll_state_t           *dll; /**< Owned code loop (despreader); always
                                      allocated, rebuilt on each hit,
                                      reset() and configure_chain_raw(). */
    dp_RateConverter_state_t *rc;  /**< Owned resampler from the Dll's
                                      partial rate to sps*symbol_rate;
                                      rebuilt with `dll`. */
    dp_mpsk_receiver_state_t *rx;  /**< Owned demodulator at `sps`
                                      samples/symbol; rebuilt with `dll`. */

    /** Pre-despread carrier loop -- embedded by value, same pattern
     * dp_despreader_state_t's own `car` uses. Rebuilt alongside dll/rc/rx
     * (create-time placeholder, every real hit, configure_chain_raw(),
     * reset()). One update per code period (`tsamps` samples). */
    dp_costas_state_t car;
    size_t         tsamps; /**< code_len*spc -- one code period, samples;
                                 the carrier loop's own fixed chunk size. */
    /** Scratch, not state: sized `tsamps`, allocated once (never per
     * steps() call -- no allocation in the hot streaming path). */
    float _Complex *car_wiped_buf;
    /** Genuinely stateful: raw samples handed to steps() that didn't fill
     * a whole code period yet, carried to the NEXT call so the carrier
     * loop always wipes exact, complete periods regardless of how a
     * caller chunks its input (this object's own "any block size, state
     * carries across calls" contract). Capacity `tsamps`; only the first
     * `car_carry_len` samples are valid. Must be serialized (see below)
     * or a resumed instance silently loses its period alignment. */
    float _Complex *car_carry_buf;
    size_t         car_carry_len; /**< Valid samples in `car_carry_buf`,
                                       0..tsamps-1; running state. */
    /** Scratch, not state: the tracking chain's Dll output, grown on
     * demand to the largest block seen and kept, so a tracking steps()
     * allocates nothing once warm (doppler#2650). Freed in destroy(). */
    float _Complex *trk_dll_buf;
    size_t         trk_dll_cap; /**< Capacity of `trk_dll_buf`, samples. */
    /** Scratch, not state: the tracking chain's RateConverter output,
     * grown on demand and kept like `trk_dll_buf`. */
    float _Complex *trk_rc_buf;
    size_t         trk_rc_cap; /**< Capacity of `trk_rc_buf`, samples. */

    /** Own copy of the spreading code -- dp_acq_create_continuous()/
     * dp_dll_create()'s own borrow-vs-copy semantics aren't part of either's
     * public contract,
     * so this object keeps a persistent copy rather than depend on being
     * able to read it back out of a child, or on the caller's original
     * buffer outliving construction. */
    uint8_t *code;
    size_t   code_len; /**< Chips in `code` (spreading factor); config. */

    /* Config carried across a dll/rc/rx rebuild (create-time or
     * configure_chain_raw()) — everything dp_mpsk_receiver_create() and
     * dp_dll_create() need that isn't re-derived from the acquisition hit. */
    size_t spc;          /**< Samples per chip at the input; config. */
    int    m;            /**< PSK order (2, 4 or 8); config. */
    int    differential; /**< Nonzero = differential demap; config. */
    size_t segments; /**< Dll's own tracking parameter.                  */
    size_t sps;      /**< MpskReceiver's own samples/symbol.             */
    int    n;        /**< MpskReceiver's terminal outputs per symbol
                              (`m_out`).                                  */
    double chip_rate;   /**< Chip rate, Hz; config. */
    double symbol_rate; /**< Data-symbol rate, Hz; config. */

    int      tracking; /**< 0 = searching, 1 = locked and demodulating.    */
    double   doppler_hz_est; /**< Cached from the winning acquisition hit. */
    double   cn0_dbhz_est;   /**< Cached from the winning acquisition hit. */
  } dp_dsss_receiver_state_t;

  /* the specification-derived defaults for the pre-despread carrier loop (bn<=0.01 (docs/design/async-dsss-receiver.md)
   * rule + this session's own validated test_costas_core.c/floor-sweep
   * bn_fll=0.03 calibration point) -- hardcoded, not public constructor
   * params, matching how Dll's own bn=0.002/zeta/spacing are already
   * hardcoded (this object's "just works" philosophy: only the signal's
   * physical parameters are required). */
  /** @brief Pre-despread carrier loop's PLL noise bandwidth, normalized to
   *  its once-per-code-period update rate. */
#define DSSS_RX_BN_CARRIER 0.01
  /** @brief Pre-despread carrier loop's FLL-assist bandwidth, normalized to
   *  its once-per-code-period update rate. */
#define DSSS_RX_BN_FLL 0.03

  /**
   * @brief Create a DSSS receiver in the searching state.
   *
   * Only `code`/`chip_rate`/`symbol_rate` describe the signal itself —
   * everything else is a physically-motivated default a caller can
   * override, not a requirement. Internally: the embedded `Acquisition`
   * is built via `dp_acq_create_continuous()` (this receiver is inherently
   * continuous/streaming) -- always window-tiles, never coherently
   * combines across epochs, sensitivity purely from an internally
   * auto-sized non-coherent look count (see `acq_core.h`'s file doc
   * comment); `Dll` always uses `bn=0.002` (this story's own validated
   * stable loop bandwidth for a one-update-per-code-epoch geometry, not
   * `dp_dll_create()`'s own default of 0.01, which this story found unstable
   * here) and `zeta=0.707`, `spacing=0.5`; `MpskReceiver` always uses
   * `pulse=iandd`, `bn_carrier=bn_timing=0.01`, `zeta=0.707` and
   * `lock_thresh=0.3` — this story's own validated values throughout. It
   * also passed `acq_to_track=1` and `warmup_syms=30` until those were
   * deleted (doppler#877, `1f417e97`); the composed receiver now runs its
   * one NDA discriminator here as everywhere else. `lock_thresh=0.3` predates the
   * lock statistic becoming a calibrated detector and is retained because it
   * is validated here, but it now has a derivable reading: the carrier lock
   * EMA's noise-only sd is 0.1132 at every M, so 0.3 is **2.65 noise sigmas**,
   * a per-look Pfa of ~4e-3 — looser than `MpskReceiver`'s own 0.5 default
   * (4.42 sigma, 5e-6) and still ~6 sigma clear of the +0.99 a locked BPSK
   * constellation reads, which is why it holds. See `carrier_nda_core.h`.
   * `n` (MpskReceiver's terminal outputs per symbol, `m_out`) is derived
   * from `sps` by mpsk_rx_derive_m_out(): the even count
   * `2*floor(sps/2)`, capped at 8.
   *
   * @param code                Spreading code, one 0/1 chip per element
   *                            (0 -> +1, 1 -> -1 BPSK; only the low bit is
   *                            used, so pass 0/1, not +/-1).
   * @param code_len             Chips in `code` (the spreading factor).
   * @param chip_rate            Chip rate, Hz. Required.
   * @param symbol_rate          Data-symbol rate, Hz. Required — passed
   *                             straight to the embedded Acquisition's
   *                             own `symbol_rate` (diagnostic there; see
   *                             `dp_acq_create_continuous()`).
   * @param spc                  Samples/chip (front-end oversample);
   *                             default 2 (fs = 2x chip_rate).
   * @param m                    PSK order, 2/4/8; default 2 (BPSK).
   * @param cn0_dbhz             Design C/N0 for acquisition sizing, dB-Hz;
   *                             default 55.0.
   * @param pfa                  Acquisition false-alarm target; default
   *                             1e-3.
   * @param pd                   Acquisition detection-probability target;
   *                             default 0.9.
   * @param doppler_uncertainty  One-sided Doppler search half-range, Hz;
   *                             default 100.0.
   * @param segments             Dll's partial correlations per code epoch:
   *                             its despread output rate and the grain of
   *                             its look-back window, independent of
   *                             `sps` (see the module docstring); default
   *                             4.
   * @param sps                  MpskReceiver's samples/symbol, reached by
   *                             an internal RateConverter bridging the
   *                             despreader's own partial rate to this
   *                             rate; default 8, MpskReceiver's own
   *                             constructor default.
   * @param differential         MpskReceiver's differential (rotation-
   *                             invariant) demap; default 0 (coherent).
   * @return A new receiver in the searching state, owned by the caller
   *         (free with dp_dsss_receiver_destroy()); NULL when @p code is
   *         NULL, a parameter is outside its domain (@p m not 2/4/8,
   *         @p spc < 1, @p sps < 2, a non-positive or non-finite rate, a
   *         symbol rate above the chip rate, a code/segments pair the Dll
   *         refuses), or the embedded Acquisition refuses its own
   *         parameters (@p pfa, @p pd, @p cn0_dbhz, ...). An allocation
   *         failure aborts the process rather than returning NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import DsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> sf, chip, sym, spc = 1023, 3.0e6, 2100.0, 2
   * >>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
   * >>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
   * >>> csign = np.where(code & 1, -1.0, 1.0)
   * >>> rng = np.random.default_rng(6)
   * >>> n = int(400 * tsym) + 2 * te            # 400 BPSK data symbols
   * >>> idx = np.arange(n)
   * >>> data = (rng.integers(0, 2, 404) * 2 - 1).astype(float)
   * >>> si = np.clip((idx / tsym).astype(int), 0, 403)
   * >>> spread = data[si] * csign[(idx // spc) % sf]        # DSSS chips
   * >>> sig = spread * np.exp(2j * np.pi * (50.0 / fs) * idx)  # +50 Hz
   * >>> pre = 3 * te                     # noise-only lead-in, pre-signal
   * >>> sigma = np.sqrt(fs / 10 ** (90.0 / 10))            # ~90 dB-Hz C/N0
   * >>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
   * ...          + 1j * rng.standard_normal(pre + n))
   * >>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
   * ...      + noise.astype(np.complex64))
   * >>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, spc=spc,
   * ...                   cn0_dbhz=55.0, doppler_uncertainty=100.0)
   * >>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
   * >>> syms = np.concatenate([s for s in syms if len(s)])
   * >>> rx.tracking                  # acquired, now demodulating
   * 1
   * >>> len(syms) > 300              # a few hundred symbols recovered
   * True
   *
   * Nearly all the energy lands on I, so the BPSK phase is resolved:
   *
   * >>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
   * True
   *
   * @endcode
   */
  dp_dsss_receiver_state_t *
  dp_dsss_receiver_create (const uint8_t *code, size_t code_len, double chip_rate,
                        double symbol_rate, size_t spc, int m, double cn0_dbhz,
                        double pfa, double pd, double doppler_uncertainty,
                        size_t segments, size_t sps, int differential);

  /** @brief Destroy a receiver and release all four children.
   *  @param state May be NULL. */
  void dp_dsss_receiver_destroy (dp_dsss_receiver_state_t *state);

  /**
   * @brief Return to the searching state.
   * Resets the embedded Acquisition and rebuilds `dll`/`rc`/`rx` and the
   * carrier loop at the placeholder seed (phase 0, no Doppler) on the
   * current grid (a configure_chain_raw() pin survives), clearing the
   * carrier carry and the cached Doppler/C/N0 estimates. Any
   * configure_lock_raw() tuning is lost with the old Dll; a grid pinned by
   * configure_search_raw() is kept. A receiver that has locked cannot
   * be "reset back to tracking the same signal," only back to searching,
   * matching every other object's reset() semantics in this codebase.
   * @param state Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import DsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
   * >>> rx.reset()                 # abort any lock, hunt from scratch
   * >>> (rx.tracking, rx.chip_phase)   # back to searching, all cleared
   * (0, 0.0)
   *
   * @endcode
   */
  void dp_dsss_receiver_reset (dp_dsss_receiver_state_t *state);

  /** @brief Output capacity hint for dp_dsss_receiver_steps().
   *  @param state  Unused.
   *  @return Always 0: size @p out from the input length @p x_len, which
   *          is always enough (at most one symbol per input sample). */
  size_t dp_dsss_receiver_steps_max_out (dp_dsss_receiver_state_t *state);

  /**
   * @brief Stream raw cf32 samples; emit demodulated symbols once locked.
   *
   * While searching, samples feed the embedded Acquisition and nothing
   * is emitted (0 return is normal, not an error). The moment a hit fires,
   * `Dll`/`RateConverter`/`MpskReceiver` are built and seeded from it, and the
   * rest of THIS call from where the hit's frame ended — the hit's own
   * `samples_consumed`, so no samples are dropped or double-fed — is handed
   * straight to them in the same call. While tracking, samples feed `Dll ->
   * RateConverter -> MpskReceiver` in sequence. Accepts any block size; state
   * carries across calls (`Acquisition`/`Dll`/ `RateConverter`/`MpskReceiver`
   * are all already block-size invariant; the only buffering this object
   * adds is the carrier loop's partial-code-period carry). While tracking,
   * each call allocates two scratch buffers (abort on OOM).
   *
   * @param state    Must be non-NULL.
   * @param x        Input cf32 samples.
   * @param x_len    Number of input samples.
   * @param out      Output symbols; caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of symbols written (0 while searching, or while
   *         tracking with not yet a full symbol's worth of input).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import DsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> sf, chip, sym, spc = 1023, 3.0e6, 2100.0, 2
   * >>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
   * >>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
   * >>> csign = np.where(code & 1, -1.0, 1.0)
   * >>> rng = np.random.default_rng(6)
   * >>> n = int(400 * tsym) + 2 * te            # 400 BPSK data symbols
   * >>> idx = np.arange(n)
   * >>> data = (rng.integers(0, 2, 404) * 2 - 1).astype(float)
   * >>> si = np.clip((idx / tsym).astype(int), 0, 403)
   * >>> spread = data[si] * csign[(idx // spc) % sf]        # DSSS chips
   * >>> sig = spread * np.exp(2j * np.pi * (50.0 / fs) * idx)  # +50 Hz
   * >>> pre = 3 * te                     # noise-only lead-in, pre-signal
   * >>> sigma = np.sqrt(fs / 10 ** (90.0 / 10))            # ~90 dB-Hz C/N0
   * >>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
   * ...          + 1j * rng.standard_normal(pre + n))
   * >>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
   * ...      + noise.astype(np.complex64))
   * >>> rx = DsssReceiver(code, chip_rate=chip, symbol_rate=sym, spc=spc,
   * ...                   cn0_dbhz=55.0, doppler_uncertainty=100.0)
   * >>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
   * >>> syms = np.concatenate([s for s in syms if len(s)])
   * >>> rx.tracking                  # acquired and now demodulating
   * 1
   * >>> len(syms) > 300              # a few hundred symbols recovered
   * True
   *
   * Nearly all the energy lands on I, so the BPSK phase is resolved:
   *
   * >>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
   * True
   *
   * @endcode
   */
  size_t dp_dsss_receiver_steps (dp_dsss_receiver_state_t *state,
                              const float _Complex *x, size_t x_len,
                              float _Complex *out, size_t max_out);

  /**
   * @brief Pin the embedded Acquisition's search grid directly.
   * Forwards to `dp_acq_configure_search_raw()` — the escape hatch under
   * this object's own `symbol_rate`-driven auto-sizing, for a power user
   * who wants a specific `(doppler_bins, n_noncoh)` instead. Pinning
   * exits the continuous engine's wideband window-tiling mode, re-derives
   * its thresholds and resets it, discarding any search in flight. While
   * tracking the engine is idle, so the pinned grid governs the search
   * that resumes after the next `reset()`.
   *
   * @param state         Must be non-NULL.
   * @param doppler_bins  Coherent depth to pin, in `[1, reps]` as
   *                      dp_acq_configure_search_raw() defines it.
   * @param n_noncoh      Non-coherent looks accumulated per grid cell
   *                      (1..256); more looks buys sensitivity at the cost
   *                      of dwell, replacing the auto-sized count.
   * @return 0 on success, -1 on an invalid grid (including
   *         @p n_noncoh > 1 on an engine with rate hypotheses) or a failed
   *         allocation (the engine keeps its prior grid; see
   *         dp_acq_configure_search_raw()).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import DsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
   * >>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it
   * >>> rx.tracking                # still searching, on the pinned grid
   * 0
   *
   * @endcode
   */
  int dp_dsss_receiver_configure_search_raw (dp_dsss_receiver_state_t *state,
                                          size_t                 doppler_bins,
                                          size_t                 n_noncoh);

  /**
   * @brief Re-tune the embedded Dll's code-lock detector directly.
   * Forwards to `dp_dll_configure_lock_raw()`, which also clears the
   * in-flight lock statistic and drops lock. Only meaningful once tracking
   * has begun: while searching it tunes the placeholder Dll, which the
   * next hit replaces, so the setting is lost (as it is on reset() and
   * configure_chain_raw()). The detector is the hysteretic lockdet over
   * the DLL's per-N-look CFAR statistic — @p up_thresh / @p down_thresh set
   * the declare/drop levels and @p n_up / @p n_down the consecutive-look
   * verify counts, trading declare latency against false-alarm rate.
   *
   * @param state        Must be non-NULL.
   * @param up_thresh    CFAR-statistic level to declare code lock (hit
   *                     when the statistic exceeds it).
   * @param down_thresh  Level below which a look is a miss; choose
   *                     <= @p up_thresh for level hysteresis.
   * @param n_looks      Looks per decision — the DLL's non-coherent
   *                     integration depth feeding one statistic.
   * @param alpha        EMA smoothing coefficient on the lock statistic
   *                     (0..1); smaller is smoother/slower.
   * @param n_up         Consecutive hits required to declare lock.
   * @param n_down       Consecutive misses required to drop lock.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import DsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
   * >>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,
   * ...                       alpha=0.1, n_up=5, n_down=3)
   * >>> rx.tracking                # a no-op until a hit builds the Dll
   * 0
   *
   * @endcode
   */
  void dp_dsss_receiver_configure_lock_raw (dp_dsss_receiver_state_t *state,
                                         double up_thresh, double down_thresh,
                                         size_t n_looks, double alpha,
                                         uint32_t n_up, uint32_t n_down);

  /**
   * @brief Pin the despread/resample/demod grid directly, bypassing the
   *        create-time `segments`/`sps` defaults.
   *
   * The escape hatch for the one composition-specific knob this object
   * adds beyond its children's own: `segments` (Dll's tracking
   * parameter) and `sps`/`n` (MpskReceiver's samples and terminal outputs
   * per symbol) are independently overridable here, still bridged by a
   * freshly-sized `RateConverter` -- never coupled to each other (see the
   * module docstring). It rebuilds `dll`/`rc`/`rx` at once, allocating
   * every replacement before freeing the old ones (`Acquisition`'s
   * `acq_regrid()` discipline), and the pinned grid is the one every later
   * chain is built on: the next hit's hand-off and reset() both rebuild at
   * it.
   *
   * The new grid is checked before anything is rebuilt, and refused with
   * the receiver left on its prior grid: @p segments >= 1, @p sps >= 2, a
   * symbol no faster than a chip and a resampling ratio that is a normal
   * double (doppler#2103), and an @p n MpskReceiver would accept
   * (doppler#2426). Still an abort: an allocation failure in the rebuild,
   * which runs under dp_xnn (doppler#2112).
   *
   * @param state     The receiver.
   * @param segments  Dll tracking segments per code period, >= 1.
   * @param sps       MpskReceiver samples per symbol (the resample target),
   *                  >= 2.
   * @param n         MpskReceiver's terminal outputs per symbol (`m_out`):
   *                  inside dp_ratesync_m_ok() (even, 2..8) and at most
   *                  @p sps -- mpsk_rx_m_out_ok(), the rule create()'s own
   *                  derived `n` satisfies, so get_n() always re-applies
   *                  (doppler#2650). It need not divide @p sps.
   * @return 0 on success, -1 on a grid refused as above (the receiver keeps
   *         its prior grid).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import DsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = DsssReceiver(code, chip_rate=3.0e6, symbol_rate=2100.0, spc=2)
   * >>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain
   * >>> rx.segments                       # tracking grid updated in place
   * 6
   *
   * @endcode
   */
  int dp_dsss_receiver_configure_chain_raw (dp_dsss_receiver_state_t *state,
                                         size_t segments, size_t sps, int n);

  /** @brief Routing state: searching or tracking.
   *  @param state  Must be non-NULL.
   *  @return 0 while searching (after create/reset), 1 once a hit has
   *          built the chain and symbols are being demodulated. */
  int    dp_dsss_receiver_get_tracking (const dp_dsss_receiver_state_t *state);
  /** @brief Doppler estimate from the winning acquisition hit.
   *  @param state  Must be non-NULL.
   *  @return Hz; 0.0 before the first hit and after reset(). Not updated
   *          while tracking. */
  double dp_dsss_receiver_get_doppler_hz (const dp_dsss_receiver_state_t *state);
  /** @brief C/N0 estimate from the winning acquisition hit.
   *  @param state  Must be non-NULL.
   *  @return dB-Hz; 0.0 before the first hit and after reset(). */
  double dp_dsss_receiver_get_cn0_dbhz_est (const dp_dsss_receiver_state_t *state);
  /** @brief Dll partial correlations per code period currently in use.
   *  @param state  Must be non-NULL.
   *  @return The create-time value, or the last configure_chain_raw() pin.
   */
  size_t dp_dsss_receiver_get_segments (const dp_dsss_receiver_state_t *state);
  /** @brief MpskReceiver samples per symbol currently in use.
   *  @param state  Must be non-NULL.
   *  @return The create-time value, or the last configure_chain_raw() pin.
   */
  size_t dp_dsss_receiver_get_sps (const dp_dsss_receiver_state_t *state);
  /** @brief MpskReceiver terminal outputs per symbol (`m_out`) in use.
   *  @param state  Must be non-NULL.
   *  @return `2*floor(sps/2)` capped at 8 from create, or the last
   *          configure_chain_raw() pin. */
  int    dp_dsss_receiver_get_n (const dp_dsss_receiver_state_t *state);
  /** @brief Dll's live tracked code phase (chips); 0.0 while searching.
   *  @param state  Must be non-NULL.
   *  @return Code phase in chips, from dp_dll_get_code_phase(). */
  double dp_dsss_receiver_get_chip_phase (const dp_dsss_receiver_state_t *state);
  /** @brief Dll's tracked code rate; 1.0 while searching.
   *  @param state  Must be non-NULL.
   *  @return Chips advanced per nominal chip (~1.0 at lock), from
   *          dp_dll_get_code_rate(). */
  double dp_dsss_receiver_get_code_rate (const dp_dsss_receiver_state_t *state);
  /** @brief MpskReceiver's carrier lock EMA; 0.0 while searching.
   *  @param state  Must be non-NULL.
   *  @return The NDA lock statistic, near 1 when locked and near 0 on
   *          noise, from dp_mpsk_receiver_get_lock(). */
  double dp_dsss_receiver_get_lock (const dp_dsss_receiver_state_t *state);
  /** @brief MpskReceiver's tracked carrier frequency; 0.0 while searching.
   *  @param state  Must be non-NULL.
   *  @return Cycles/sample at the demodulator's rate (sps * symbol_rate):
   *          the residual left after the pre-despread carrier loop, not
   *          the full Doppler. */
  double dp_dsss_receiver_get_norm_freq (const dp_dsss_receiver_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────
   * Composition: acq + car + dll + rc + rx, always all five (a fixed
   * shape -- see the state struct's own doc comment for why `tracking`
   * doesn't gate child presence here). `segments`/`sps`/`n` are the
   * layout key: set_state rejects a blob whose grid disagrees with the
   * live engine's, the same way ddc_extra_t's `rate` is checked before
   * touching any child. `car_carry_len` counts the live samples of the
   * carrier carry, 0..tsamps-1 (steps processes a full period at once, so
   * set_state refuses tsamps) -- losing it on resume would desync the
   * carrier loop's period alignment. The carry region itself is packed
   * LAST, at its fixed tsamps size: the live samples, then zeros. */

  /** @brief Receiver-level payload that follows the state envelope: the
   *  routing flag, cached hit estimates, the grid (layout key) and the
   *  carrier-carry depth. */
  typedef struct
  {
    uint8_t  tracking;       /**< 0 searching, 1 tracking. */
    uint8_t  _pad[7];        /**< Explicit padding to 8-byte alignment. */
    double   doppler_hz_est; /**< Cached hit Doppler estimate, Hz. */
    double   cn0_dbhz_est;   /**< Cached hit C/N0 estimate, dB-Hz. */
    uint64_t segments;       /**< Layout key; must match the live engine. */
    uint64_t sps;            /**< Layout key; must match the live engine. */
    uint64_t n;              /**< Layout key; must match the live engine. */
    uint64_t car_carry_len;  /**< Valid carry samples, < tsamps. */
  } dsss_receiver_extra_t;

  /** @brief Magic tag ("DSRX") stamped in a receiver state blob. */
#define DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('D', 'S', 'R', 'X')
  /** @brief Layout version of the receiver state blob. */
#define DSSS_RECEIVER_STATE_VERSION 3u /* v3: the dll child grew (output
                                           level, #2495); v2: pre-despread
                                           dp_costas_state_t car + carry
                                           buffer added */

  /**
   * @brief Byte size of @p state's blob.
   * @param state  Must be non-NULL.
   * @return Envelope + extra + the five children + a tsamps-sample carry
   *         region; fixed for a given grid, independent of `tracking`.
   */
  size_t dp_dsss_receiver_state_bytes (const dp_dsss_receiver_state_t *state);
  /**
   * @brief Serialize the whole receiver (searching or tracking) into
   *        @p blob.
   * @param state  Must be non-NULL; not modified.
   * @param blob   Caller-owned buffer of dp_dsss_receiver_state_bytes()
   *               bytes.
   */
  void   dp_dsss_receiver_get_state (const dp_dsss_receiver_state_t *state,
                                  void                        *blob);
  /**
   * @brief Restore a blob from dp_dsss_receiver_get_state() into @p state.
   *
   * A searching engine may restore a tracking blob and vice versa.
   * Children are restored in order (acq, carrier, dll, rc, rx); a child
   * that rejects aborts with DP_ERR_INVALID after the earlier children
   * were already restored.
   *
   * @param state  Must be non-NULL, built with the same code/spc and set to
   *               the same `segments`/`sps`/`n` grid as the blob's source.
   * @param blob   Blob of dp_dsss_receiver_state_bytes(state) bytes.
   * @return DP_OK, or DP_ERR_INVALID on an envelope mismatch, a grid that
   *         differs from the live engine's (pin it with
   *         configure_chain_raw() first), a carry length >= tsamps, or a
   *         rejected child.
   */
  int dp_dsss_receiver_set_state (dp_dsss_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DSSS_RECEIVER_CORE_H */
