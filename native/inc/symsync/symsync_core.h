/**
 * @file symsync_core.h
 * @brief SymbolSync component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * symsync_state_t *obj = symsync_create(4, 0.01, 0.707, 0, 0);
 * float complex y = symsync_step(obj, 0.0f + 0.0f * I);
 * symsync_destroy(obj);
 * @endcode
 */
#ifndef SYMSYNC_CORE_H
#define SYMSYNC_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "farrow/farrow_core.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "nco/nco_core.h"
#include "telemetry/telemetry.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Timing-error-detector selection for symsync_state_t::ted. */
  enum
  {
    SYMSYNC_TED_GARDNER = 0, /**< blind Gardner TED (mid * conj diff).    */
    SYMSYNC_TED_DTTL    = 1  /**< decision-directed sign-sign DTTL.       */
  };

/* Numerical guard on the on-time+mid-symbol energy sum feeding the lock
 * statistic (not tunable). */
#define SYMSYNC_LOCK_EPS 1e-12

  /**
   * @brief Telemetry attachment: a borrowed context + this object's probe
   *        ids.  NULL ctx (the default) means detached — every probe site
   *        is then a single predicted-not-taken branch per recovered
   *        symbol.  Zeroed in state blobs and preserved across set_state
   *        (DP_DEFINE_POD_STATE_TLM).
   */
  typedef struct
  {
    dp_tlm_t *ctx;       /**< NULL = detached                     */
    int32_t   id_e;      /**< "<prefix>.e"      — TED error       */
    int32_t   id_freq;   /**< "<prefix>.freq"   — loop control    */
    int32_t   id_rate;   /**< "<prefix>.rate"   — rate estimate   */
    int32_t   id_lock;   /**< "<prefix>.lock"   — lock_signal mean */
    int32_t   id_locked; /**< "<prefix>.locked" — lockdet flag    */
  } symsync_tlm_t;

  /**
   * @brief SymbolSync state.
   *
   * Allocate with symsync_create().  Embeds the integer timing NCO, the Farrow
   * interpolator and the PI loop filter by value; treat the TED history as
   * internal.
   */
  typedef struct
  {
    nco_state_t         timing; /**< integer timing NCO (phase/phase_inc).  */
    farrow_state_t      farrow; /**< fractional interpolator.               */
    loop_filter_state_t lf;     /**< 2nd-order timing PI loop.              */
    size_t              sps;    /**< nominal samples per symbol.            */
    uint32_t      base_inc;     /**< nominal NCO inc (one wrap / symbol).   */
    int           ted;          /**< SYMSYNC_TED_GARDNER / _DTTL.           */
    int           have_ontime;  /**< a previous on-time sample exists.      */
    float complex prev_ontime;  /**< previous on-time interpolant.          */
    float complex mid;          /**< mid-symbol (transition-gate) sample.   */
    double        bn;           /**< loop noise bandwidth (retained).       */
    double        zeta;         /**< damping factor (retained).             */
    double        last_error;   /**< last TED timing error.                 */
    double        rate_est;     /**< smoothed tracked samples/symbol.       */
    double        pwr_avg;      /**< running symbol power (TED normaliser).  */
    /* ── lock detector (always on): tumbling-window block average ────── */
    double lock_sum;      /**< running sum of lock_signal over the current
                                avgs-symbol block (mirrors dll_state_t's
                                lock_sum/lock_count/n_looks pattern).         */
    size_t lock_count;    /**< looks accumulated in the current block.       */
    size_t avgs;          /**< non-coherent block size (looks/decision).     */
    double lock_stat;     /**< last block-averaged lock_signal
                                = mean(2*(|on-time|^2-|mid|^2)
                                        /(|on-time|^2+|mid|^2)) over avgs
                                looks; compare against the configured
                                threshold (see symsync_configure_lock).      */
    lockdet_state_t lock; /**< decision rule: thresholds + verify counters
                                stepped on lock_stat each avgs-look block. */
    symsync_tlm_t tlm; /**< live telemetry attachment; zeroed in blobs      */
  } symsync_state_t;

  /**
   * @brief Gardner timing-error detector: Re{ conj(mid) * (y - prev) }.
   *
   * Blind (non-data-aided): correlates the transition-gate sample against the
   * on-time step, so it locks for any constellation but pays a
   * non-transition-symbol self-noise cost. @see dttl_ted for the
   * decision-directed alternative.
   *
   * @param mid   Mid-symbol (transition-gate) interpolant.
   * @param diff  on_time[k] - on_time[k-1].
   * @return Raw (pre-AGC-normalized) timing error.
   */
  JM_FORCEINLINE double
  gardner_ted (float complex mid, float complex diff)
  {
    return (double)(crealf (mid) * crealf (diff)
                    + cimagf (mid) * cimagf (diff));
  }

  /**
   * @brief Sign-sign DTTL: gate the transition sample by the hard-decision
   * transition on each rail.
   *
   * Decision-directed (M.K. Simon's Data Transition Tracking Loop, digital
   * point-sample reduction): zero unless a rail's hard decision actually
   * flips between on_time[k-1] and on_time[k], in which case the error is
   * the transition-gate sample's value on that rail. Valid only for
   * constellations with independent, rectangular I/Q decision boundaries
   * (BPSK, QPSK/OQPSK) -- not 8PSK/QAM. Diff order (current minus previous)
   * matches gardner_ted's convention so both TEDs share one loop-filter
   * polarity.
   *
   * @param mid   Mid-symbol (transition-gate) interpolant.
   * @param y     on_time[k].
   * @param prev  on_time[k-1].
   * @return Raw (pre-AGC-normalized) timing error.
   */
  JM_FORCEINLINE double
  dttl_ted (float complex mid, float complex y, float complex prev)
  {
    double si = (crealf (y) >= 0.0f ? 1.0 : -1.0)
                - (crealf (prev) >= 0.0f ? 1.0 : -1.0);
    double sq = (cimagf (y) >= 0.0f ? 1.0 : -1.0)
                - (cimagf (prev) >= 0.0f ? 1.0 : -1.0);
    return (double)crealf (mid) * si + (double)cimagf (mid) * sq;
  }

  /**
   * @brief Per-sample symbol-timing step with the TED selection as a
   * parameter.
   *
   * The workhorse behind symsync_step()/symsync_steps(). Pushes one input
   * sample into the Farrow history and advances the integer timing NCO.
   * When the NCO crosses its half-scale (mid-symbol) it stores the
   * transition-gate interpolant; when it wraps (on-time) it forms the
   * on-time interpolant, runs the selected TED (see gardner_ted /
   * dttl_ted), steers the NCO frequency, and emits the timing-corrected
   * symbol.
   *
   * Passing a literal @p ted (SYMSYNC_TED_GARDNER / SYMSYNC_TED_DTTL) lets
   * the force-inlined body constant-fold the detector branch away, so a
   * specialised block loop carries exactly one TED — the runtime `s->ted`
   * branch inside the 64k-block loop measured ~30% slower (both TED bodies
   * kept live across the per-sample path). Compositions that hardcode a
   * detector (the MPSK receiver is Gardner-only) call this directly with
   * the literal; runtime-configured callers use symsync_step().
   *
   * @param s      State.  Must be non-NULL.
   * @param x      One input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @param ted    SYMSYNC_TED_GARDNER or SYMSYNC_TED_DTTL — pass a literal
   *               for a specialised (branch-free) instantiation.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  symsync_step_ted (symsync_state_t *s, float complex x, float complex *y_out,
                    int ted)
  {
    const uint32_t HALF = 0x80000000u;
    farrow_push (&s->farrow, x);
    uint32_t old    = s->timing.phase;
    uint64_t sum    = (uint64_t)old + s->timing.phase_inc;
    s->timing.phase = (uint32_t)sum;

    int wrapped = sum >> 32 != 0;
    int mid_evt = !wrapped && old < HALF && (uint32_t)sum >= HALF;
    if (!wrapped && !mid_evt)
      return 0;

    double inc = (double)s->timing.phase_inc;
    if (mid_evt)
      {
        float mu = (float)(1.0 - ((double)((uint32_t)sum - HALF)) / inc);
        s->mid   = farrow_eval (&s->farrow, mu);
        return 0;
      }
    float         mu   = (float)(1.0 - (double)s->timing.phase / inc);
    float complex y    = farrow_eval (&s->farrow, mu);
    int           emit = 0;
    if (s->have_ontime)
      {
        double num;
        if (ted == SYMSYNC_TED_DTTL)
          num = dttl_ted (s->mid, y, s->prev_ontime);
        else
          {
            float complex diff = y - s->prev_ontime;
            num                = gardner_ted (s->mid, diff);
          }
        double inst_pwr
            = (double)(crealf (y) * crealf (y) + cimagf (y) * cimagf (y));
        s->pwr_avg += 0.01 * (inst_pwr - s->pwr_avg);
        double e       = num / (s->pwr_avg + 1e-6);
        s->last_error  = e;
        double control = loop_filter_step (&s->lf, e);
        s->timing.phase_inc
            = (uint32_t)((double)s->base_inc * (1.0 + control));
        double inst = (double)s->sps / (1.0 + control);
        double lo_r = 0.5 * (double)s->sps, hi_r = 1.5 * (double)s->sps;
        if (inst < lo_r)
          inst = lo_r;
        else if (inst > hi_r)
          inst = hi_r;
        s->rate_est += 0.02 * (inst - s->rate_est);
        /* Lock statistic: lock_signal = 2*(|on-time|^2-|mid|^2)
         * /(|on-time|^2+|mid|^2), a Gardner-style eye-opening ratio (the
         * on-time sample vs. the mid-symbol/transition-gate sample already
         * used by the TED, reusing inst_pwr from above). Non-coherently
         * block-averaged over `avgs` looks before the decision, mirroring
         * dll_state_t's lock_sum/lock_count/n_looks tumbling window (a
         * sliding window would break the verify-count independence
         * assumption the same way it would for the DLL -- see
         * dll_configure_lock's derivation). See symsync_configure_lock()
         * for how avgs/threshold are sized from (rolloff, esno_min, pfa,
         * pd). */
        float complex md = s->mid;
        double        mid_pwr
            = (double)(crealf (md) * crealf (md) + cimagf (md) * cimagf (md));
        double lock_signal = 2.0 * (inst_pwr - mid_pwr)
                             / (inst_pwr + mid_pwr + SYMSYNC_LOCK_EPS);
        s->lock_sum += lock_signal;
        if (++s->lock_count >= s->avgs)
          {
            s->lock_stat = s->lock_sum / (double)s->avgs;
            (void)lockdet_step (&s->lock, s->lock_stat);
            s->lock_sum   = 0.0;
            s->lock_count = 0;
          }
        *y_out = y;
        emit   = 1;
      }
    else
      s->have_ontime = 1;
    s->prev_ontime = y;
    return emit;
  }

  /**
   * @brief Emit the timing loop's telemetry records for the symbol just
   * recovered.
   *
   * Out-of-line on purpose: the emit machinery must not inline into the
   * per-sample hot loops (three inlined ring-write expansions measured
   * ~20% slower detached, from sheer body growth). Callers gate on
   * `s->tlm.ctx` and call this once per emitted symbol — the detached
   * cost stays one predicted-not-taken branch per symbol, outside the
   * force-inlined step. Records "<prefix>.e" (last TED error),
   * "<prefix>.freq" (the NCO rate control, reconstructed as
   * phase_inc/base_inc - 1), "<prefix>.rate" (tracked samples/symbol),
   * "<prefix>.lock" (the last block-averaged lock_signal, refreshed every
   * avgs looks) and "<prefix>.locked" (the verify-counted lockdet
   * decision, 0/1).
   *
   * @param s  State with a non-NULL tlm.ctx (caller-checked).
   */
  void symsync_tlm_flush (const symsync_state_t *s);

  /**
   * @brief Per-sample symbol-timing step (the inline composition API).
   *
   * The public form of symsync_step_ted(): dispatches on the state's
   * configured detector (`s->ted`) and flushes telemetry when attached.
   * symsync_steps() is this in a loop (with the TED specialised per
   * detector); a tracking channel inlines it to drive a downstream
   * carrier loop on the recovered symbols.
   *
   * @param s      State.  Must be non-NULL.
   * @param x      One input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  symsync_step (symsync_state_t *s, float complex x, float complex *y_out)
  {
    int r = symsync_step_ted (s, x, y_out, s->ted);
    if (r && s->tlm.ctx)
      symsync_tlm_flush (s);
    return r;
  }

  /**
   * @brief Initialise a SymbolSync in place (no allocation).
   *
   * The by-value counterpart to symsync_create(): lets a composing object
   * embed a symsync_state_t by value and initialise it without a heap
   * allocation (symsync_state_t holds no heap members — the NCO, Farrow and
   * loop filter are all by value). Mirrors loop_filter_init()/costas_init().
   *
   * @param s      State to initialise.  Must be non-NULL.
   * @param sps    Nominal samples per symbol.
   * @param bn     Loop noise bandwidth (normalised to the symbol rate).
   * @param zeta   Damping factor (0.707 = critically damped).
   * @param order  Farrow interpolator order (0=linear, 1=parabolic, 2=cubic).
   * @param ted    Timing-error detector: SYMSYNC_TED_GARDNER (0, blind) or
   *               SYMSYNC_TED_DTTL (1, decision-directed; BPSK/QPSK only).
   */
  void symsync_init (symsync_state_t *s, size_t sps, double bn, double zeta,
                     int order, int ted);

  /**
   * @brief Create a symsync instance.
   *
   * @param sps  sps (default: 4).
   * @param bn  bn (default: 0.01).
   * @param zeta  zeta (default: 0.707).
   * @param order  Enum index; 0=linear…2=cubic.
   * @param ted  Enum index; 0=gardner, 1=dttl (BPSK/QPSK only).
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note Caller must call symsync_destroy() when done.
   */
  symsync_state_t *symsync_create (size_t sps, double bn, double zeta,
                                   int order, int ted);

  /**
   * @brief Destroy a symsync instance and release all memory.
   * @param state  May be NULL.
   */
  void symsync_destroy (symsync_state_t *state);

  /**
   * @brief Reset SymbolSync to its post-create state.
   * @param state  Must be non-NULL.
   */
  void symsync_reset (symsync_state_t *state);

  size_t symsync_steps_max_out (symsync_state_t *state);
  size_t symsync_steps (symsync_state_t *state, const float complex *x,
                        size_t x_len, float complex *out, size_t max_out);
  void   symsync_configure (symsync_state_t *state, double bn, double zeta);
  double symsync_get_bn (const symsync_state_t *state);
  void   symsync_set_bn (symsync_state_t *state, double val);
  double symsync_get_timing_error (const symsync_state_t *state);
  double symsync_get_rate (const symsync_state_t *state);

  /** @brief Last block-averaged lock statistic:
   *         mean(2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2)) over the
   *         configured avgs looks; compare against the configured
   *         threshold (see symsync_configure_lock). */
  double symsync_get_lock_stat (const symsync_state_t *state);

  /** @brief Current lock decision (1 = locked, 0 = not), with the
   *         configured verify-count / hysteresis rule applied. */
  int symsync_get_locked (const symsync_state_t *state);

  /**
   * @brief Tune the always-on timing-lock detector to a target (pfa, pd)
   * at a given link operating point.
   *
   * Sizes the non-coherent block size (avgs) and declare threshold from a
   * Gaussian sizing of the eye-opening statistic lock_signal =
   * 2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2): a per-look mean
   * (mean_lock_detect, from rolloff and the minimum operating Es/N0) drives
   * the classic N = variance*((Q^-1(pfa)-Q^-1(pd))/mean)^2 /
   * threshold = Q^-1(pfa)*mean/(Q^-1(pfa)-Q^-1(pd)) derivation, implemented
   * directly from a formula supplied by a doppler user (not re-derived
   * against a primary source). Empirically validated at the default
   * operating point (0 false declares over 500,000 independent noise-only
   * blocks against a nominal pfa=1e-3; 500/500 true declares at the
   * esno_min design SNR against a nominal pd=0.9): both targets are met
   * with large margin in the safe direction, because the formula's "8"
   * variance-role scale factor is ~6x larger than this statistic's real
   * measured per-look variance (~1.33) -- see symsync_core.c's
   * SYMSYNC_LOCK_DEFAULT_* comment for the full validation. The
   * consequence is pure declare latency (avgs comes out ~6x larger than
   * strictly needed to hit the stated targets), not reduced reliability.
   * No level hysteresis by default (up = down = threshold, matching
   * dll_configure_lock's shape); the raw escape hatch
   * (symsync_configure_lock_raw) exposes split thresholds, an explicit
   * avgs, and independent n_up/n_down.
   *
   * @param state        Must be non-NULL.
   * @param rolloff      Matched-filter excess bandwidth (e.g. 0.35 for a
   *                      typical RRC system).
   * @param esno_min_db  Minimum operating Es/N0, dB -- the worst-case link
   *                      point the detector must still declare lock at.
   * @param pfa          Target false-alarm probability per decision, in
   *                      (0, 1).
   * @param pd           Target detection probability per decision, in
   *                      (0, 1); must exceed pfa.
   * @return DP_OK, or DP_ERR_INVALID if pfa/pd are out of range or pd <= pfa.
   * @code
   * >>> from doppler.track import SymbolSync
   * >>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
   * >>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=1e-3, pd=0.9)
   * >>> ss.locked
   * False
   * >>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=0.9, pd=0.9)
   * Traceback (most recent call last):
   *     ...
   * ValueError: configure_lock failed (rc=-4)
   *
   * @endcode
   */
  int symsync_configure_lock (symsync_state_t *state, double rolloff,
                              double esno_min_db, double pfa, double pd);

  /**
   * @brief Set the lock detector's raw geometry directly.
   *
   * The escape hatch under symsync_configure_lock() for a caller that
   * derives its own averaging/threshold geometry: the block size (avgs), a
   * split declare/drop threshold pair on lock_stat (level hysteresis), and
   * both verify counts (time hysteresis). Re-tuning clears the in-flight
   * block sum and drops the lock so the next decision uses only looks
   * gathered under the new config.
   *
   * @param state        Must be non-NULL.
   * @param avgs         Non-coherent block size (looks/decision); clamped
   *                     >= 1.
   * @param up_thresh    Declare threshold on lock_stat.
   * @param down_thresh  Drop threshold; choose <= up_thresh for level
   *                     hysteresis.
   * @param n_up         Consecutive above-threshold decisions to declare;
   *                     clamped >= 1.
   * @param n_down       Consecutive below-threshold decisions to drop;
   *                     clamped >= 1.
   */
  void symsync_configure_lock_raw (symsync_state_t *state, size_t avgs,
                                   double up_thresh, double down_thresh,
                                   uint32_t n_up, uint32_t n_down);

  /**
   * @brief Attach (or detach) a telemetry context and register the timing
   * loop's probes on it.
   * Registers five probes, emitted once per recovered symbol and further
   * thinned by decim: "<prefix>.e" (the normalised TED error — the loop
   * stress), "<prefix>.freq" (the loop-filter control steering the timing
   * NCO, fractional rate offset), "<prefix>.rate" (the smoothed tracked
   * samples/symbol), "<prefix>.lock" (the last block-averaged lock_signal,
   * held between avgs-look updates) and "<prefix>.locked" (the
   * verify-counted lockdet decision, 0/1).
   * Passing NULL detaches.  Setup path, never hot: call before the
   * producer thread starts stepping; the context is borrowed and must
   * outlive the attachment (SPSC rules in telemetry/telemetry.h).
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "sync" or "rx.sync".
   * @param decim  Emit every decim-th symbol; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
   *         five probes (the attach fails whole; the object stays
   *         detached).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import SymbolSync
   * >>> from doppler.telemetry import Telemetry
   * >>> tlm = Telemetry(1 << 12)
   * >>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
   * >>> ss.set_telemetry(tlm, "sync")
   * >>> sorted(tlm.probe_names())
   * ['sync.e', 'sync.freq', 'sync.lock', 'sync.locked', 'sync.rate']
   * >>> x = np.repeat([1 + 1j, -1 - 1j], 4 * 64).astype(np.complex64)
   * >>> _ = ss.steps(x)
   * >>> recs = tlm.read()   # five records per recovered symbol
   * >>> len(recs) > 0 and len(recs) % 5 == 0
   * True
   *
   * @endcode
   */
  int symsync_set_telemetry (symsync_state_t *state, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * pointer-free composition: nco + farrow + loop_filter embedded by value
 * (all POD) + scalar timing state — a whole-struct snapshot. */
#define SYMSYNC_STATE_MAGIC DP_FOURCC ('S', 'Y', 'N', 'C')
#define SYMSYNC_STATE_VERSION                                                 \
  5u /* v5: block-averaged lock_signal statistic (avgs/lock_sum/lock_count)   \
      */
  size_t symsync_state_bytes (const symsync_state_t *state);
  void   symsync_get_state (const symsync_state_t *state, void *blob);
  int    symsync_set_state (symsync_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* SYMSYNC_CORE_H */
