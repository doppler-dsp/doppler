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
   * @brief Create a feedforward BPSK DSSS burst demodulator.
   *
   * Recovers the payload of a single spread burst end to end, with no tracking
   * loops: it estimates the burst's Doppler (and Doppler rate) from the
   * unmodulated acquisition preamble, dechirps by that estimate, despreads the
   * data section into soft symbols, aligns on the known sync word, slices to
   * bits, and checks the CRC-16 trailer. One @p max_rate knob spans the whole
   * range from near-static Doppler (0) to a severe LEO chirp.
   *
   * After construction, register the templates and the acquisition seed —
   * set_preamble(), set_sync(), set_prior() — then call demod() once per burst.
   *
   * @param data_code      Data spreading code, one 0/1 chip per element; copied
   *                       into the object (its length is the data spreading
   *                       factor, chips/symbol).
   * @param data_code_len  Data spreading factor (chips/symbol); the length of
   *                       @p data_code.
   * @param spc            Samples per chip (front-end oversample).
   * @param chip_rate      Chip rate (Hz); sets the sample rate as spc*chip_rate.
   * @param carrier_hz     RF carrier (Hz) for code-Doppler scaling; 0 = ignore.
   * @param max_rate       Chirp-rate search half-span (cycles/sample^2 at the
   *                       input rate); 0 = Doppler only (no rate search).
   * @param payload_len    Number of payload data symbols (bits) in a frame.
   * @param est_segments   Partial correlations per acq period (segmentation for
   *                       the feedforward estimate; larger tolerates more rate).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50
   * >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
   * >>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(np.uint8)
   * >>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)
   * >>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)
   * >>> def crc16(bits):
   * ...     c = 0xFFFF
   * ...     for b in bits:
   * ...         c ^= (int(b) & 1) << 15
   * ...         c = (((c << 1) ^ 0x1021) & 0xFFFF
   * ...              if c & 0x8000 else (c << 1) & 0xFFFF)
   * ...     return c
   * >>> crc = crc16(payload)
   * >>> crc_bits = np.array([(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
   * >>> frame = np.concatenate([sync, payload, crc_bits])
   * >>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)
   * >>> chips = ([np.tile(csign(acode), reps)]
   * ...          + [csign(b) * csign(dcode) for b in frame])
   * >>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)
   * >>> n = np.arange(len(bb))
   * >>> f0 = 0.012
   * >>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)
   * >>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, payload_len=64)
   * >>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble
   * >>> d.set_sync(sync)              # Barker-13 frame-sync word
   * >>> d.set_prior(f0, 0)           # coarse Doppler + preamble start
   * >>> bits = d.demod(x)           # estimate -> dechirp -> despread -> slice
   * >>> int(d.frame_valid), bool(np.array_equal(bits, payload))
   * (1, True)
   *
   * @endcode
   */
  burst_demod_state_t *burst_demod_create (const uint8_t *data_code,
                                           size_t data_code_len, size_t spc,
                                           double chip_rate, double carrier_hz,
                                           double max_rate, size_t payload_len,
                                           size_t est_segments);

  /** @brief Destroy a demodulator.  @param state May be NULL. */
  void burst_demod_destroy (burst_demod_state_t *state);

  /**
   * @brief Clear the per-burst read-backs, leaving the configuration intact.
   *
   * Zeros the after-demod fields (@c frame_valid, @c frame_offset,
   * @c n_symbols, and the @c est_* estimates) so a stale result cannot be
   * mistaken for a fresh one. The spreading codes, sync word, and prior set up
   * before the first burst are preserved, so the object is immediately ready
   * to demodulate the next burst.
   *
   * @param state  Demodulator handle.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> dcode = (np.arange(50) & 1).astype(np.uint8)
   * >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
   * >>> d.reset()               # clears est_ fields + frame_valid, keeps config
   * >>> d.frame_valid
   * 0
   *
   * @endcode
   */
  void burst_demod_reset (burst_demod_state_t *state);

  /**
   * @brief Register the unmodulated acquisition preamble code and its
   *        repetition count used for the feedforward (f0, rate) estimate.
   *
   * The preamble is the acq spreading code transmitted @p reps times with no
   * data modulation; demod() segment-despreads it into partial correlations
   * and feeds those to the polynomial-phase estimator to recover the coarse
   * (frequency, chirp-rate). Call once after construction; the code is copied.
   *
   * @param state         Demodulator handle.
   * @param acq_code      Acq preamble spreading code, one 0/1 chip per element;
   *                      copied into the object.
   * @param acq_code_len  Acq code length (chips); the length of @p acq_code.
   * @param reps          Number of preamble repetitions in the burst.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> dcode = (np.arange(50) & 1).astype(np.uint8)
   * >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
   * >>> acode = (np.arange(500) & 1).astype(np.uint8)   # unmodulated preamble
   * >>> d.set_preamble(acode, reps=5)   # 5 repeats drive the (f0, rate) fit
   *
   * @endcode
   */
  void burst_demod_set_preamble (burst_demod_state_t *state,
                                 const uint8_t *acq_code, size_t acq_code_len,
                                 size_t reps);

  /**
   * @brief Register the known frame-sync word used for frame alignment and
   *        phase/sign resolution.
   *
   * After the data section is despread to soft BPSK symbols, demod() correlates
   * them against this word; the complex correlation peak locates the frame
   * (its @c frame_offset) and its phase resolves the residual carrier rotation
   * and the BPSK sign ambiguity before slicing. Pass the word as 0/1 symbols;
   * it is copied and stored internally as +/-1.
   *
   * @param state     Demodulator handle.
   * @param sync      Frame-sync word, one 0/1 symbol per element; copied.
   * @param sync_len  Sync word length (symbols); the length of @p sync.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> dcode = (np.arange(50) & 1).astype(np.uint8)
   * >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
   * >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
   * >>> d.set_sync(sync)   # Barker-13: frame alignment + phase/sign resolution
   *
   * @endcode
   */
  void burst_demod_set_sync (burst_demod_state_t *state, const uint8_t *sync,
                             size_t sync_len);

  /**
   * @brief Seed the demodulator from acquisition with the coarse Doppler and
   *        the preamble start sample.
   *
   * These come from the upstream acquisition stage: @p f0_coarse centres the
   * feedforward frequency search near the true Doppler, and @p start tells
   * demod() where the preamble begins within the burst so it despreads the
   * right samples. Call once per burst before demod().
   *
   * @param state      Demodulator handle.
   * @param f0_coarse  Coarse Doppler prior (cycles/sample at the input rate).
   * @param start      Preamble start sample index within the burst.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> dcode = (np.arange(50) & 1).astype(np.uint8)
   * >>> d = BurstDemod(dcode, spc=4, chip_rate=1e6, payload_len=64)
   * >>> d.set_prior(0.012, start=0)   # coarse Doppler + preamble start from acq
   *
   * @endcode
   */
  void burst_demod_set_prior (burst_demod_state_t *state, double f0_coarse,
                              size_t start);

  /** @brief Max output bits = payload_len (caller sizes the buffer). */
  size_t burst_demod_demod_max_out (burst_demod_state_t *state);

  /**
   * @brief Demodulate one burst end to end and write the payload bits.
   *
   * Runs the whole feedforward chain on the supplied samples: estimate the
   * (frequency, chirp-rate) from the preamble, dechirp, despread the data
   * section to soft symbols, sync-align and derotate, slice to bits, and check
   * the CRC-16 trailer. On return the read-back fields report the outcome —
   * @c frame_valid (CRC match), @c frame_offset, @c n_symbols, and the
   * @c est_freq_hz / @c est_rate_hz / @c est_snr_db estimates. The templates
   * and prior must already be set via set_preamble(), set_sync(), set_prior().
   *
   * The C function returns the number of bits written; the Python binding
   * returns those bits as an array (a view into a reused buffer unless an
   * @p out buffer is supplied).
   *
   * @param state    Demodulator handle.
   * @param x        Burst samples (complex baseband at spc*chip_rate).
   * @param x_len    Number of input samples.
   * @param out      Caller-provided output buffer for the payload bits.
   * @param max_out  Capacity of @p out, in bits.
   * @return Number of payload bits written (0 on failure / too-short burst).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50
   * >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
   * >>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(np.uint8)
   * >>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)
   * >>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)
   * >>> def crc16(bits):
   * ...     c = 0xFFFF
   * ...     for b in bits:
   * ...         c ^= (int(b) & 1) << 15
   * ...         c = (((c << 1) ^ 0x1021) & 0xFFFF
   * ...              if c & 0x8000 else (c << 1) & 0xFFFF)
   * ...     return c
   * >>> crc = crc16(payload)
   * >>> crc_bits = np.array([(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
   * >>> frame = np.concatenate([sync, payload, crc_bits])
   * >>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)
   * >>> chips = ([np.tile(csign(acode), reps)]
   * ...          + [csign(b) * csign(dcode) for b in frame])
   * >>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)
   * >>> n = np.arange(len(bb))
   * >>> f0 = 0.012
   * >>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)
   * >>> d = BurstDemod(dcode, spc=spc, chip_rate=1e6, payload_len=64)
   * >>> d.set_preamble(acode, reps)
   * >>> d.set_sync(sync)
   * >>> d.set_prior(f0, 0)
   * >>> bits = d.demod(x)
   * >>> int(d.frame_valid), bool(np.array_equal(bits, payload))
   * (1, True)
   *
   * @endcode
   */
  size_t burst_demod_demod (burst_demod_state_t *state, const float complex *x,
                            size_t x_len, uint8_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* BURST_DEMOD_CORE_H */
