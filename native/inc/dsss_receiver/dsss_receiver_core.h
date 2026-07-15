/**
 * @file dsss_receiver_core.h
 * @brief Composed continuous DSSS receiver: Acquisition -> Dll(segments) ->
 *        RateConverter -> MpskReceiver, one object.
 *
 * The single-object form of the chain validated across this repo's
 * "continuous async-DSSS receiver" story
 * (`docs/gallery/dsss-acq-async-data.md`,
 * `docs/gallery/dsss-despread-async-data.md`,
 * `docs/gallery/async-dsss-receiver.md`): a continuous, non-bursty
 * spreading code whose data-symbol clock need not be synchronous to the
 * code-epoch clock. `steps()` streams raw samples through whichever
 * child is currently active:
 *
 *   - **searching** (`tracking() == 0`): samples feed the embedded
 *     `Acquisition`. Nothing is emitted. On a hit, `Dll`/`RateConverter`/
 *     `MpskReceiver` are built from the hit's code phase and Doppler
 *     estimate (the exact `dll_init_chip_from_acq` phase-inversion and
 *     `RateConverter`-bridged sample-rate hand-off this repo's gallery
 *     pages validated by hand), and the **unconsumed tail** of the same
 *     `steps()` call is handed straight to them — no samples are
 *     dropped at the transition.
 *   - **tracking** (`tracking() == 1`): samples feed
 *     `Dll -> RateConverter -> MpskReceiver` in sequence, exactly the
 *     C-level equivalent of `async_dsss_receiver_demo.py`'s `_receive()`
 *     helper, and demodulated symbols are emitted.
 *
 * Per `[[feedback_despread_resample_demod_separation]]` (this story's
 * own hard-won lesson): `segments` (the despreader's own tracking
 * parameter) and `sps` (the demodulator's own sample-rate need) are
 * independently configurable and bridged by an explicit
 * `RateConverter`, never coupled to each other.
 *
 * @code
 * // "Just works": only the signal's own physical parameters are required.
 * dsss_receiver_state_t *rx = dsss_receiver_create(
 *     code, code_len, 3.0e6, 2100.0,   // chip_rate, symbol_rate
 *     2, 2,                            // spc, m (BPSK)
 *     55.0, 1e-3, 0.9, 100.0,          // cn0_dbhz, pfa, pd,
 * doppler_uncertainty 16, 8,                           // reps, max_noncoh
 * (Acquisition's own
 *                                      // search-grid upper bounds)
 *     4, 8,                            // segments, sps
 *     0);                              // differential
 * float complex syms[4096];
 * size_t n = dsss_receiver_steps(rx, x, x_len, syms, 4096);
 * dsss_receiver_destroy(rx);
 * @endcode
 */
#ifndef DSSS_RECEIVER_CORE_H
#define DSSS_RECEIVER_CORE_H

