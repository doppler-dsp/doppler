/**
 * @file ratesync_core.h
 * @brief RateSync — symbol-timing recovery on a matched-filter rate cascade.
 *
 * RateSync owns a `RateConverter` whose **terminal stage carries the pulse**
 * (`RateConverter_create_matched`) and closes a timing loop around that
 * stage's control port. It builds no filters of its own: the matched filter
 * IS the cascade's last dot product, and the polyphase arm that dot product
 * selects IS the fractional timing delay. One filter, no Farrow, no separate
 * matched-filter pass.
 *
 * Where SymbolSync separates the jobs (a matched FIR, then a Farrow
 * interpolator steered by a timing NCO), this fuses them — and because the
 * cascade in front is a full `RateConverter`, the fusion inherits its
 * planning: HB/CIC stages do the bulk decimation for free, so the matched
 * filter is sized by the POST-decimation rate. A matched filter at 256
 * input samples per symbol costs the same bank as one at 4.
 *
 * **Arbitrary rate, by construction.** `sps` is a double — 4, 17.33389, an
 * irrational ratio, or a slowly drifting clock — because the terminal stage's
 * accumulator is a double and the loop only has to steer the strobe. That is
 * the real-world case whenever the ADC clock is free-running against the
 * symbol clock.
 *
 * ## Two things this object gets right that are easy to get wrong
 *
 * **1. The TED normaliser is `|on|^2 + |mid|^2`, never `|on|^2`.** The
 * on-time energy vanishes exactly when the strobe sits on the symbol
 * transitions — which is precisely the state the loop must recover FROM — so
 * normalising by it alone divides by zero at the worst possible moment.
 * Measured: the error reaches -91, the control drives the terminal stage's
 * effective rate NEGATIVE, its accumulator stops advancing, and the cascade
 * emits nothing ever again (2 symbols where 4000 were expected — a permanent
 * death, not a transient). The two energies are the same signal half a symbol
 * apart, so their sum is bounded away from zero at every timing phase; the
 * lock statistic already needs both. This is why RateSync needs no clamp on
 * the control anywhere: with a normaliser that cannot vanish, there is no
 * runaway to clamp.
 *
 * **2. The loop stays open until the cascade is primed.** A cascade's first
 * outputs are its delay lines filling, not signal (the eye statistic swings
 * over its whole +-2 range through them). Steering on them is meaningless and
 * was worth one lost acquisition in sixteen. ratesync_create() computes the
 * prime length from the terminal bank's own geometry.
 *
 * ## The T/2 role ambiguity resolves itself
 *
 * Gardner needs an on-time strobe and a transition gate half a symbol earlier.
 * Running at `rate = m/sps` and taking every m-th output as on-time makes that
 * a parity count, which looks like it should be ambiguous — and a half-symbol
 * error is indeed an equilibrium of the detector. It is an **unstable** one:
 * measured over a fine sweep, each parity's S-curve has exactly two zeros per
 * symbol, one at the eye centre with negative slope (stable) and one at the
 * T/2 point with positive slope (unstable). The loop runs away from the wrong
 * one on its own, so **the parity does not matter** and no eye-sign detector
 * or counter flip is needed. (An earlier prototype ran two displaced banks to
 * pin the roles structurally; measurement showed that buys nothing and costs
 * double the multiplies.)
 *
 * ## Measured
 *
 * RRC-BPSK, noiseless, eight initial timing offsets each, `bn = 0.01`:
 *
 * | sps    | planned cascade                  | lock | EVM      |
 * | ------ | -------------------------------- | ---- | -------- |
 * | 4      | HB + Resampler(1,rrc)            | 8/8  | -40.1 dB |
 * | 17.333 | CIC(8) + Resampler(0.923,rrc)    | 8/8  | -37.4 dB |
 * | 64     | CIC(32) + Resampler(1,rrc)       | 8/8  | -37.3 dB |
 *
 * `bn` behaves identically across all three (within ~2 dB at every setting),
 * which is the point of referencing the control to the terminal rate: the
 * loop bandwidth means the same thing whatever the planner decided to do in
 * front of it. `bn = 0.005` measured best here (-46 / -40 / -40 dB); lower
 * settings acquire too slowly to have settled within the test length.
 *
 * Lifecycle: `create -> (step / steps / reset)* -> destroy`
 *
 * @code
 * ratesync_state_t *rx = ratesync_create (17.33389, RATESYNC_PULSE_RRC, 0.35,
 *                                         8, 2, 1024, 0.01, 0.707,
 *                                         RATESYNC_TED_GARDNER);
 * float complex sym;
 * if (ratesync_step (rx, x, &sym))
 *   consume (sym);
 * ratesync_destroy (rx);
 * @endcode
 */
