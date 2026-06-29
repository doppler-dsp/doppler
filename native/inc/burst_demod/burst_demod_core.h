/**
 * @file burst_demod_core.h
 * @brief Feedforward BPSK DSSS frame demodulator.
 *
 * The whole post-acquisition payload chain, in C, with no tracking loops:
 *   1. preamble estimate — segment-despread the unmodulated, repeated acq
 *      preamble into partial correlations and feed them to ppe, giving a
 *      coarse (frequency, chirp-rate);
 *   2. sample-rate dechirp by (f0, rate) — removes Doppler AND Doppler rate;
 *   3. despread the data section with the (short) data code -> soft BPSK symbols;
 *   4. frame sync — correlate the symbols against the known sync word; the
 *      complex peak gives the frame offset and the residual phase (derotated);
 *   5. slice the payload to bits; verify the CRC-16 trailer -> @c frame_valid.
 *
 * Seed from acquisition with set_prior(coarse Doppler, preamble start),
 * set_preamble(acq code, reps) and set_sync(sync word), then demod(burst). One
 * @c max_rate knob spans near-static Doppler (0) to severe LEO chirp. One-shot
 * per burst. Composes ppe (which composes fft + spectral).
 *
 * @code
 * burst_demod_state_t *d = burst_demod_create(dcode, 50, 4, 1e6, 0, 0, 256, 10);
 * burst_demod_set_preamble(d, acode, 500, 5);
 * burst_demod_set_sync(d, sync, 31);
 * burst_demod_set_prior(d, f0_coarse, preamble_start);
 * size_t nbits = burst_demod_demod(d, x, n, bits, 256);   // d->frame_valid ...
 * @endcode
 */
#ifndef BURST_DEMOD_CORE_H
#define BURST_DEMOD_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "ppe/ppe_core.h"
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"
#include <complex.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief BurstDemod state.  Allocate with burst_demod_create().
   */
  typedef struct
  {
    /* ── configuration ── */
    uint8_t *data_code; /**< owned data spreading code (0/1), length data_sf. */
    size_t   data_sf;   /**< data spreading factor (chips/symbol).           */
    uint8_t *acq_code;  /**< owned acq preamble code (0/1), length acq_sf.    */
    size_t   acq_sf;    /**< acq code length (chips).                        */
    size_t   acq_reps;  /**< acq preamble repetitions.                       */
    int8_t  *sync;      /**< owned sync word as +/-1, length sync_len.       */
    size_t   sync_len;  /**< sync word length (symbols).                     */
    size_t   spc;       /**< samples per chip.                              */
    double   chip_rate; /**< chip rate (Hz).                               */
    double   carrier_hz; /**< RF carrier (Hz) for code-Doppler; 0 = ignore. */
    double   max_rate;  /**< chirp-rate search half-span (cycles/sample^2). */
    size_t   payload_len;  /**< payload data symbols (bits).                */
    size_t   est_segments; /**< partials per acq period for the estimate.   */
    double   f0_prior;     /**< coarse Doppler prior (cycles/sample).       */
    size_t   start;        /**< preamble start sample in the burst.         */

    /* ── engine ── */
    ppe_state_t   *ppe;  /**< feedforward (rate x freq) estimator.          */
    float complex *part; /**< preamble partials scratch (acq_reps*est_seg). */
    size_t         n_part;

    /* ── read-backs (after demod) ── */
    int    frame_valid;  /**< 1 if the CRC-16 trailer matched.             */
    size_t frame_offset; /**< symbol offset of the sync word.             */
    size_t n_symbols;    /**< despread data symbols produced.             */
    double est_freq_hz;  /**< estimated residual Doppler (Hz).            */
    double est_rate_hz;  /**< estimated Doppler rate (Hz/s).              */
    double est_snr_db;   /**< estimator confidence (dB).                  */
  } burst_demod_state_t;

  /**
   * @brief Create a burst demodulator.
   * @param data_code      Data spreading code (0/1); copied.
   * @param data_code_len  Data spreading factor (chips/symbol).
   * @param spc            Samples per chip.
   * @param chip_rate      Chip rate (Hz).
   * @param carrier_hz     RF carrier (Hz) for code-Doppler scaling; 0 = ignore.
   * @param max_rate       Chirp-rate search half-span (cycles/sample^2 at the
   *                       input rate); 0 = Doppler only (no rate search).
   * @param payload_len    Number of payload data symbols (bits) in a frame.
   * @param est_segments   Partial correlations per acq period (segmentation for
   *                       the feedforward estimate; larger tolerates more rate).
   * @return Heap state, or NULL on bad args / allocation failure.
   */
  burst_demod_state_t *burst_demod_create (const uint8_t *data_code,
                                           size_t data_code_len, size_t spc,
                                           double chip_rate, double carrier_hz,
                                           double max_rate, size_t payload_len,
                                           size_t est_segments);

  /** @brief Destroy a demodulator.  @param state May be NULL. */
  void burst_demod_destroy (burst_demod_state_t *state);

  /** @brief Clear the read-backs (config is preserved). */
  void burst_demod_reset (burst_demod_state_t *state);

  /** @brief Set the unmodulated acq preamble code + repetition count. */
  void burst_demod_set_preamble (burst_demod_state_t *state,
                                 const uint8_t *acq_code, size_t acq_code_len,
                                 size_t reps);

  /** @brief Set the known frame-sync word (0/1 symbols). */
  void burst_demod_set_sync (burst_demod_state_t *state, const uint8_t *sync,
                             size_t sync_len);

  /** @brief Seed from acquisition: coarse Doppler + preamble start sample. */
  void burst_demod_set_prior (burst_demod_state_t *state, double f0_coarse,
                              size_t start);

  /** @brief Max output bits = payload_len (caller sizes the buffer). */
  size_t burst_demod_demod_max_out (burst_demod_state_t *state);

  /**
   * @brief Demodulate a burst; write the payload bits to @p out.
   * @return Number of bits written (0 on failure / too-short burst). The
   *         read-back fields (frame_valid, est_*, frame_offset) are updated.
   */
  size_t burst_demod_demod (burst_demod_state_t *state, const float complex *x,
                            size_t x_len, uint8_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* BURST_DEMOD_CORE_H */
