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
