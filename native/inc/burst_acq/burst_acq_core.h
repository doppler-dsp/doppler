/**
 * @file burst_acq_core.h
 * @brief BurstAcquisition — thin forwarder onto acq_core.c's shared engine.
 *
 * Composes acq_state_t (native/inc/acq/acq_core.h) as an embedded pointer,
 * built via acq_create_burst() -- the BURST front door onto the SAME shared
 * engine `Acquisition` (acq_core.h) composes via acq_create_continuous().
 * Every function here is a direct forward to the corresponding acq_* call;
 * the entire algorithm lives in acq_core.c exactly once (see
 * docs/design/async-dsss-spec.md's Acquisition/BurstAcquisition split
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
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstAcquisition
   * >>> from doppler.wfm import PN, mls_poly
   * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
   * ...                      length=5).generate(31)).astype(np.uint8)
   * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
   * ...     np.complex64)
   * >>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
   * >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
   * ...                      cn0_dbhz=50.0)
   * >>> b.push(burst)[0][:2]      # detects (Doppler bin, code phase)
   * (0, 17)
   *
   * @endcode
   */
  burst_acq_state_t *burst_acq_create (const uint8_t *code, size_t code_len,
                                       size_t reps, size_t spc,
                                       double chip_rate, double cn0_dbhz,
                                       double doppler_uncertainty, double pfa,
                                       double pd, int noise_mode);

  /** @brief Destroy and free an instance.  @param state May be NULL. */
  void burst_acq_destroy (burst_acq_state_t *state);

  /**
   * @brief Drain the input ring and reset the coherent accumulator.
   *
   * Forwards to acq_reset() on the embedded engine: discards any buffered
   * samples that have not yet completed a frame and clears the non-coherent
   * power accumulator and dwell bookkeeping, so the next push() begins a
   * fresh search from an empty ring.  Construction parameters are untouched.
   *
   * @param state Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstAcquisition
   * >>> from doppler.wfm import PN, mls_poly
   * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
   * ...                      length=5).generate(31)).astype(np.uint8)
   * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
   * ...     np.complex64)
   * >>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
   * >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
   * ...                      cn0_dbhz=50.0)
   * >>> _ = b.push(burst[:100])   # a partial frame, buffered mid-stream
   * >>> b.reset()                 # drop it before it can bias a detection
   * >>> b.push(burst)[0][:2]      # (Doppler bin, code phase)
   * (0, 17)
   *
   * @endcode
   */
  void burst_acq_reset (burst_acq_state_t *state);

  /**
   * @brief Stream raw samples; emit one event per CFAR dump above threshold.
   *
   * Forwards to acq_push() on the embedded engine (see its doc comment in
   * acq_core.h for the framing/CFAR mechanics).  Each event carries the
   * peak's Doppler bin and code phase (the two search axes), its CFAR
   * statistic, and an estimated C/N0 — see @ref acq_result_t.
   *
   * @param state        Allocated engine (non-NULL).
   * @param x            Raw input, interleaved CF32, @p n_in complex samples.
   * @param n_in         Number of complex input samples.
   * @param result       Output array for detection events.
   * @param max_results  Capacity of @p result.
   * @return Number of events written (0 … max_results).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstAcquisition
   * >>> from doppler.wfm import PN, mls_poly
   * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
   * ...                      length=5).generate(31)).astype(np.uint8)
   * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
   * ...     np.complex64)
   * >>> burst = np.tile(np.roll(s0, 17), 24).astype(np.complex64)
   * >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
   * ...                      cn0_dbhz=50.0)
   * >>> b.push(burst)[0][:2]      # (Doppler bin, code phase)
   * (0, 17)
   *
   * @endcode
   */
  size_t burst_acq_push (burst_acq_state_t *state, const float complex *x,
                         size_t n_in, acq_result_t *result,
                         size_t max_results);

  /**
   * @brief Pin the search grid directly, bypassing the auto-sizing search.
   *
   * Forwards to acq_configure_search_raw() on the embedded engine (see its
   * doc comment in acq_core.h): resizes every grid-dependent buffer/plan,
   * re-derives the threshold ladder for the pinned grid, and clears in-flight
   * accumulation — call between push() calls, never a substitute for one.
   *
   * @param state        Allocated engine (non-NULL).
   * @param doppler_bins Coherent depth to pin, in `[1, reps]`.
   * @param n_noncoh     Non-coherent look count to pin, in
   *                     `[1, ACQ_N_NONCOH_SAFETY_CEILING]`.
   * @return 0 on success, -1 if either argument is out of range or an
   *         allocation fails (the engine keeps its prior grid on failure).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstAcquisition
   * >>> from doppler.wfm import PN, mls_poly
   * >>> code = np.asarray(PN(poly=mls_poly(5), seed=1,
   * ...                      length=5).generate(31)).astype(np.uint8)
   * >>> s0 = np.repeat(np.where(code & 1, -1.0, 1.0), 4).astype(
   * ...     np.complex64)
   * >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
   * ...                      cn0_dbhz=50.0)
   * >>> b.configure_search_raw(doppler_bins=4, n_noncoh=2)  # pin the grid
   * >>> b.doppler_bins, b.n_noncoh
   * (4, 2)
   * >>> burst = np.tile(np.roll(s0, 17), 8).astype(np.complex64)
   * >>> b.push(burst)[0][:2]      # detects at the pinned grid
   * (0, 17)
   *
   * @endcode
   */
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