#ifndef RATESYNC_CORE_H
#define RATESYNC_CORE_H

#include "RateConverter/RateConverter_core.h"
#include "cic/cic_core.h"
#include "clib_common.h"
#include "dp_state.h"
#include "fir/fir_core.h"
#include "hbdecim/hbdecim_core.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "resamp/resamp_core.h"
#include "resample/resample_core.h"
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
   * @brief Matched-filter pulse shape.
   *
   * Aliases of the cascade's own vocabulary (`rc_pulse_t`) so one set of
   * names covers the family; the pulse is built by the RateConverter, which
   * is the only party that knows its own CIC geometry.
   */
  enum
  {
    RATESYNC_PULSE_IANDD = RC_PULSE_IANDD, /**< rectangular / NRZ.        */
    RATESYNC_PULSE_RRC   = RC_PULSE_RRC    /**< root-raised cosine.       */
  };

/** Largest supported outputs-per-symbol (bounds the strobe ring in-struct). */
#define RATESYNC_MAX_M 8

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
    int32_t   id_mu;     /**< "<prefix>.mu"     — timing NCO phase     */
  } ratesync_tlm_t;

  /**
   * @brief The symbol-timing loop, independent of what feeds it.
   *
   * Everything RateSync does *after* the cascade emits an output: the strobe
   * ring, the TED, the PI loop, the lock detector and the telemetry. It holds
   * no filter and no cascade — it consumes a stream of terminal-stage outputs
   * and produces a per-input rate deviation (`ctrl`) for whoever owns the
   * accumulator those outputs came from.
   *
   * That split is what lets a receiver reuse this loop verbatim. RateSync
   * owns a `RateConverter` and steers it directly; MpskReceiver owns a
   * `Ddc`/`Ddcr` (mix + the same cascade) and steers the *same* accumulator
   * through the DDC's `rate_ctrl` port. Both drive one implementation of the
   * timing loop, so a fix to the TED or the normaliser reaches both — the two
   * are not peers that can drift apart.
   *
   * The loop must be told the geometry of the accumulator it is steering
   * (ratesync_loop_set_cascade()): the terminal stage's own rate, because
   * that is the scale `ctrl` is referenced to, and the terminal bank's tap
   * count, because that is how many outputs are delay-line fill rather than
   * signal.
   */
  typedef struct
  {
    loop_filter_state_t lf; /**< 2nd-order timing PI loop.               */

    /* ── config (restored by the owner's create(), never packed) ────── */
    double sps;        /**< nominal samples per symbol (any double).      */
    size_t m;          /**< terminal outputs per symbol (>= 2, even).     */
    double term_rate;  /**< terminal stage's own rate; the ctrl scale.    */
    size_t prime_taps; /**< terminal bank taps; sets the prime length.    */
    /** The terminal stage itself, borrowed for TELEMETRY ONLY: the loop
     *  steers this accumulator but does not own it, and `mu` — the sampling
     *  phase the steering produces — is otherwise unobservable from outside
     *  the cascade. NULL when the owner bound the geometry by hand
     *  (ratesync_loop_set_cascade()) rather than from a cascade; the probe
     *  then reports 0. Never dereferenced on the hot path. */
    const resamp_state_t *term;
    double bn;         /**< loop noise bandwidth (retained).              */
    double zeta;       /**< damping factor (retained).                    */
    int    ted;        /**< RATESYNC_TED_GARDNER / _DTTL.                 */

    /* ── running state ──────────────────────────────────────────────── */
    double ctrl;       /**< per-input rate deviation now applied.         */
    double last_error; /**< last normalised TED error.                    */
    double pwr_avg;    /**< running |on|^2+|mid|^2 (the TED normaliser).  */
    double rate_est;   /**< smoothed tracked samples/symbol.              */
    int    pwr_seeded; /**< pwr_avg has taken its first value.            */
    int    have_prev;  /**< a previous on-time strobe exists.             */
    size_t prime_left; /**< strobes still to discard (cascade filling).   */
    size_t out_count;  /**< terminal outputs seen (mod m: strobe phase).  */

    /** Newest-first ring of the last m/2+1 outputs: the transition gate is
     *  m/2 outputs behind the on-time strobe, so it is simply the element at
     *  index m/2. (Written without brackets on purpose: mkdoxy renders this
     *  comment into markdown, where a bare `name[i]` parses as a link
     *  reference and fails the --strict docs build.) */
    float complex ring[RATESYNC_MAX_M / 2 + 1];
    size_t        ring_n;
    float complex prev_on; /**< previous on-time strobe.                  */

    /* ── lock detector (always on): tumbling-window block average ───── */
    double lock_sum;      /**< running sum over the current avgs block.      */
    size_t lock_count;    /**< looks accumulated in the current block.       */
    size_t avgs;          /**< non-coherent block size (looks/decision).     */
    double lock_stat;     /**< last block-averaged lock_signal.              */
    lockdet_state_t lock; /**< declare/drop rule stepped on lock_stat.    */

    ratesync_tlm_t tlm; /**< live telemetry attachment; zeroed in blobs.  */
  } ratesync_loop_t;

  /**
   * @brief RateSync state: a matched-filter cascade and the timing loop.
   *
   * The matched filter is a heap `RateConverter` child (it owns the cascade,
   * the banks and every delay line); the loop is embedded by value.
   */
  typedef struct
  {
    RateConverter_state_t *mf;   /**< cascade; terminal stage is the MF.  */
    ratesync_loop_t        loop; /**< timing loop closed around it.       */

    /* ── config (restored by create(), never packed in a state blob) ── */
    int    pulse;      /**< RATESYNC_PULSE_IANDD / _RRC.                  */
    double beta;       /**< RRC roll-off.                                 */
    size_t span;       /**< one-sided RRC span, symbols.                  */
    size_t num_phases; /**< bank arms (power of two).                     */
  } ratesync_state_t;

  /* ------------------------------------------------------------------
   * The timing loop on its own (shared with the receivers)
   * ------------------------------------------------------------------ */

  /**
   * @brief Initialise a standalone timing loop.
   *
   * Sets the loop filter (update period = one symbol, so @p bn is normalised
   * to the symbol rate) and the default lock-detector geometry, then seeds
   * every running field. The caller must still describe the accumulator being
   * steered with ratesync_loop_set_cascade() before pushing outputs through.
   *
   * @param l     Loop to initialise. Must be non-NULL.
   * @param sps   Nominal samples per symbol (any double).
   * @param m     Terminal outputs per symbol; even, 2..RATESYNC_MAX_M.
   * @param bn    Loop noise bandwidth, normalised to the symbol rate.
   * @param zeta  Damping factor.
   * @param ted   RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL.
   */
  void ratesync_loop_init (ratesync_loop_t *l, double sps, size_t m, double bn,
                           double zeta, int ted);

  /**
   * @brief Tell the loop the geometry of the accumulator it steers.
   *
   * @param l          Loop. Must be non-NULL.
   * @param term_rate  The terminal stage's own rate. `ctrl` is referenced to
   *                   this, not to the overall cascade rate — they differ by
   *                   the whole integer decimation in front, which would
   *                   under-drive the loop by exactly that factor.
   * @param prime_taps The terminal bank's tap count; the loop discards
   *                   `prime_taps + 1` outputs before closing, because those
   *                   are the delay lines filling rather than signal.
   */
  void ratesync_loop_set_cascade (ratesync_loop_t *l, double term_rate,
                                  size_t prime_taps);

  /**
   * @brief Read that geometry straight off a cascade.
   *
   * Walks @p rc to its terminal stage and forwards its rate and tap count to
   * ratesync_loop_set_cascade(). Every owner of this loop owns a
   * `RateConverter` somewhere — RateSync directly, the receivers inside their
   * DDC — so the walk lives here once rather than in each of them.
   *
   * @param l   Must be non-NULL.
   * @param rc  The cascade whose terminal stage the loop steers.
   */
  void ratesync_loop_bind_cascade (ratesync_loop_t             *l,
                                   const RateConverter_state_t *rc);

  /** @brief Re-seed the loop: integrator, strobe ring, lock detector and the
   *         prime countdown. Configuration and cascade geometry are kept.
   *  @param l  Must be non-NULL. */
  void ratesync_loop_reset (ratesync_loop_t *l);

  /** @brief Retune the loop; preserves the integrator (and so the lock). */
  void ratesync_loop_configure (ratesync_loop_t *l, double bn, double zeta);

  /** @brief Set the lock detector's geometry; see
   *         ratesync_configure_lock_raw(), which forwards here. */
  void ratesync_loop_configure_lock_raw (ratesync_loop_t *l, size_t avgs,
                                         double up_thresh, double down_thresh,
                                         uint32_t n_up, uint32_t n_down);

  /** @brief Register the six timing probes; see ratesync_set_telemetry(),
   *         which forwards here. NULL @p tlm detaches. */
  int ratesync_loop_set_telemetry (ratesync_loop_t *l, dp_tlm_t *tlm,
                                   const char *prefix, uint32_t decim);

  /** @brief Emit the timing loop's telemetry for the symbol just recovered.
   *
   *  Out-of-line on purpose: the emit machinery must not inline into the
   *  per-sample hot loop (the same body-growth cost symsync measured).
   *  Callers gate on `l->tlm.ctx`, so the detached cost is one
   *  predicted-not-taken branch per symbol.
   *
   *  @param l  Loop with a non-NULL tlm.ctx (caller-checked). */
  void ratesync_loop_tlm_flush (const ratesync_loop_t *l);