#include "RateConverter/RateConverter_core.h"
#include "acq/acq_core.h"
#include "cic/cic_core.h"
#include "dll/dll_core.h"
#include "dp_state.h"
#include "hbdecim/hbdecim_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include "resamp/resamp_core.h"
#include "resample/resample_core.h"
#include <complex.h>
#include <stddef.h>

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
    acq_state_t           *acq;
    dll_state_t           *dll;
    RateConverter_state_t *rc;
    mpsk_receiver_state_t *rx;

    /* Own copy of the spreading code -- acq_create()/dll_create()'s own
     * borrow-vs-copy semantics aren't part of either's public contract,
     * so this object keeps a persistent copy rather than depend on being
     * able to read it back out of a child, or on the caller's original
     * buffer outliving construction. */
    uint8_t *code;
    size_t   code_len;

    /* Config carried across a dll/rc/rx rebuild (create-time or
     * configure_chain_raw()) — everything mpsk_receiver_create() and
     * dll_create() need that isn't re-derived from the acquisition hit. */
    size_t spc;
    int    m;
    int    differential;
    size_t segments; /**< Dll's own tracking parameter.                  */
    size_t sps;      /**< MpskReceiver's own samples/symbol.             */
    int    n;        /**< MpskReceiver's own carrier-arm count.          */
    double chip_rate;
    double symbol_rate;

    int      tracking; /**< 0 = searching, 1 = locked and demodulating.    */
    double   doppler_hz_est; /**< Cached from the winning acquisition hit. */
    double   cn0_dbhz_est;   /**< Cached from the winning acquisition hit. */
    uint64_t samples_fed;    /**< Running total handed to acq_push() so far
                                   — diffed against acq->samples_consumed
                                   right after a hit to find the exact
                                   unconsumed tail of the current call.    */
  } dsss_receiver_state_t;

  /**
   * @brief Create a DSSS receiver in the searching state.
   *
   * Only `code`/`chip_rate`/`symbol_rate` describe the signal itself —
   * everything else is a physically-motivated default a caller can
   * override, not a requirement. Internally: the embedded `Acquisition`
   * is sized by `symbol_rate` the same joint `(doppler_bins, n_noncoh)`
   * way `docs/guide/dsss-acquisition.md` recommends; `Dll` always uses
   * `bn=0.002` (this story's own validated stable loop bandwidth for a
   * one-update-per-code-epoch geometry, not `dll_create()`'s own default
   * of 0.01, which this story found unstable here) and `zeta=0.707`,
   * `spacing=0.5`; `MpskReceiver` always uses `pulse=iandd`,
   * `bn_carrier=bn_timing=0.01`, `zeta=0.707`, `acq_to_track=1`,
   * `lock_thresh=0.3`, `warmup_syms=30` — this story's own validated
   * values throughout. `n` (MpskReceiver's carrier-arm count) is derived
   * from `sps`: the largest divisor of `sps` in `{4, 2, 1}`.
   *
   * @param code                Spreading code (chip values).
   * @param code_len             Chips in `code` (the spreading factor).
   * @param chip_rate            Chip rate, Hz. Required.
   * @param symbol_rate          Data-symbol rate, Hz. Required — sizes
   *                             the embedded Acquisition's joint search
   *                             (see `acq_create()`'s own `symbol_rate`).
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
   * @param reps                 Acquisition's own coherent-depth upper
   *                             bound for its joint search; default 16.
   * @param max_noncoh           Acquisition's own non-coherent-look upper
   *                             bound for its joint search; default 8.
   * @param segments             Dll's own non-coherent partial-correlation
   *                             count per code epoch — its tracking-
   *                             robustness parameter, independent of
   *                             `sps` (see the module docstring); default
   *                             4, this story's own validated sweet spot.
   * @param sps                  MpskReceiver's samples/symbol, reached by
   *                             an internal RateConverter bridging the
   *                             despreader's own partial rate to this
   *                             rate; default 8, MpskReceiver's own
   *                             constructor default.
   * @param differential         MpskReceiver's differential (rotation-
   *                             invariant) demap; default 0 (coherent).
   * @return Heap-allocated state, or NULL on invalid args / allocation
   *         failure.
   */
  dsss_receiver_state_t *
  dsss_receiver_create (const uint8_t *code, size_t code_len, double chip_rate,
                        double symbol_rate, size_t spc, int m, double cn0_dbhz,
                        double pfa, double pd, double doppler_uncertainty,
                        size_t reps, size_t max_noncoh, size_t segments,
                        size_t sps, int differential);

  /** @brief Destroy a receiver and release all four children.
   *  @param state May be NULL. */
  void dsss_receiver_destroy (dsss_receiver_state_t *state);

  /**
   * @brief Return to the searching state.
   * Resets the embedded Acquisition and frees `dll`/`rc`/`rx` (rebuilt
   * from scratch on the next hit) — a receiver that has locked cannot
   * be "reset back to tracking the same signal," only back to searching,
   * matching every other object's reset() semantics in this codebase.
   * @param state Must be non-NULL.
   */
  void dsss_receiver_reset (dsss_receiver_state_t *state);

  size_t dsss_receiver_steps_max_out (dsss_receiver_state_t *state);

  /**
   * @brief Stream raw cf32 samples; emit demodulated symbols once locked.
   *
   * While searching, samples feed the embedded Acquisition and nothing
   * is emitted (0 return is normal, not an error). The moment a hit
   * fires, `Dll`/`RateConverter`/`MpskReceiver` are built and seeded
   * from it, and the unconsumed tail of THIS call — computed exactly
   * from `acq->samples_consumed`, no samples dropped or double-fed — is
   * handed straight to them in the same call. While tracking, samples
   * feed `Dll -> RateConverter -> MpskReceiver` in sequence. Accepts any
   * block size; state carries across calls (`Acquisition`/`Dll`/
   * `RateConverter`/`MpskReceiver` are all already block-size invariant,
   * so this object needs no ring-buffering of its own).
   *
   * @param state    Must be non-NULL.
   * @param x        Input cf32 samples.
   * @param x_len    Number of input samples.
   * @param out      Output symbols; caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of symbols written (0 while searching, or while
   *         tracking with not yet a full symbol's worth of input).
   */
  size_t dsss_receiver_steps (dsss_receiver_state_t *state,
                              const float complex *x, size_t x_len,
                              float complex *out, size_t max_out);

  /**
   * @brief Pin the embedded Acquisition's search grid directly.
   * Forwards to `acq_configure_search_raw()` — the escape hatch under
   * this object's own `symbol_rate`-driven auto-sizing, for a power user
   * who wants a specific `(doppler_bins, n_noncoh)` instead. Only
   * meaningful while searching (a no-op has already happened once
   * tracking has begun; the acquisition search doesn't run again until
   * the next `reset()`).
   * @return 0 on success, -1 on invalid grid (see acq_configure_search_raw).
   */
  int dsss_receiver_configure_search_raw (dsss_receiver_state_t *state,
                                          size_t                 doppler_bins,
                                          size_t                 n_noncoh);

  /**
   * @brief Re-tune the embedded Dll's code-lock detector directly.
   * Forwards to `dll_configure_lock_raw()`. Only meaningful once
   * tracking has begun (`dll` is NULL before then); a no-op while
   * searching.
   */
  void dsss_receiver_configure_lock_raw (dsss_receiver_state_t *state,
                                         double up_thresh, double down_thresh,
                                         size_t n_looks, double alpha,
                                         uint32_t n_up, uint32_t n_down);

  /**
   * @brief Pin the despread/resample/demod grid directly, bypassing the
   *        create-time `segments`/`sps` defaults.
   *
   * The escape hatch for the one composition-specific knob this object
   * adds beyond its children's own: `segments` (Dll's tracking
   * parameter) and `sps`/`n` (MpskReceiver's sample-rate/carrier-arm
   * parameters) are indepen­dently overridable here, still bridged by a
   * freshly-sized `RateConverter` — never coupled to each other (see the
   * module docstring). Rebuilds `dll`/`rc`/`rx` with every replacement
   * allocated first, only freeing and adopting the old ones once every
   * allocation has succeeded (mirrors `Acquisition`'s own `_regrid()`
   * discipline) — a failed pin leaves the receiver tracking on its prior
   * grid, not half-destroyed. Only meaningful once tracking (the grid
   * defaults still apply to create-time auto-sizing for the next hit
   * while searching; call `dsss_receiver_create()` with different
   * `segments`/`sps` for that, or re-pin here again after the next hit).
   *
   * @param n  MpskReceiver's carrier-arm count; must divide @p sps.
   * @return 0 on success, -1 on invalid grid or an allocation failure
   *         (the receiver is left usable at its prior grid on failure).
   */
  int dsss_receiver_configure_chain_raw (dsss_receiver_state_t *state,
                                         size_t segments, size_t sps, int n);

  int    dsss_receiver_get_tracking (const dsss_receiver_state_t *state);
  double dsss_receiver_get_doppler_hz (const dsss_receiver_state_t *state);
  double dsss_receiver_get_cn0_dbhz_est (const dsss_receiver_state_t *state);
  size_t dsss_receiver_get_segments (const dsss_receiver_state_t *state);
  size_t dsss_receiver_get_sps (const dsss_receiver_state_t *state);
  int    dsss_receiver_get_n (const dsss_receiver_state_t *state);
  /** @brief Dll's live tracked code phase (chips); 0.0 while searching. */
  double dsss_receiver_get_chip_phase (const dsss_receiver_state_t *state);
  /** @brief Dll's own tracking-quality indicator; 1.0 while searching. */
  double dsss_receiver_get_code_rate (const dsss_receiver_state_t *state);
  /** @brief MpskReceiver's carrier lock EMA; 0.0 while searching. */
  double dsss_receiver_get_lock (const dsss_receiver_state_t *state);
  /** @brief MpskReceiver's tracked carrier frequency; 0.0 while searching. */
  double dsss_receiver_get_norm_freq (const dsss_receiver_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────
   * Composition: acq + dll + rc + rx, always all four (a fixed shape --
   * see the state struct's own doc comment for why `tracking` doesn't
   * gate child presence here). `segments`/`sps`/`n` are the layout key:
   * set_state rejects a blob whose grid disagrees with the live engine's,
   * the same way ddc_extra_t's `rate` is checked before touching any
   * child. */

  typedef struct
  {
    uint8_t  tracking;
    uint8_t  _pad[7];
    double   doppler_hz_est;
    double   cn0_dbhz_est;
    uint64_t segments;
    uint64_t sps;
    uint64_t n;
  } dsss_receiver_extra_t;

#define DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('D', 'S', 'R', 'X')
#define DSSS_RECEIVER_STATE_VERSION 1u

  size_t dsss_receiver_state_bytes (const dsss_receiver_state_t *state);
  void   dsss_receiver_get_state (const dsss_receiver_state_t *state,
                                  void                        *blob);
  int dsss_receiver_set_state (dsss_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DSSS_RECEIVER_CORE_H */
