/**
 * @file symsync_core.h
 * @brief SymbolSync component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * symsync_state_t *obj = symsync_create(4, 0.01, 0.707, 0);
 * float complex y = symsync_step(obj, 0.0f + 0.0f * I);
 * symsync_destroy(obj);
 * @endcode
 */
#ifndef SYMSYNC_CORE_H
#define SYMSYNC_CORE_H

#include "clib_common.h"
#include "farrow/farrow_core.h"
#include "jm_perf.h"
#include "loop_filter/loop_filter_core.h"
#include "nco/nco_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief SymbolSync state.
   *
   * Allocate with symsync_create().  Embeds the integer timing NCO, the Farrow
   * interpolator and the PI loop filter by value; treat the Gardner history as
   * internal.
   */
  typedef struct
  {
    nco_state_t         timing; /**< integer timing NCO (phase/phase_inc).  */
    farrow_state_t      farrow; /**< fractional interpolator.               */
    loop_filter_state_t lf;     /**< 2nd-order timing PI loop.              */
    size_t              sps;    /**< nominal samples per symbol.            */
    uint32_t      base_inc;     /**< nominal NCO inc (one wrap / symbol).   */
    int           have_ontime;  /**< a previous on-time sample exists.      */
    float complex prev_ontime;  /**< previous on-time interpolant.          */
    float complex mid;          /**< mid-symbol interpolant (Gardner).      */
    double        bn;           /**< loop noise bandwidth (retained).       */
    double        zeta;         /**< damping factor (retained).             */
    double        last_error;   /**< last Gardner timing error.             */
    double        rate_est;     /**< smoothed tracked samples/symbol.       */
    double        pwr_avg;      /**< running symbol power (TED normaliser).  */
  } symsync_state_t;

  /**
   * @brief Per-sample symbol-timing step (the inline composition API).
   *
   * Pushes one input sample into the Farrow history and advances the integer
   * timing NCO. When the NCO crosses its half-scale (mid-symbol) it stores the
   * Gardner mid interpolant; when it wraps (on-time) it forms the on-time
   * interpolant, runs the Gardner TED, steers the NCO frequency, and emits the
   * timing-corrected symbol. Returns 1 and writes @p y_out on an on-time symbol,
   * else 0. symsync_steps() is exactly this in a loop; a tracking channel inlines
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
        float complex diff = y - s->prev_ontime;
        double        num  = (double)(crealf (s->mid) * crealf (diff)
                                      + cimagf (s->mid) * cimagf (diff));
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
   * @brief Create a symsync instance.
   *
   * @param sps  sps (default: 4).
   * @param bn  bn (default: 0.01).
   * @param zeta  zeta (default: 0.707).
   * @param order  Enum index; 0=linear…2=cubic.
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note Caller must call symsync_destroy() when done.
   */
  symsync_state_t *symsync_create (size_t sps, double bn, double zeta,
                                   int order);

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
#ifdef __cplusplus
}
#endif

#endif /* SYMSYNC_CORE_H */
