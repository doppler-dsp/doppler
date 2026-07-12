/**
 * @file acq_core.h
 * @brief Streaming DSSS burst-acquisition engine.
 *
 * Acquires a direct-sequence spread-spectrum burst — a run of repeated,
 * BPSK-modulated PN-code segments — arriving with an unknown integer code
 * phase and an unknown carrier-frequency (Doppler) offset, buried in AWGN.
 * It jointly estimates the (Doppler bin, code phase) of the burst and declares
 * a detection whenever the CFAR test statistic crosses an automatically
 * configured threshold.
 *
 * Pipeline (owned end to end, one object):
 *   push(raw cf32) -> ring buffer -> reframe to (doppler_bins, code_bins) ->
 *   slow-time Doppler FFT (FFT along the segment axis) -> 2-D code correlation
 *   against a single-row PN reference (corr2d) -> argmax + CFAR noise estimate
 *   -> threshold gate -> acq_result_t.
 *
 * The fast-time axis (code_bins = sf*spc columns) is the circular code matched
 * filter; the slow-time axis (doppler_bins rows, one row per code repetition)
 * is the Doppler search.  A carrier offset f (cycles/sample) lands the peak at
 * row = round(f*code_bins*doppler_bins) mod doppler_bins, column = code phase.
 *
 * Physics-only construction: the user gives the PN @p code, the front-end
 * geometry (@p reps, @p spc, @p chip_rate), the sensitivity (@p cn0_dbhz), and
 * the detection targets (@p pfa, @p pd, optional @p doppler_uncertainty).  The
 * engine converts C/N0 to a per-sample amplitude SNR
 * (snr = sqrt(10^(cn0_dbhz/10) / (chip_rate*spc))) and picks the *smallest*
 * coherent depth doppler_bins in `[1, reps]` whose doppler_bins*code_bins
 * coherent samples meet @p pd (det_threshold / det_pd) — minimum latency for a
 * strong signal.  A tighter @p doppler_uncertainty shrinks the searched cell
 * count, lowering the Bonferroni threshold (more sensitive).
 *
 * @code
 * // 31-chip PN, 4x oversample, up to 16 coherent reps; 1 MHz chips, 45 dB-Hz
 * uint8_t code[31] = { 0 };   // ... fill with PN chips (0/1) ...
 * acq_state_t *a = acq_create(code, 31, 16, 4, 1.0e6, 45.0,
 *                             0.0, 1e-3, 0.9, 0, 1);
 * acq_result_t hits[64];
 * size_t nh = acq_push(a, samples, n_samples, hits, 64);
 * for (size_t i = 0; i < nh; i++)
 *   printf("Doppler %zu, code phase %zu, SNR %.2f\n",
 *          hits[i].doppler_bin, hits[i].code_phase, hits[i].snr_est);
 * acq_destroy(a);
 * @endcode
 */
#ifndef ACQ_CORE_H
#define ACQ_CORE_H

