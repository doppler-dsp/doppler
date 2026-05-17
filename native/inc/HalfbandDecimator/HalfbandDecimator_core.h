/**
 * @file HalfbandDecimator_core.h
 * @brief Halfband 2:1 decimator for CF32 IQ (adapter over hbdecim_core).
 *
 * Thin adapter over hbdecim_state_t.  The caller supplies a FIR coefficient
 * array at construction; use resample.build_bank(2, ...) from Python to
 * generate a suitable halfband prototype.
 *
 * Lifecycle:
 * @code
 *   float h[] = { ... };  // num_taps FIR branch coefficients
 *   HalfbandDecimator_state_t *r =
 *       HalfbandDecimator_create(num_taps, h);
 *   float complex out[512];
 *   size_t n = HalfbandDecimator_execute(r, in, 1024, out);
 *   HalfbandDecimator_destroy(r);
 * @endcode
 */
#ifndef HALFBANDDECIMATOR_CORE_H
#define HALFBANDDECIMATOR_CORE_H

#include "hbdecim/hbdecim_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef hbdecim_state_t HalfbandDecimator_state_t;

/* Maximum output samples per call (pre-allocated by ext.c at init). */
#define HBDECIM_MAX_OUT 32768

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Create a HalfbandDecimator.
   *
   * @param num_taps  FIR branch length.
   * @param h         FIR branch coefficients, length num_taps.
   * @return Non-NULL on success, NULL on invalid args or OOM.
   */
  HalfbandDecimator_state_t *HalfbandDecimator_create (size_t num_taps,
                                                       const float *h);

  /** Free all resources.  NULL is a no-op. */
  void HalfbandDecimator_destroy (HalfbandDecimator_state_t *state);

  /** Zero delay lines.  Coefficients preserved. */
  void HalfbandDecimator_reset (HalfbandDecimator_state_t *state);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  /** Always returns HBDECIM_MAX_OUT. */
  size_t HalfbandDecimator_execute_max_out (HalfbandDecimator_state_t *state);

  /**
   * @brief Decimate x(0..x_len-1) by 2 into out(0..n_out-1).
   *
   * out must be at least HalfbandDecimator_execute_max_out() samples.
   * Returns actual output count (roughly x_len / 2).
   */
  size_t HalfbandDecimator_execute (HalfbandDecimator_state_t *state,
                                    const float complex *x, size_t x_len,
                                    float complex *out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  /** Always returns 0.5. */
  double HalfbandDecimator_get_rate (const HalfbandDecimator_state_t *state);

  /** Returns the FIR branch length passed to create. */
  size_t
  HalfbandDecimator_get_num_taps (const HalfbandDecimator_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* HALFBANDDECIMATOR_CORE_H */
