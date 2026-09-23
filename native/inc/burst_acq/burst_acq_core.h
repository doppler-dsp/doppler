/**
 * @file burst_acq_core.h
 * @brief BurstAcquisition — thin forwarder onto acq_core.c's shared engine.
 *
 * Composes acq_state_t (native/inc/acq/acq_core.h) as an embedded pointer,
 * built via acq_create_burst() -- the BURST front door onto the SAME shared
 * engine `Acquisition` (acq_core.h) composes via acq_create_continuous().
 * Every function here is a direct forward to the corresponding acq_* call;
 * the entire algorithm lives in acq_core.c exactly once (see
 * docs/design/async-dsss-receiver.md's Acquisition/BurstAcquisition split
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
    /** The engine's `underpowered` at construction -- a field of THIS
        struct because a declared jm warning's condition must be one; it is
        what raises the under-powered UserWarning after __init__. */
    uint8_t underpowered;
  } burst_acq_state_t;

  /**
   * @brief Create a burst-mode acquisition engine for a PN code (forwards
   *        to acq_create_burst() -- see its doc comment in acq_core.h for
   *        the full physics).
   *
   * @param code  PN chips (0/1), length @p code_len.
   * @param code_len  Number of chips supplied (= sf).
   * @param reps  Max coherent code repetitions (>= 1).
   * @param spc  Samples per chip (>= 1).
   * @param chip_rate  Chip rate in Hz (> 0).
   * @param cn0_dbhz  Carrier-to-noise density in dB-Hz: any finite value,
   *                  or NaN (ACQ_CN0_NONE) for no design point.
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

  /**
   * @brief Create a burst-mode acquisition engine for ANY repeated complex
   *        preamble -- a chirp, a Zadoff-Chu sequence, shaped PSK -- by its
   *        samples (forwards to acq_create_burst_template(); doppler#1470).
   *
   * One chip is one sample: `sf = n`, `spc = 1`, `chip_rate = fs`, and
   * `code_phase` is the delay into the repetition in samples.
   *
   * @param tmpl  One period of the preamble, @p n samples.
   * @param n  Samples per repetition (>= 1).
   * @param reps  Max coherent repetitions (>= 1).
   * @param fs  Sample rate in Hz (> 0); 1 for normalized units.
   * @param cn0_dbhz  Design C/N0 of the preamble's mean power, dB-Hz.
   * @param doppler_uncertainty  One-sided Doppler search half-range in Hz.
   * @param pfa  Target system false-alarm probability (0,1).
   * @param pd  Target detection probability (0,1).
   * @param noise_mode  CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
   * @return Heap-allocated state, or NULL on bad arguments / allocation
   *         failure.
   */
  burst_acq_state_t *burst_acq_create_template (
      const float _Complex *tmpl, size_t n, size_t reps, double fs,
      double cn0_dbhz, double doppler_uncertainty, double pfa, double pd,
      int noise_mode);

  /**
   * @brief Build a BurstAcquisition from a PN code OR a preamble's samples
   *        -- the one Python constructor, dispatched on the first array's
   *        dtype.
   *
   * A `uint8` code (chips 0/1) searches the code held @p spc samples per
   * chip at @p chip_rate, and @p fs is ignored. A `complex64` preamble is
   * searched by its own samples at @p fs, and @p spc and @p chip_rate are
   * ignored. @p fs defaults to 1: normalized units, where Doppler is in
   * cycles/sample and @p cn0_dbhz is the per-sample SNR in dB.
   *
   * This and burst_acq_bind_template() share one argument list because
   * jm's dtype dispatch calls both branches with the same arguments; C
   * callers want burst_acq_create() / burst_acq_create_template().
   *
   * @param code  The preamble: PN chips (uint8, 0/1) or its samples
   *              (complex64), one period.
   * @param code_len  Its length (chips, or samples).
   * @param reps  Max coherent repetitions (>= 1).
   * @param spc  Samples per chip (>= 1); a code only.
   * @param chip_rate  Chip rate in Hz (> 0); a code only.
   * @param cn0_dbhz  Design carrier-to-noise density in dB-Hz: any finite
   *                  value, or NaN (ACQ_CN0_NONE) for no design point --
   *                  size for the whole preamble.
   * @param doppler_uncertainty  One-sided Doppler search half-range in Hz.
   * @param pfa  Target system false-alarm probability (0,1).
   * @param pd  Target detection probability (0,1).
   * @param noise_mode  CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
   * @param fs  Sample rate in Hz (> 0); a preamble's samples only.
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
   * The same object searches any repeated preamble by its samples -- here
   * a 127-sample Zadoff-Chu sequence, in normalized units:
   *
   * >>> k = np.arange(127)
   * >>> zc = np.exp(-1j * np.pi * 5 * k * (k + 1) / 127).astype(
   * ...     np.complex64)
   * >>> z = BurstAcquisition(zc, reps=8)
   * >>> z.sf, z.spc                # one chip is one sample
   * (127, 1)
   * >>> z.push(np.tile(np.roll(zc, 40), 10))[0][:2]
   * (0, 40)
   *
   * @endcode
   */
  burst_acq_state_t *burst_acq_bind_code (const uint8_t *code,
                                          size_t code_len, size_t reps,
                                          size_t spc, double chip_rate,
                                          double cn0_dbhz,
                                          double doppler_uncertainty,
                                          double pfa, double pd,
                                          int noise_mode, double fs);

  /**
   * @brief The complex64 branch of burst_acq_bind_code(): forwards to
   *        burst_acq_create_template(), ignoring @p spc and @p chip_rate.
   *
   * @param tmpl  One period of the preamble, @p n samples.
   * @param n  Samples per repetition.
   * @param reps  Max coherent repetitions.
   * @param spc  Ignored (a code's argument).
   * @param chip_rate  Ignored (a code's argument).
   * @param cn0_dbhz  Design C/N0, dB-Hz.
   * @param doppler_uncertainty  Doppler search half-range in Hz.
   * @param pfa  Target system false-alarm probability.
   * @param pd  Target detection probability.
   * @param noise_mode  CFAR mode index.
   * @param fs  Sample rate in Hz.
   * @return Heap-allocated state, or NULL.
   */
  burst_acq_state_t *burst_acq_bind_template (
      const float _Complex *tmpl, size_t n, size_t reps, size_t spc,
      double chip_rate, double cn0_dbhz, double doppler_uncertainty,
      double pfa, double pd, int noise_mode, double fs);

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
  size_t burst_acq_push (burst_acq_state_t *state, const float _Complex *x,
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

  /**
   * @brief How many peaks a dwell may report: the peak list's capacity.
   *
   * Forwards to acq_set_max_peaks() on the embedded engine (see its doc
   * comment in acq_core.h): one is the classic gated maximum; more is the
   * list of docs/design/async-dsss-receiver.md §7.1 -- every peak above the
   * same gate, strongest first, an exclusion zone of one Doppler bin by one
   * chip around each, and the two-epoch rule for a peak at an
   * already-listed code phase. Each listed peak is one result from push().
   *
   * @param state  Allocated engine (non-NULL).
   * @param n      1 … ACQ_MAX_PEAKS.
   * @return 0, or -1 (engine untouched) when @p n is out of range.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstAcquisition
   * >>> code = (np.arange(31) * 5 % 2).astype(np.uint8)
   * >>> b = BurstAcquisition(code, reps=8, spc=4, chip_rate=1e6,
   * ...                      cn0_dbhz=50.0)
   * >>> b.set_max_peaks(4)
   * >>> b.max_peaks
   * 4
   * @endcode
   */
  int burst_acq_set_max_peaks (burst_acq_state_t *state, size_t n);

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
