/**
 * @file ratesync_core.h
 * @brief RateSync component API — an RRC matched filter fused with
 *        symbol-timing recovery on one polyphase resampler.
 *
 * Where SymbolSync separates the two jobs (a matched FIR, then a Farrow
 * interpolator steered by a timing NCO), RateSync does them in a single dot
 * product: the polyphase bank IS the root-raised-cosine matched filter, and
 * the arm the resampler's accumulator selects IS the fractional timing
 * delay. One filter, no Farrow.
 *
 * That fusion buys **arbitrary-rate reception**. The `resamp` accumulator is
 * a double, so `sps` is a double: 4, 17.33389, an irrational ratio, or a
 * slowly drifting clock all work by construction — the loop only has to
 * steer the strobe, not resample first. A fixed-integer-`sps` matched filter
 * cannot do that, and it is the real-world case whenever the ADC clock is
 * free-running against the symbol clock.
 *
 * **Two banks, not two samples per symbol.** The resampler runs at
 * `rate = 1/sps` — exactly one strobe per symbol — and the Gardner
 * transition-gate ("mid") sample comes from a *second* bank whose prototype
 * is displaced half a symbol, driven by the same input and the same control.
 * Both accumulators therefore evolve identically and the two strobes are
 * always paired, so "on-time" and "mid" are pinned **structurally**. Running
 * one bank at `rate = 2/sps` and calling alternate outputs on-time/mid
 * instead makes the roles a parity count anchored to nothing at the symbol
 * rate: the S-curve then has period T/2 with TWO equally stable equilibria,
 * the wrong one sampling the transitions, and the loop hunts between them.
 *
 * Lifecycle: `create -> (step / steps / reset)* -> destroy`
 *
 * Example:
 * @code
 * ratesync_state_t *rx = ratesync_create (4.0, 0.35, 8, 1024, 0.005, 0.707,
 *                                       RATESYNC_TED_GARDNER);
 * float complex sym;
 * if (ratesync_step (rx, x, &sym))
 *   consume (sym);
 * ratesync_destroy (rx);
 * @endcode
 */
#ifndef RATESYNC_CORE_H
#define RATESYNC_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "resamp/resamp_core.h"
#include "symsync/symsync_core.h" /* gardner_ted / dttl_ted — one TED, reused */
#include "telemetry/telemetry.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Timing-error-detector selection for ratesync_state_t::ted. */
  enum
  {
    RATESYNC_TED_GARDNER = 0, /**< blind Gardner TED (mid * conj diff).   */
    RATESYNC_TED_DTTL    = 1  /**< decision-directed sign-sign DTTL.      */
  };

  /**
   * @brief Matched-filter pulse shape — the ONLY thing that differs between
   *        a band-limited link and a rectangular one.
   *
   * The fusion (bank == matched filter, arm == fractional delay) is a
   * property of the polyphase engine, not of the root-raised cosine, so the
   * rectangular/NRZ case that dominates chip- and NRZ-rate links is the same
   * object with a different prototype. Values match the MPSK receiver's
   * (`MPSK_RX_PULSE_IANDD` / `_RRC`) so one vocabulary covers both.
   */
  enum
  {
    RATESYNC_PULSE_IANDD = 0, /**< rectangular: integrate-and-dump boxcar. */
    RATESYNC_PULSE_RRC   = 1  /**< root-raised cosine, roll-off `beta`.    */
  };

  /**
   * @brief The matched filter's prototype at one instant, in symbol periods.
   *
   * `RATESYNC_PULSE_RRC` delegates to the canonical `wfm_rrc_h()`;
   * `RATESYNC_PULSE_IANDD` is the unit rectangle over one symbol — matched to
   * a rectangular symbol, and the pulse an integrate-and-dump implements.
   *
   * @param pulse  RATESYNC_PULSE_IANDD or RATESYNC_PULSE_RRC.
   * @param t      time in symbol periods, relative to the pulse centre.
   * @param beta   RRC roll-off (ignored for the rectangular pulse).
   */
  double ratesync_pulse_h (int pulse, double t, double beta);

  /** @brief One-sided support of a pulse, in symbols: `span` for the RRC,
   *  always 0.5 for the rectangle (which is one symbol wide, whatever
   *  `span` says). */
  double ratesync_pulse_support (int pulse, size_t span);

/* Numerical guard on the on-time+mid energy sum feeding the lock statistic
 * (not tunable) — mirrors SYMSYNC_LOCK_EPS. */
