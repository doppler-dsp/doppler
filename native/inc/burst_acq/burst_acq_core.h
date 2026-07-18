/**
 * @file burst_acq_core.h
 * @brief BurstAcquisition — thin forwarder onto acq_core.c's shared engine.
 *
 * Composes acq_state_t (native/inc/acq/acq_core.h) as an embedded pointer,
 * built via acq_create_burst() -- the BURST front door onto the SAME shared
 * engine `Acquisition` (acq_core.h) composes via acq_create_continuous().
 * Every function here is a direct forward to the corresponding acq_* call;
 * the entire algorithm lives in acq_core.c exactly once (see
 * prototypes/async_despreader/SPEC.md's Acquisition/BurstAcquisition split
 * and CLAUDE.md's "every algorithm lives in C exactly once" rule).
 *
 * @code
 * uint8_t code[7] = { 1, 1, 1, 0, 1, 0, 0 };
 * burst_acq_state_t *obj = burst_acq_create(code, 7, 8, 4, 1000000.0, 50.0,
 *                                           0.0, 1e-3, 0.9, 0);
 * acq_result_t hits[64];
 * size_t nh = burst_acq_push(obj, samples, n_samples, hits, 64);
 * burst_acq_destroy(obj);
 * @endcode
 */
#ifndef BURST_ACQ_CORE_H
#define BURST_ACQ_CORE_H

#include "acq/acq_core.h"
#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief BurstAcquisition state: a pure wrapper around one shared
   *        acq_state_t engine.
   *
   * Allocate with burst_acq_create(); every other function forwards
   * straight to the corresponding acq_* call on `engine`.
   */
  typedef struct
  {
    acq_state_t *engine;
  } burst_acq_state_t;

  /**
   * @brief Create a burst-mode acquisition engine (forwards to
   *        acq_create_burst() -- see its doc comment in acq_core.h for the
   *        full physics).
   *
   * @param code  PN chips (0/1), length @p code_len.
   * @param code_len  Number of chips supplied (= sf).
   * @param reps  Max coherent code repetitions (>= 1).
   * @param spc  Samples per chip (>= 1).
   * @param chip_rate  Chip rate in Hz (> 0).
   * @param cn0_dbhz  Carrier-to-noise density in dB-Hz (> 0).
   * @param doppler_uncertainty  One-sided Doppler search half-range in Hz.
   * @param pfa  Target system false-alarm probability (0,1).
   * @param pd  Target detection probability (0,1).
   * @param noise_mode  CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
   * @return Heap-allocated state, or NULL on bad arguments / allocation
   *         failure.
   */
  burst_acq_state_t *burst_acq_create (const uint8_t *code, size_t code_len,
                                       size_t reps, size_t spc,
                                       double chip_rate, double cn0_dbhz,
                                       double doppler_uncertainty, double pfa,
                                       double pd, int noise_mode);

  /** @brief Destroy and free an instance.  @param state May be NULL. */
  void burst_acq_destroy (burst_acq_state_t *state);

  /** @brief Drain the input ring and reset the coherent accumulator.
   *  @param state Must be non-NULL. */
  void burst_acq_reset (burst_acq_state_t *state);

  /** @brief Stream raw samples; emit one event per CFAR dump above
   *         threshold. Forwards to acq_push() -- see its doc comment. */
  size_t burst_acq_push (burst_acq_state_t *state, const float complex *in,
                         size_t n_in, acq_result_t *result,
                         size_t max_results);

  /** @brief Pin the search grid directly. Forwards to
   *         acq_configure_search_raw() -- see its doc comment. */
  int burst_acq_configure_search_raw (burst_acq_state_t *state,
                                      size_t doppler_bins, size_t n_noncoh);

  /* ── Serializable state — forwards straight to the embedded engine's own
   * triplet (the serialized bytes ARE the shared acq_state_t's own state;
   * no separate format needed). */

  size_t burst_acq_state_bytes (const burst_acq_state_t *state);
  void   burst_acq_get_state (const burst_acq_state_t *state, void *blob);
  int    burst_acq_set_state (burst_acq_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* BURST_ACQ_CORE_H */