/* ── Serializable state — the loop alone (nested by every owner) ───────────
 * Envelope, this loop's running scalars, then the loop filter's own
 * self-validating sub-blob. Config (sps/m/term_rate/prime_taps/bn/zeta/ted)
 * is restored by the owner's create() and never packed. */
#define RATESYNC_LOOP_STATE_MAGIC DP_FOURCC ('R', 'S', 'L', 'P')
#define RATESYNC_LOOP_STATE_VERSION 1u

  /** @brief Bytes ratesync_loop_get_state() writes (envelope + payload +
   *         the loop filter's child blob). */
  size_t ratesync_loop_state_bytes (const ratesync_loop_t *l);
  /** @brief Serialize the loop's mutable state into @p blob. */
  void ratesync_loop_get_state (const ratesync_loop_t *l, void *blob);
  /** @brief Restore the loop's mutable state from @p blob.
   *  @return DP_OK, or DP_ERR_INVALID if any envelope rejects. */
  int ratesync_loop_set_state (ratesync_loop_t *l, const void *blob);

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Create a RateSync instance.
   *
   * Builds a `RateConverter(rate = m/sps, pulse, ..., pulse_sps = m)` with CIC
   * droop compensation on — folded into the bank, so it costs six taps per arm
   * and no extra stage, and is worth ~28 dB of EVM on any cascade that plans a
   * CIC. See RateConverter_create_matched().
   *
   * @param sps         Nominal samples per symbol; any double >= @p m
   *                    (17.33389 is as valid as 4). The bound is `m`, not 2,
   *                    because the terminal stage must not be asked to
   *                    interpolate: `rate = m/sps <= 1`.
   * @param pulse       RATESYNC_PULSE_IANDD (rectangular/NRZ) or _RRC.
   * @param beta        RRC roll-off in `[0, 1]` (ignored for the rectangle).
   * @param span        One-sided RRC span in symbols (ignored for the
   *                    rectangle, whose support is always one symbol).
   * @param m           Terminal outputs per symbol: even, `2 <= m <=
   *                    RATESYNC_MAX_M`. Gardner needs the half-symbol gate,
   *                    so m must be even and at least 2. The oversampled
   *                    stream is a by-product, not an extra cost.
   *                    **Use m >= 4 with RATESYNC_PULSE_IANDD**: the
   *                    rectangle is one symbol wide, so at m = 2 its matched
   *                    filter is a two-tap sum and the eye barely opens
   *                    (measured lock_stat -0.34 at m = 2 against +0.95 at
   *                    m = 4 on the same NRZ stream). The RRC spans many
   *                    symbols and is unaffected.
   * @param num_phases  Matched-filter arms; power of two (1024 is a good
   *                    default). Sets the fractional-timing resolution to
   *                    `1/num_phases` of an output period.
   * @param bn          Loop noise bandwidth, normalised to the symbol rate.
   * @param zeta        Damping factor (0.707 = critically damped).
   * @param ted         RATESYNC_TED_GARDNER (blind) or RATESYNC_TED_DTTL
   *                    (decision-directed; BPSK/QPSK only).
   * @return Heap-allocated state, or NULL if a parameter is out of range or
   *         allocation fails.
   * @note Caller must call ratesync_destroy() when done.
   */
  ratesync_state_t *ratesync_create (double sps, int pulse, double beta,
                                     size_t span, size_t m, size_t num_phases,
                                     double bn, double zeta, int ted);

  /** @brief Destroy a RateSync instance and release all memory.
   *  @param state  May be NULL. */
  void ratesync_destroy (ratesync_state_t *state);

  /** @brief Reset to the post-create state: the cascade, the loop integrator,
   *         the lock detector, the strobe ring and the prime countdown.
   *  @param state  Must be non-NULL. */
  void ratesync_reset (ratesync_state_t *state);

  /* ------------------------------------------------------------------
   * Execute
   * ------------------------------------------------------------------ */

  /** @brief Fold one terminal-stage output into the timing loop.
   *
   *  The whole of the loop's per-output work, and the reason the loop is a
   *  struct of its own: it never touches the cascade, so a receiver that owns
   *  its cascade inside a DDC drives this with exactly the same call RateSync
   *  makes.
   *
   *  @param s      Loop state. Must be non-NULL.
   *  @param y      One terminal-stage output.
   *  @param y_out  Receives the symbol when the return is 1.
   *  @param ted    RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL — pass a literal
   *                for a specialised (branch-free) instantiation.
   *  @return 1 if this output was an on-time strobe that produced a symbol. */
  JM_FORCEINLINE JM_HOT int
  ratesync_loop_take_output (ratesync_loop_t *s, float complex y,
                             float complex *y_out, int ted)
  {
    /* Newest-first ring: ring[0] is this output, ring[m/2] the gate. */
    const size_t half = s->m >> 1;
    for (size_t i = s->ring_n < half ? s->ring_n : half; i > 0; i--)
      s->ring[i] = s->ring[i - 1];
    s->ring[0] = y;
    if (s->ring_n <= half)
      s->ring_n++;

    if (s->prime_left)
      {
        /* The cascade is still filling: these outputs are the delay lines,
           not the signal. Drop them without advancing the strobe phase, so
           the first real strobe starts a clean count. */
        s->prime_left--;
        return 0;
      }

    size_t phase = s->out_count++ % s->m;
    if (phase != 0 || s->ring_n <= half)
      return 0; /* not a strobe, or the gate is not yet in the ring */

    const float complex on  = y;
    const float complex mid = s->ring[half];
    if (!s->have_prev)
      {
        s->have_prev = 1;
        s->prev_on   = on;
        return 0;
      }

    double num;
    if (ted == RATESYNC_TED_DTTL)
      num = dttl_ted (mid, on, s->prev_on);
    else
      num = gardner_ted (mid, on - s->prev_on);

    double on_pwr
        = (double)(crealf (on) * crealf (on) + cimagf (on) * cimagf (on));
    double mid_pwr
        = (double)(crealf (mid) * crealf (mid) + cimagf (mid) * cimagf (mid));
    /* The normaliser is the SUM. |on|^2 alone vanishes exactly when the
       strobe sits on the transitions — the state the loop must recover from —
       and dividing by it there sends the control to infinity and the terminal
       stage's effective rate negative, which kills the cascade permanently.
       See the file header. */
    double ref = on_pwr + mid_pwr;
    if (!s->pwr_seeded)
      {
        s->pwr_avg    = ref;
        s->pwr_seeded = 1;
      }
    else
      s->pwr_avg += 0.01 * (ref - s->pwr_avg);

    double e      = num / (s->pwr_avg + RATESYNC_LOCK_EPS);
    s->last_error = e;

    /* loop_filter_step returns a correction in symbols per symbol; `ctrl` is
       a rate deviation the TERMINAL stage adds to its accumulator once per
       one of ITS OWN inputs — not once per cascade input. Those differ by the
       whole integer decimation in front, so scaling by the cascade rate m/sps
       under-drives the loop by exactly that factor (32x at sps=64 behind a
       CIC(32), which is why it could barely track).
         Over one symbol the terminal stage sees N = m/rate_term inputs, so
       the accumulator gains N*ctrl output periods = N*ctrl/m symbols; setting
       that equal to the requested correction gives ctrl = correction *
       rate_term, with no reference to sps or the decimation at all.
       e > 0 means the strobe is LATE and a positive ctrl advances it — the
       classic Gardner polarity. */
    s->ctrl = loop_filter_step (&s->lf, e) * s->term_rate;

    /* Tracked samples/symbol from the loop INTEGRATOR, not the instantaneous
       control. The integrator is the rate memory (loop_filter_core.h: "kp*e
       is the instantaneous phase nudge"); feeding the noisy total through the
       convex 1/(1+integ) instead biases the estimate high by Jensen. Exact
       here: a loop tracking true sps settles at integ = sps/true - 1, so
       sps/(1 + integ) == true. */
    double inst = s->sps / (1.0 + s->lf.integ);
    double lo_r = 0.5 * s->sps, hi_r = 1.5 * s->sps;
    if (!(inst > lo_r))
      inst = lo_r;
    else if (inst > hi_r)
      inst = hi_r;
    s->rate_est = inst;

    /* Lock statistic: the Gardner eye-opening ratio
       2*(|on|^2 - |mid|^2)/(|on|^2 + |mid|^2) — an open eye means the on-time
       strobe carries the energy and the transition gate sits near zero.
       Block-averaged over `avgs` looks before the decision (a tumbling
       window, so verify counts stay independent), mirroring symsync/dll. */
    double lock_signal = 2.0 * (on_pwr - mid_pwr) / (ref + RATESYNC_LOCK_EPS);
    s->lock_sum += lock_signal;
    if (++s->lock_count >= s->avgs)
      {
        s->lock_stat = s->lock_sum / (double)s->avgs;
        (void)lockdet_step (&s->lock, s->lock_stat);
        s->lock_sum   = 0.0;
        s->lock_count = 0;
      }

    s->prev_on = on;
    *y_out     = on;
    return 1;
  }

  /**
   * @brief Per-input timing step with the TED selection as a parameter.
   *
   * The workhorse behind ratesync_step()/ratesync_steps(). Pushes one input
   * through the cascade at the current control deviation. `rate = m/sps <= 1`
   * so the terminal stage emits at most one output per input; every m-th
   * output is an on-time strobe, and the output m/2 back is the transition
   * gate. On a strobe the TED compares the two, the PI loop steers the next
   * control, and the on-time sample is the recovered symbol.
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
  ratesync_step_ted (ratesync_state_t *s, float complex x,
                     float complex *y_out, int ted)
  {
    /* One input can complete MORE THAN ONE output period. It happens
       whenever the terminal stage's own rate is at or near 1.0 (a cascade
       like HB + Resampler(1.0), which is what an integer sps plans) and the
       control has pushed the accumulator over: that input emits two. Asking
       for only one silently DROPS the second, which permanently shifts the
       strobe parity and leaves the loop sliding — measured as `rate_est`
       walking monotonically away while the eye never opens. The cascade rate
       is m/sps <= 1, so an input can complete at most two output periods, and
       with m >= 2 those can contain at most one on-time strobe: the
       single-symbol return of this function is still correct. */
    float complex ys[4];
    size_t n = RateConverter_execute_ctrl_push (s->mf, x, s->loop.ctrl, ys,
                                                sizeof (ys) / sizeof (ys[0]));
    int    emitted = 0;
    for (size_t oi = 0; oi < n; oi++)
      emitted |= ratesync_loop_take_output (&s->loop, ys[oi], y_out, ted);
    return emitted;
  }

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
    int r = ratesync_step_ted (s, x, y_out, s->loop.ted);
    if (r && s->loop.tlm.ctx)
      ratesync_loop_tlm_flush (&s->loop);
    return r;
  }

  /** @brief Output-buffer hint for the generated binding; 0 means "the input
   *  length is already a safe bound" — with `sps >= m >= 2` a block can never
   *  yield more symbols than it has samples (mirrors symsync). */
  size_t ratesync_steps_max_out (ratesync_state_t *state);

  /**
   * @brief Recover symbols from a block of oversampled cf32 baseband.
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

  /** @brief Last block-averaged lock statistic (the eye-opening ratio).
   *
   *  This, not an error-vector magnitude, is the honest lock indicator: a
   *  single cycle slip during acquisition drags a windowed EVM by 20 dB while
   *  the eye stays wide open at +0.75. Judge lock here. */
  double ratesync_get_lock_stat (const ratesync_state_t *state);

  /** @brief Current lock decision (1 = locked), verify-counted. */
  int ratesync_get_locked (const ratesync_state_t *state);

  /** @brief Has the cascade's CIC stage clipped its input since the last
   *         reset? Forwarded from the RateConverter: a CIC bounds its input
   *         to +-1.0 and clips silently past that, which no timing metric
   *         reveals. Always 0 when the plan has no CIC stage. */
  int ratesync_get_clipped (const ratesync_state_t *state);

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
   * Registers six probes, emitted once per recovered symbol and further
   * thinned by @p decim: "<prefix>.e" (normalised TED error),
   * "<prefix>.ctrl" (the per-input control steering the strobe),
   * "<prefix>.rate" (tracked samples/symbol), "<prefix>.lock" (last
   * block-averaged lock_signal), "<prefix>.locked" (0/1) and "<prefix>.mu"
   * (the timing NCO's fractional phase — see resamp_get_ctrl_acc()). Passing
   * NULL detaches. Setup path, never hot: the context is borrowed and must
   * outlive the attachment (SPSC rules in telemetry/telemetry.h).
   *
   * The three form one readable picture of the loop: `e` is what the detector
   * saw, `ctrl` is what the filter did about it, and `mu` is where the
   * sampling instant ended up as a result — the only one of the three that is
   * a physical position rather than a correction.
   *
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "sync".
   * @param decim  Emit every decim-th symbol; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
   *         six probes (the attach fails whole; the object stays detached).
   */
  int ratesync_set_telemetry (ratesync_state_t *state, dp_tlm_t *tlm,
                              const char *prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ─────────
 * A composition of exactly two children now that the timing loop is its own
 * struct: `[hdr][RateConverter][ratesync_loop]`, each self-validating. All of
 * this object's own running state moved into the loop's blob, so it packs no
 * scalars of its own; config (sps/pulse/beta/span/m/num_phases/bn/zeta/ted) is
 * restored by create() and never packed. */
#define RATESYNC_STATE_MAGIC DP_FOURCC ('R', 'A', 'T', 'S')
#define RATESYNC_STATE_VERSION 2u /* v2: running state moved into the loop */

  /** @brief Bytes ratesync_get_state() writes (envelope + payload + child). */
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