#include "buffer/buffer.h"
#include "clib_common.h"
#include "dp_state.h"
#include "corr2d/corr2d_core.h"
#include "detection/detection_core.h"
#include "fft/fft_core.h"
#include "jm_perf.h"
/* detector2d_core.h supplies det_noise_mode_t (guarded typedef). */
#include "detector2d/detector2d_core.h"
#include "fft2d/fft2d_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief One acquisition detection event.
   */
  typedef struct
  {
    size_t doppler_bin; /**< Peak row: Doppler bin (0 … doppler_bins-1). */
    size_t code_phase; /**< Peak col: code phase (0 … code_bins-1).          */
    float  peak_mag;   /**< max `|R[i,j]|` over the surface (linear).        */
    float  noise_est;  /**< CFAR noise estimate over `[noise_lo, noise_hi]`. */
    float  test_stat;  /**< peak_mag / noise_est; 0 if noise_est == 0.       */
    float  snr_est;    /**< Estimated per-sample amplitude SNR of the burst. */
  } acq_result_t;

  /**
   * @brief Streaming acquisition-engine state.
   *
   * Allocate with acq_create(); never stack-allocate.
   */
  typedef struct
  {
    corr2d_state_t *corr; /**< Single-row-ref correlator (dwell=1).          */
    fft_state_t *slow_fft; /**< Length-doppler_bins forward FFT (slow time). */
    dp_f32_t      *ring; /**< Raw cf32 input ring (the only ring).          */
    float complex *ref;  /**< Single-row reference (n), owned.              */
    float complex *yframe;  /**< Slow-time-FFT'd frame (n) fed to corr.  */
    float complex *colbuf;  /**< Gathered column scratch (doppler_bins).  */
    float complex *colout;  /**< FFT'd column scratch (doppler_bins).  */
    float complex *out_buf; /**< corr2d dump output (n). */
    float *mag_buf;       /**< |out_buf| (n).                                */
    float *noise_scratch; /**< Scratch for the median sort (n).              */
    float *nc_surface;    /**< Non-coherent |·|² accumulator (n); NULL
                               unless n_noncoh > 1.                          */

    size_t
        doppler_bins; /**< Coherent depth = slow-time FFT length (<= reps). */
    size_t code_bins; /**< One segment in samples = sf*spc.                 */
    size_t n;         /**< doppler_bins * code_bins — frame size in samples.*/
    size_t sf;        /**< Chips per PN segment (= len(code)).              */
    size_t spc;       /**< Samples per chip (chip-rate oversample factor).  */
    size_t reps;      /**< Max coherent code repetitions (the ceiling).     */
    size_t
        searched_bins; /**< Doppler bins scanned (<= doppler_bins; du prior).*/
    size_t n_noncoh;   /**< Non-coherent looks per detection (1 = coherent). */
    size_t max_noncoh; /**< Cap on the auto-split non-coherent look count.   */
    size_t nc_count; /**< Coherent dumps in the current look (0…n_noncoh-1).*/
    size_t ring_cap; /**< Ring capacity in complex samples.                */
    size_t noise_lo; /**< First CFAR reference bin (inclusive).            */
    size_t noise_hi; /**< Last CFAR reference bin (inclusive).             */
    det_noise_mode_t noise_mode; /**< CFAR aggregation mode. */

    double chip_rate; /**< Chip rate (Hz).                               */
    double fs;        /**< Sample rate (Hz) = chip_rate * spc.           */
    double cn0_dbhz;  /**< Sensitivity used to size the search (dB-Hz).  */
    double
        doppler_span_hz; /**< Native Doppler half-range = chip_rate/(2*sf). */
    double
        doppler_res_hz; /**< Doppler bin width = chip_rate/(sf*doppler_bins).*/

    float  threshold; /**< CFAR gate on test_stat (theta); coherent path.   */
    float  eta;       /**< Raw per-cell Rayleigh amplitude threshold.       */
    float  eta_nc;    /**< Non-coherent CFAR threshold (order-N_nc Marcum).  */
    double pfa_cell;  /**< Bonferroni per-cell false-alarm probability.     */
    double pd;        /**< Target detection probability.                    */
    double pd_predicted;  /**< Predicted Pd at cn0_dbhz and the chosen
                               grid: the AVERAGE Pd over the straddle
                               priors (slow-time scalloping, intra-segment
                               rotation, code sample offset — quadrature
                               over uniform priors), not the on-grid best
                               case, and not Pd at the mean amplitude
                               (which Jensen makes optimistic). */
    double straddle_loss; /**< Mean AMPLITUDE derating from grid straddle —
                               a diagnostic summary (~20*log10 of it in dB);
                               sizing and pd_predicted average Pd itself
                               over the priors. Derived config, recomputed
                               by create(). */
    uint8_t underpowered; /**< 1 when pd_predicted < pd. */

    uint64_t
        samples_consumed; /**< Total framed samples (the state's offset).   */

    /* Last-dump bookkeeping (for inspection). */
    size_t peak_row;
    size_t peak_col;
    float  peak_mag;
    float  noise_est;
    float  test_stat;
  } acq_state_t;

  /**
   * @brief Per-object extra header for an engine's cross-call state.
   *
   * The state blob is the *only* thing a fresh engine needs to continue a
   * stream from `(descriptor, state, input)` — it makes the engine a pure
   * transducer for the elastic fan-out (thread / process / pod).  Standard
   * bytes interface (see dp_state.h); layout, contiguous and flat:
   *
   *   `[ dp_state_hdr_t ] [ acq_extra_t ]`
   *   `[ float complex unconsumed[n_unconsumed] ]`   (partial frame, < n samples)
   *   `[ float          nc_surface[n] ]`             (only when n_noncoh > 1)
   *
   * Build the byte buffer with acq_state_bytes(); set_state validates the
   * envelope (magic/version/size) plus n / n_noncoh below, rejecting a mismatch
   * rather than reinterpreting it.
   */
  typedef struct
  {
    uint16_t has_nc;  /**< 1 if `nc_surface[n]` follows the samples.     */
    uint16_t _pad;
    uint32_t n_noncoh;         /**< Non-coherent looks (consistency).     */
    uint64_t n;                /**< Frame size; must equal engine's n.    */
    uint64_t samples_consumed; /**< Stream offset framed so far.          */
    uint32_t nc_count;     /**< Looks accumulated in the current dump.    */
    uint32_t n_unconsumed; /**< Partial-frame samples that follow (< n).  */
  } acq_extra_t;

