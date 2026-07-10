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
#include "loop_filter/loop_filter_core.h"
#include "nco/nco_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Timing-error-detector selection for symsync_state_t::ted. */
  enum
  {
    SYMSYNC_TED_GARDNER = 0, /**< blind Gardner TED (mid * conj diff).    */
    SYMSYNC_TED_DTTL = 1     /**< decision-directed sign-sign DTTL.       */
  };

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
  symsync_step_ted (symsync_state_t *s, float complex x,
                    float complex *y_out, int ted)
  {
    const uint32_t HALF = 0x80000000u;
    farrow_push (&s->farrow, x);
    uint32_t old   = s->timing.phase;
    uint64_t sum   = (uint64_t)old + s->timing.phase_inc;
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
    float         mu = (float)(1.0 - (double)s->timing.phase / inc);
    float complex y  = farrow_eval (&s->farrow, mu);
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
        double e          = num / (s->pwr_avg + 1e-6);
        s->last_error     = e;
        double control    = loop_filter_step (&s->lf, e);
        s->timing.phase_inc
            = (uint32_t)((double)s->base_inc * (1.0 + control));
        double inst = (double)s->sps / (1.0 + control);
        double lo_r = 0.5 * (double)s->sps, hi_r = 1.5 * (double)s->sps;
        if (inst < lo_r)
          inst = lo_r;
        else if (inst > hi_r)
          inst = hi_r;
        s->rate_est += 0.02 * (inst - s->rate_est);
        *y_out = y;
        emit   = 1;
      }
    else
      s->have_ontime = 1;
    s->prev_ontime = y;
    return emit;
  }

  /**
   * @brief Per-sample symbol-timing step (the inline composition API).
   *
   * The public form of symsync_step_ted(): dispatches on the state's
   * configured detector (`s->ted`). symsync_steps() is this in a loop
   * (with the TED specialised per detector); a tracking channel inlines
   * it to drive a downstream carrier loop on the recovered symbols.
   *
   * @param s      State.  Must be non-NULL.
   * @param x      One input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  symsync_step (symsync_state_t *s, float complex x, float complex *y_out)
  {
    return symsync_step_ted (s, x, y_out, s->ted);
  }

  /**
   * @brief Initialise a SymbolSync in place (no allocation).
   *
   * The by-value counterpart to symsync_create(): lets a composing object embed
   * a symsync_state_t by value and initialise it without a heap allocation
   * (symsync_state_t holds no heap members — the NCO, Farrow and loop filter are
   * all by value). Mirrors loop_filter_init()/costas_init().
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
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * pointer-free composition: nco + farrow + loop_filter embedded by value
 * (all POD) + scalar timing state — a whole-struct snapshot. */
#define SYMSYNC_STATE_MAGIC DP_FOURCC ('S','Y','N','C')
#define SYMSYNC_STATE_VERSION 2u /* v2: ted */
size_t symsync_state_bytes (const symsync_state_t *state);
void symsync_get_state (const symsync_state_t *state, void *blob);
int symsync_set_state (symsync_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* SYMSYNC_CORE_H */