#define RATESYNC_LOCK_EPS 1e-12

  /**
   * @brief Telemetry attachment: a borrowed context + this object's probe
   *        ids. NULL ctx (the default) means detached — every probe site is
   *        then one predicted-not-taken branch per recovered symbol.
   */
  typedef struct
  {
    dp_tlm_t *ctx;       /**< NULL = detached                          */
    int32_t   id_e;      /**< "<prefix>.e"      — normalised TED error */
    int32_t   id_ctrl;   /**< "<prefix>.ctrl"   — loop control         */
    int32_t   id_rate;   /**< "<prefix>.rate"   — tracked samples/sym  */
    int32_t   id_lock;   /**< "<prefix>.lock"   — lock_signal mean     */
    int32_t   id_locked; /**< "<prefix>.locked" — lockdet flag         */
  } ratesync_tlm_t;

  /**
   * @brief RateSync state.
   *
   * Allocate with ratesync_create(). The two matched filters are heap
   * `resamp` children (they own the banks and delay lines); the loop filter
   * and lock detector are embedded by value.
   */
  typedef struct
  {
    resamp_state_t *mf_on;  /**< matched filter, on-time strobe.        */
    resamp_state_t *mf_mid; /**< same filter, half a symbol earlier.    */
    loop_filter_state_t lf; /**< 2nd-order timing PI loop.              */

    /* ── config (restored by create(), never packed in a state blob) ── */
    double sps;        /**< nominal samples per symbol (any double >= 1). */
    int    pulse;      /**< RATESYNC_PULSE_IANDD / _RRC.                  */
    double beta;       /**< RRC roll-off.                                */
    size_t span;       /**< one-sided RRC span, symbols.                 */
    size_t num_phases; /**< bank arms (power of two).                    */
    size_t num_taps;   /**< taps per arm.                                */
    double bn;         /**< loop noise bandwidth (retained).             */
    double zeta;       /**< damping factor (retained).                   */
    int    ted;        /**< RATESYNC_TED_GARDNER / _DTTL.                 */

    /* ── running state ──────────────────────────────────────────────── */
    double ctrl;       /**< per-input rate deviation now applied.        */
    double last_error; /**< last normalised TED error.                   */
    double pwr_avg;    /**< running symbol power (TED normaliser).       */
    double rate_est;   /**< smoothed tracked samples/symbol.             */
    int    have_on;    /**< a previous on-time strobe exists.            */
    float complex prev_on; /**< previous on-time strobe.                 */

    /* ── lock detector (always on): tumbling-window block average ───── */
    double lock_sum;   /**< running sum over the current avgs block.     */
    size_t lock_count; /**< looks accumulated in the current block.      */
    size_t avgs;       /**< non-coherent block size (looks/decision).    */
    double lock_stat;  /**< last block-averaged lock_signal.             */
    lockdet_state_t lock; /**< declare/drop rule stepped on lock_stat.   */

    ratesync_tlm_t tlm; /**< live telemetry attachment; zeroed in blobs.  */
  } ratesync_state_t;

  /* ------------------------------------------------------------------
   * Bank construction
   * ------------------------------------------------------------------ */

  /**
   * @brief Taps per arm for a matched-filter bank of the given geometry.
   *
   * Covers the pulse's full support plus the extra half symbol the
   * transition-gate bank is displaced by, sampled on the input grid:
   * `ceil((2*support + 0.5) * sps) + 1`. The rectangular pulse needs only
   * `ceil(1.5*sps)+1` taps against the RRC's `ceil((2*span+0.5)*sps)+1`, so
   * an NRZ link's matched filter is dramatically cheaper.
   *
   * @param pulse  RATESYNC_PULSE_IANDD or RATESYNC_PULSE_RRC.
   * @param sps    samples per symbol (>= 1, any double).
   * @param span   one-sided RRC span in symbols (ignored for the rectangle).
   */
  size_t ratesync_bank_ntaps (int pulse, double sps, size_t span);

  /**
   * @brief Build one RRC matched-filter arm bank for the ctrl-path convention.
   *
   * `bank[p*num_taps + t] = wfm_rrc_h(-t/sps + span + offset_sym + p/P)`,
   * i.e. tap `t` multiplies `x[n-t]` (the delay line is newest-first) and arm
   * `p` moves the sampling instant by `p/P` of an **output period**.
   *
   * **The `+p/P` sign is load-bearing.** The accumulator is the only timing
   * authority; the arm is that phase's fractional read-out, so it must move
   * the instant the *same* way crossing an emission boundary does. Build the
   * bank with `-p/P` and the two fight: the effective sampling instant
   * becomes a sawtooth of one full output period (measured 2.02 input
   * samples peak-to-peak), which no loop bandwidth can remove.
   *
   * Because `sps` is an arbitrary double, the arm instants are **not** a
   * uniform sub-multiple of the input grid, so this samples the analytic
   * pulse directly rather than decomposing an oversampled prototype the way
   * the TX shaper's `wfm_rrc_polyphase_bank()` can.
   *
   * @param pulse       RATESYNC_PULSE_IANDD or RATESYNC_PULSE_RRC.
   * @param beta        RRC roll-off in `[0, 1]` (ignored for the rectangle).
   * @param sps         samples per symbol (>= 1).
   * @param span        one-sided RRC span in symbols (ignored for the
   *                    rectangle, whose support is always one symbol).
   * @param num_phases  arms (power of two).
   * @param num_taps    taps per arm (ratesync_bank_ntaps()).
   * @param offset_sym  extra delay in symbols: 0 for on-time, 0.5 for the
   *                    transition gate (half a symbol earlier).
   * @param bank        output, `num_phases * num_taps` floats, row-major.
   *                    Unnormalised — ratesync_create() applies one common
   *                    scale to both banks so |on-time| and |mid| stay
   *                    directly comparable.
   */
  void ratesync_bank (int pulse, double beta, double sps, size_t span,
                     size_t num_phases, size_t num_taps, double offset_sym,
                     float *bank);

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Create an RateSync instance.
   *
   * @param sps         Nominal samples per symbol — any double >= 1
   *                    (17.33389 is as valid as 4).
   * @param pulse       Matched-filter shape: RATESYNC_PULSE_IANDD
   *                    (rectangular/NRZ) or RATESYNC_PULSE_RRC.
   * @param beta        RRC roll-off in `[0, 1]` (default 0.35; ignored for
   *                    the rectangular pulse).
   * @param span        One-sided RRC span in symbols (default 8; ignored for
   *                    the rectangular pulse).
   * @param num_phases  Matched-filter arms; power of two (default 1024).
   *                    Sets the fractional-timing resolution: `1/num_phases`
   *                    of a symbol.
   * @param bn          Loop noise bandwidth, normalised to the symbol rate.
   * @param zeta        Damping factor (0.707 = critically damped).
   * @param ted         RATESYNC_TED_GARDNER (blind) or RATESYNC_TED_DTTL
   *                    (decision-directed; BPSK/QPSK only).
   * @return Heap-allocated state, or NULL if a parameter is out of range or
   *         allocation fails.
   * @note Caller must call ratesync_destroy() when done.
   */
  ratesync_state_t *ratesync_create (double sps, int pulse, double beta,
                                   size_t span, size_t num_phases, double bn,
                                   double zeta, int ted);

  /** @brief Destroy an RateSync instance and release all memory.
   *  @param state  May be NULL. */
  void ratesync_destroy (ratesync_state_t *state);

  /** @brief Reset to the post-create state: both matched filters, the loop
   *         integrator, the lock detector and the TED history.
   *  @param state  Must be non-NULL. */
  void ratesync_reset (ratesync_state_t *state);

  /* ------------------------------------------------------------------
   * Execute
   * ------------------------------------------------------------------ */

  /**
   * @brief Per-input timing step with the TED selection as a parameter.
   *
   * The workhorse behind ratesync_step()/ratesync_steps(). Pushes one input
   * through both matched filters at the current control deviation; when the
   * accumulator completes a symbol period both emit (their accumulators are
   * bit-identical by construction), the TED compares the transition-gate
   * sample against the on-time step, the PI loop steers the next control,
   * and the on-time sample is the recovered symbol.
   *
   * Passing a literal @p ted lets the force-inlined body constant-fold the
   * detector branch away, exactly as symsync_step_ted() does.
   *
   * @param s      State. Must be non-NULL.
   * @param x      One input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @param ted    RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL — pass a literal
   *               for a specialised (branch-free) instantiation.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  ratesync_step_ted (ratesync_state_t *s, float complex x, float complex *y_out,
                    int ted)
  {
    float complex on = 0.0f, mid = 0.0f;
    /* Both filters see the same input and the same control, so their
       accumulators are bit-identical and they emit together. The mid arm is
       half a symbol early because its BANK carries the offset — never
       because of an emission count. */
    size_t n = resamp_execute_ctrl_push (s->mf_on, x, s->ctrl, &on, 1);
    (void)resamp_execute_ctrl_push (s->mf_mid, x, s->ctrl, &mid, 1);
    if (n == 0)
      return 0;

    int emit = 0;
    if (s->have_on)
      {
        double num;
        if (ted == RATESYNC_TED_DTTL)
          num = dttl_ted (mid, on, s->prev_on);
        else
          num = gardner_ted (mid, on - s->prev_on);

        double inst_pwr
            = (double)(crealf (on) * crealf (on) + cimagf (on) * cimagf (on));
        s->pwr_avg += 0.01 * (inst_pwr - s->pwr_avg);
        double e      = num / (s->pwr_avg + 1e-6);
        s->last_error = e;

        /* loop_filter_step returns a correction in symbols per symbol; ctrl
           is a per-INPUT deviation of an accumulator that counts output
           periods (one per symbol), so spread it over the sps inputs a
           symbol spans. e > 0 means the strobe is LATE, and a positive ctrl
           advances it — the classic Gardner polarity. */
        s->ctrl = loop_filter_step (&s->lf, e) / s->sps;

        /* Tracked samples/symbol from the loop INTEGRATOR, not from the
           instantaneous control. The integrator is the rate memory
           (loop_filter_core.h: "kp*e is the instantaneous phase nudge");
           feeding the noisy total through the convex 1/(1/sps + ctrl)
           instead biases the estimate high by Jensen — measured +2.7e-4
           samples/symbol at every offset, including zero. Exact here: a
           loop tracking true sps settles at integ = sps/true - 1, so
           sps/(1 + integ) == true. */
        double inst = s->sps / (1.0 + s->lf.integ);
        double lo_r = 0.5 * s->sps, hi_r = 1.5 * s->sps;
        if (!(inst > lo_r))
          inst = lo_r;
        else if (inst > hi_r)
          inst = hi_r;
        s->rate_est = inst;

        /* Lock statistic: the Gardner eye-opening ratio
           2*(|on|^2 - |mid|^2)/(|on|^2 + |mid|^2) — open eye => on-time
           carries the energy and the transition gate sits near zero.
           Non-coherently block-averaged over `avgs` looks before the
           decision (a tumbling window, so the verify counts stay
           independent), mirroring symsync/dll. */
        double mid_pwr = (double)(crealf (mid) * crealf (mid)
                                  + cimagf (mid) * cimagf (mid));
        double lock_signal = 2.0 * (inst_pwr - mid_pwr)
                             / (inst_pwr + mid_pwr + RATESYNC_LOCK_EPS);
        s->lock_sum += lock_signal;
        if (++s->lock_count >= s->avgs)
          {
            s->lock_stat = s->lock_sum / (double)s->avgs;
            (void)lockdet_step (&s->lock, s->lock_stat);
            s->lock_sum   = 0.0;
            s->lock_count = 0;
          }
        *y_out = on;
        emit   = 1;
      }
    else
      s->have_on = 1;
    s->prev_on = on;
    return emit;
  }

  /**
   * @brief Emit the timing loop's telemetry for the symbol just recovered.
   *
   * Out-of-line on purpose: the emit machinery must not inline into the
   * per-sample hot loop (the same body-growth cost symsync measured).
   * Callers gate on `s->tlm.ctx`, so the detached cost is one
   * predicted-not-taken branch per symbol.
   *
   * @param s  State with a non-NULL tlm.ctx (caller-checked).
   */
  void ratesync_tlm_flush (const ratesync_state_t *s);

  /**
   * @brief Per-input timing step (the inline composition API).
   *
   * The public form of ratesync_step_ted(): dispatches on the configured
   * detector and flushes telemetry when attached.
   *
   * @param s      State. Must be non-NULL.
   * @param x      One input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  ratesync_step (ratesync_state_t *s, float complex x, float complex *y_out)
  {
    int r = ratesync_step_ted (s, x, y_out, s->ted);
    if (r && s->tlm.ctx)
      ratesync_tlm_flush (s);
    return r;
  }

  /** @brief Output-buffer hint for the generated binding; 0 means "the
   *  input length is already a safe bound" — with `sps >= 1` a block can
   *  never yield more symbols than it has samples (mirrors symsync). */
  size_t ratesync_steps_max_out (ratesync_state_t *state);

  /**
   * @brief Recover symbol timing from a block of oversampled cf32 baseband.
   *
   * ratesync_step() in a loop, with the TED specialised per detector; state
   * carries across calls, so contiguous blocks give the same symbols as one
   * large block.
   *
   * @param state    Must be non-NULL.
   * @param x        Input samples.
   * @param x_len    Number of inputs.
   * @param out      Recovered symbols.
   * @param max_out  Capacity of @p out.
   * @return Symbols written to @p out.
   */
  size_t ratesync_steps (ratesync_state_t *state, const float complex *x,
                        size_t x_len, float complex *out, size_t max_out);

  /* ------------------------------------------------------------------
   * Properties / configuration
   * ------------------------------------------------------------------ */

  /** @brief Retune the loop; preserves the integrator (and so the lock). */
  void   ratesync_configure (ratesync_state_t *state, double bn, double zeta);
  double ratesync_get_bn (const ratesync_state_t *state);
  void   ratesync_set_bn (ratesync_state_t *state, double val);

  /** @brief Last normalised TED error — the loop stress. */
  double ratesync_get_timing_error (const ratesync_state_t *state);

  /** @brief Smoothed tracked samples per symbol. Departs from the nominal
   *         `sps` by exactly the sample-clock offset being tracked, so it is
   *         the estimator a rate-disciplining caller reads. */
  double ratesync_get_rate (const ratesync_state_t *state);

  /** @brief Current per-input control deviation steering the strobe. */
  double ratesync_get_ctrl (const ratesync_state_t *state);

  /** @brief Last block-averaged lock statistic (the eye-opening ratio). */
  double ratesync_get_lock_stat (const ratesync_state_t *state);

  /** @brief Current lock decision (1 = locked), verify-counted. */
  int ratesync_get_locked (const ratesync_state_t *state);

  /**
   * @brief Set the lock detector's geometry directly.
   *
   * The block size (avgs), a split declare/drop threshold pair on lock_stat
   * (level hysteresis) and both verify counts (time hysteresis). Re-tuning
   * clears the in-flight block sum and drops the lock, so the next decision
   * uses only looks gathered under the new config.
   *
   * @param state        Must be non-NULL.
   * @param avgs         Looks per decision; clamped >= 1.
   * @param up_thresh    Declare threshold on lock_stat.
   * @param down_thresh  Drop threshold; <= up_thresh for level hysteresis.
   * @param n_up         Consecutive above-threshold decisions to declare.
   * @param n_down       Consecutive below-threshold decisions to drop.
   */
  void ratesync_configure_lock_raw (ratesync_state_t *state, size_t avgs,
                                   double up_thresh, double down_thresh,
                                   uint32_t n_up, uint32_t n_down);

  /**
   * @brief Attach (or detach) a telemetry context and register the probes.
   *
   * Registers five probes, emitted once per recovered symbol and further
   * thinned by @p decim: "<prefix>.e" (normalised TED error),
   * "<prefix>.ctrl" (the per-input control steering the strobe),
   * "<prefix>.rate" (tracked samples/symbol), "<prefix>.lock" (last
   * block-averaged lock_signal) and "<prefix>.locked" (0/1). Passing NULL
   * detaches. Setup path, never hot: the context is borrowed and must
   * outlive the attachment (SPSC rules in telemetry/telemetry.h).
   *
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "sync".
   * @param decim  Emit every decim-th symbol; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
   *         five probes (the attach fails whole; the object stays detached).
   */
  int ratesync_set_telemetry (ratesync_state_t *state, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ─────────
 * A composition: the envelope and this object's own running scalars, then
 * the two matched filters' and the loop filter's self-validating child
 * blobs. Config (sps/beta/span/num_phases/bn/zeta/ted) is restored by
 * create(), never packed. */
#define RATESYNC_STATE_MAGIC DP_FOURCC ('R', 'A', 'T', 'S')
#define RATESYNC_STATE_VERSION 1u

  /** @brief Bytes ratesync_get_state() writes (envelope + payload + children). */
  size_t ratesync_state_bytes (const ratesync_state_t *state);
  /** @brief Serialize the mutable state into @p blob. */
  void ratesync_get_state (const ratesync_state_t *state, void *blob);
  /** @brief Restore mutable state from @p blob into an identically built
   *  instance. @return DP_OK, or DP_ERR_INVALID if any envelope rejects. */
  int ratesync_set_state (ratesync_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* RATESYNC_CORE_H */