#define ACQ_STATE_MAGIC DP_FOURCC ('A', 'C', 'Q', 'R')
#define ACQ_STATE_VERSION 1u

  /**
   * @brief Create a streaming DSSS acquisition engine.
   *
   * Builds the single-row oversampled BPSK reference from @p code, infers
   * sf = @p code_len, converts @p cn0_dbhz to a per-sample amplitude SNR
   * (snr = sqrt(10^(cn0_dbhz/10) / (chip_rate*spc))), and auto-configures the
   * search grid: the smallest coherent depth doppler_bins in `[1, reps]`
   * whose doppler_bins*code_bins coherent samples meet @p pd at the Bonferroni
   * threshold, plus non-coherent looks (up to @p max_noncoh) if the coherent
   * depth alone falls short.  A tighter @p doppler_uncertainty narrows the
   * scanned Doppler band, lowering the per-cell threshold (more sensitive).
   *
   * @param code        PN chips (0/1), length @p code_len.
   * @param code_len    Number of chips supplied (= sf, the spreading factor).
   * @param reps        Max coherent code repetitions, the coherence ceiling
   * (>=1).
   * @param spc         Samples per chip (>= 1).
   * @param chip_rate   Chip rate in Hz (> 0).
   * @param cn0_dbhz    Carrier-to-noise density in dB-Hz (> 0).
   * @param doppler_uncertainty  One-sided Doppler search half-range in Hz; 0
   * uses the full native span +/- chip_rate/(2*sf).  Must be <= span.
   * @param pfa         Target system (max-of-N) false-alarm probability (0,1).
   * @param pd          Target detection probability (0,1).
   * @param noise_mode  CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
   * @param max_noncoh  Cap on the auto-split non-coherent look count (>= 1;
   *                    default 1 keeps the engine purely coherent).
   * @return Heap-allocated state, or NULL on bad arguments / allocation
   * failure.
   */
  acq_state_t *acq_create (const uint8_t *code, size_t code_len, size_t reps,
                           size_t spc, double chip_rate, double cn0_dbhz,
                           double doppler_uncertainty, double pfa, double pd,
                           int noise_mode, size_t max_noncoh);

  /** @brief Destroy and free an engine.  @param state May be NULL. */
  void acq_destroy (acq_state_t *state);

  /**
   * @brief Drain the input ring and reset the coherent accumulator.
   * @param state Must be non-NULL.
   */
  void acq_reset (acq_state_t *state);

  /**
   * @brief Stream raw samples; emit one event per CFAR dump above threshold.
   *
   * Buffers @p in, then for every complete frame applies the slow-time Doppler
   * FFT, correlates against the PN reference, dumps the coherent surface (or,
   * when n_noncoh > 1, accumulates |·|² over n_noncoh looks first), gates the
   * peak on the auto-configured threshold, and appends an acq_result_t.
   *
   * @param state        Allocated engine (non-NULL).
   * @param in           Raw input, interleaved CF32, @p n_in complex samples.
   * @param n_in         Number of complex input samples.
   * @param result       Output array for detection events.
   * @param max_results  Capacity of @p result.
   * @return Number of events written (0 … max_results).
   */
  size_t acq_push (acq_state_t *state, const float complex *in, size_t n_in,
                   acq_result_t *result, size_t max_results);

  /* ── Serializable state — the elastic / pure-transducer face
   * ─────────────────
   *
   * The OO engine above is convenient but stateful.  These match the rest of
   * the library's serializable objects (lo/cic/fir/ddcr): serialize a
   * channel's cross-call state to a flat POD, ship (descriptor, state, input)
   * to any thread/process/pod, rebuild the engine from the descriptor
   * (acq_create), inject the state, and continue — bit-identical to an
   * uninterrupted run.
   */

  /** @brief Byte size of @p state's blob (header + unconsumed + nc). */
  size_t acq_state_bytes (const acq_state_t *state);

  /**
   * @brief Serialize @p state's cross-call state into @p blob (caller-owned,
   *        acq_state_bytes() long).  Call between pushes (no partial dump
   * pending).
   */
  void acq_get_state (const acq_state_t *state, void *blob);

  /**
   * @brief Restore cross-call state from @p blob into @p state (replacing it).
   * @return 0 on success, -1 if the blob's magic/version/n/n_noncoh disagree
   *         with @p state (rebuild the engine from the matching descriptor
   * first).
   */
  int acq_set_state (acq_state_t *state, const void *blob);

  /**
   * @brief Pure run: inject @p state_in, stream @p in, emit hits, export
   *        @p state_out — `(state_in, input) -> (state_out, output)` over an
   *        engine treated as immutable config + scratch.  @p state_in / @p
   *        state_out may alias.  Either may be NULL (NULL in = fresh;
   *        NULL out = discard).
   * @return Number of events written (0 … max_results).
   */
  size_t acq_run (acq_state_t *state, const void *state_in, void *state_out,
                  const float complex *in, size_t n_in, acq_result_t *result,
                  size_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* ACQ_CORE_H */
