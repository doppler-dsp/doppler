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
 *   push(raw cf32) -> ring buffer -> reframe to (ny, nx) -> slow-time Doppler
 *   FFT (FFT along the ny segment axis) -> 2-D code correlation against a
 *   single-row PN reference (corr2d, coherently integrated over `dwell`
 *   frames) -> argmax + CFAR noise estimate -> threshold gate -> acq_result_t.
 *
 * The fast-time axis (nx = sf*sps columns) is the circular code matched
 * filter; the slow-time axis (ny rows) is the Doppler search.  A carrier
 * offset f (cycles/sample) lands the peak at row = round(f*nx*ny) mod ny,
 * column = code phase (integer samples).
 *
 * Auto-configuration: given the PN @p code and a target (@p pfa, @p pd) plus
 * the expected per-sample amplitude SNR @p min_snr, create() computes the CFAR
 * threshold and the coherent @p dwell from the detection-theory functions
 * (det_threshold / det_pd).  One frame integrates n = ny*nx samples, so a dump
 * of `dwell` frames integrates dwell*n samples in the Marcum-Q model.
 *
 * @code
 * // 31-chip PN, 4x oversample, 16-segment Doppler search; aim Pfa=1e-3, Pd=0.9
 * uint8_t code[31] = { 0 };   // ... fill with PN chips (0/1) ...
 * acq_state_t *a = acq_create(code, 31, 31, 4, 16,
 *                             1e-3, 0.9, 0.126, 0, 64);
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

#include "clib_common.h"
#include "jm_perf.h"
#include "buffer/buffer.h"
#include "fft/fft_core.h"
#include "corr2d/corr2d_core.h"
#include "detection/detection_core.h"
/* detector2d_core.h supplies det_noise_mode_t (guarded typedef). */
#include "detector2d/detector2d_core.h"
#include "fft2d/fft2d_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief One acquisition detection event.
 */
typedef struct {
  size_t doppler_bin; /**< Peak row: slow-time Doppler bin (0 … ny-1).      */
  size_t code_phase;  /**< Peak col: integer-sample code phase (0 … nx-1).  */
  float peak_mag;     /**< max |R[i,j]| over the surface (linear).          */
  float noise_est;    /**< CFAR noise estimate over [noise_lo, noise_hi].   */
  float test_stat;    /**< peak_mag / noise_est; 0 if noise_est == 0.       */
  float snr_est;      /**< Estimated per-sample amplitude SNR of the burst. */
} acq_result_t;

/**
 * @brief Streaming acquisition-engine state.
 *
 * Allocate with acq_create(); never stack-allocate.
 */
typedef struct {
  corr2d_state_t *corr;   /**< Single-row-ref correlator; dwell-integrating. */
  fft_state_t *slow_fft;  /**< Length-ny forward FFT (slow-time Doppler).    */
  dp_f32_t *ring;         /**< Raw cf32 input ring (the only ring).          */
  float complex *ref;     /**< Single-row reference (ny*nx), owned.          */
  float complex *yframe;  /**< Slow-time-FFT'd frame (ny*nx) fed to corr.    */
  float complex *colbuf;  /**< Gathered column scratch (ny).                 */
  float complex *colout;  /**< FFT'd column scratch (ny).                    */
  float complex *out_buf; /**< corr2d dump output (ny*nx).                   */
  float *mag_buf;         /**< |out_buf| (ny*nx).                            */
  float *noise_scratch;   /**< Scratch for the median sort (ny*nx).          */

  size_t ny;        /**< Slow-time segments = Doppler search bins.           */
  size_t nx;        /**< One segment in samples = sf*sps = code-phase bins.  */
  size_t n;         /**< ny * nx — frame size in samples.                    */
  size_t sf;        /**< Chips per PN segment.                               */
  size_t sps;       /**< Samples per chip.                                   */
  size_t dwell;     /**< Frames coherently integrated per CFAR dump.         */
  size_t max_dwell; /**< Search cap used by the auto-config.                 */
  size_t ring_cap;  /**< Ring capacity in complex samples.                   */
  size_t noise_lo;  /**< First CFAR reference bin (inclusive).               */
  size_t noise_hi;  /**< Last CFAR reference bin (inclusive).                */
  det_noise_mode_t noise_mode; /**< CFAR aggregation mode.                   */

  float threshold;     /**< CFAR gate on test_stat (theta).                  */
  float eta;           /**< Raw per-cell Rayleigh amplitude threshold.       */
  double pfa_cell;     /**< Bonferroni per-cell false-alarm probability.     */
  double pd_predicted; /**< Predicted Pd at min_snr and the chosen dwell.    */

  /* Last-dump bookkeeping (for inspection). */
  size_t peak_row;
  size_t peak_col;
  float peak_mag;
  float noise_est;
  float test_stat;
} acq_state_t;

/**
 * @brief Create a streaming DSSS acquisition engine.
 *
 * Builds the single-row oversampled BPSK reference from @p code and
 * auto-configures the CFAR threshold and coherent dwell from the target
 * (@p pfa, @p pd) at the expected SNR @p min_snr.
 *
 * @param code        PN chips (0/1), length @p code_len; must equal @p sf.
 * @param code_len    Number of chips supplied.
 * @param sf          Chips per PN segment (>= 1).
 * @param sps         Samples per chip (>= 1).
 * @param ny          Slow-time segments = Doppler bins (>= 1).
 * @param pfa         Target system (max-of-N) false-alarm probability (0,1).
 * @param pd          Target detection probability (0,1).
 * @param min_snr     Expected per-sample amplitude SNR (linear, > 0).
 * @param noise_mode  CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
 * @param max_dwell   Upper bound on the coherent dwell search (frames).
 * @return Heap-allocated state, or NULL on bad arguments / allocation failure.
 */
acq_state_t *acq_create (const uint8_t *code, size_t code_len, size_t sf,
                         size_t sps, size_t ny, double pfa, double pd,
                         double min_snr, int noise_mode, size_t max_dwell);

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
 * FFT, correlates against the PN reference, and — every @p dwell frames —
 * dumps the coherent surface, gates the peak on the auto-configured threshold,
 * and appends an acq_result_t.
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

#ifdef __cplusplus
}
#endif

#endif /* ACQ_CORE_H */
