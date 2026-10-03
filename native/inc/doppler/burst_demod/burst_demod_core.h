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
 *   5. slice `frame_syms` symbols to bits, hard and soft, and STOP.
 *
 * ## Where this object's job ends
 *
 * At a decision. It hands back one bit per symbol (demod()) and one LLR per
 * symbol (dp_burst_demod_llrs()), and it does not know what any of them mean:
 * which are payload, which are a check, what an outer code would repair are
 * all questions about a FRAME, and answering them needs a description this
 * object deliberately does not hold (doppler#1022). It used to hold half of
 * one — a hard-coded `sync | payload | CRC-16` — which is how a burst sent
 * without a trailer came to be reported invalid.
 *
 * What it does need is the sync word, to find the frame and resolve the BPSK
 * sign, and `frame_syms`, to know how many symbols to slice. Both are
 * physical-layer facts, and both come from ONE place: the frame description
 * the transmitter spread. Its field 0 is the sync word and its layout is the
 * frame's length, so the two cannot disagree with each other or with the
 * transmitter.
 *
 * Build it from that description with dp_burst_demod_create_desc(), seed it
 * with set_preamble(acq code, reps) and set_prior(coarse Doppler, preamble
 * start), then demod(burst).
 * One @c max_rate knob spans near-static Doppler (0) to severe LEO chirp.
 * One-shot per burst. Composes ppe (which composes fft + spectral).
 *
 * @code
 * const uint8_t    sb[3] = { 1, 0, 1 }, dcode[4] = { 1, 0, 1, 1 };
 * const uint8_t    acode[8] = { 1, 1, 1, 0, 1, 0, 0, 1 };
 * const wfm_seq_t  sync  = { .kind = WFM_SEQ_LITERAL, .bits = sb, .len = 3 };
 * const wfm_seq_t  data  = { .kind = WFM_SEQ_DATA, .len = 8 };
 * wfm_frame_desc_t f;
 * dp_wfm_frame_fixed (&f, NULL, 0, &sync, &data, 1); // sync|data:8|crc16
 * dp_burst_demod_state_t *d = dp_burst_demod_create_desc (
 *     dcode, 4, &f, 4, 1e6, 0.0, 0.0, 10, NULL);
 * if (!d)
 *   return 1;
 * dp_burst_demod_set_preamble (d, acode, 8, 5);
 * dp_burst_demod_set_prior (d, 0.0, 0);  // coarse Doppler, preamble start
 * float _Complex x[16] = { 0 };          // a real burst goes here
 * uint8_t        bits[3 + 8 + 16];
 * size_t nbits = dp_burst_demod_demod (d, x, 16, bits, sizeof bits);
 * (void)nbits;                           // 0: too short to be a burst
 * dp_burst_demod_destroy (d);
 * @endcode
 */
#ifndef DP_BURST_DEMOD_CORE_H
#define DP_BURST_DEMOD_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/ppe/ppe_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/dp_complex.h"
#include "doppler/conv/conv_core.h"
#include "doppler/rs/rs_core.h"
#include "doppler/pn/pn_core.h"
#include "doppler/gold/gold_core.h"
#include "doppler/wfm/wfm_frame.h"
#include "doppler/mpsk/mpsk_core.h"
#include "doppler/cvt/cvt_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief BurstDemod state.  Allocate with dp_burst_demod_create_desc().
   */
  typedef struct
  {
    /* ── configuration ── */
    uint8_t *data_code; /**< owned data spreading code (0/1), length data_sf. */
    size_t   data_sf;   /**< data spreading factor (chips/symbol).           */
    uint8_t *acq_code;  /**< owned acq preamble code (0/1), length acq_sf.    */
    size_t   acq_sf;    /**< acq code length (chips).                        */
    size_t   acq_reps;  /**< acq preamble repetitions.                       */
    int8_t  *sync;      /**< owned sync word as +/-1, length sync_len — the
                             correlation template, and the only thing this
                             object knows about the frame's CONTENT.        */
    size_t   sync_len;  /**< sync word length (symbols).                     */
    size_t   spc;       /**< samples per chip.                              */
    double   chip_rate; /**< chip rate (Hz).                               */
    double   carrier_hz; /**< RF carrier (Hz) for code-Doppler; 0 = ignore. */
    double   max_rate;  /**< chirp-rate search half-span (cycles/sample^2). */
    size_t   frame_syms;   /**< symbols the frame occupies, sync word
                                included — the description's layout length,
                                read at create. What they MEAN is the frame
                                description's business, one layer up.      */
    size_t   est_segments; /**< partials per acq period for the estimate.   */
    double   f0_prior;     /**< coarse Doppler prior (cycles/sample).       */
    size_t   start;        /**< preamble start sample in the burst.         */

    /* ── engine ── */
    dp_ppe_state_t   *ppe;  /**< feedforward (rate x freq) estimator.          */
    float _Complex *part; /**< preamble partials scratch (acq_reps*est_seg). */
    size_t         n_part;

    /* ── read-backs (after demod) ── */
    float *llr;   /**< The frame's soft bits, `mpsk_soft_demap`'s
                       convention: positive means bit 0, so `L < 0` is the
                       hard decision demod() returned. Valid until the next
                       demod(); n_llr of them.                            */
    size_t n_llr; /**< LLRs the last demod() wrote (the frame's length).  */
    float _Complex *sym; /**< The frame's DEROTATED, unit-normalised complex
                             symbols -- the constellation the LLRs are the
                             real part of. Built either way to compute the
                             projection and the noise estimate, and freed
                             unread until doppler#1087: for BPSK a residual
                             phase error and a genuine amplitude loss are
                             indistinguishable in every real-only statistic
                             (a rotation scales Re by cos(phi) without adding
                             noise), so |LLR|, its spread and the BER all
                             agree while only the quadrature separates them.
                             Valid until the next demod(); n_sym of them.  */
    size_t n_sym; /**< Symbols the last demod() wrote (the frame's length).*/
    double est_n0; /**< Noise power the LLRs are scaled by, referred to
                        unit symbol amplitude. Published so a caller can
                        undo the scaling, or compare bursts by it.        */
    size_t frame_offset; /**< symbol offset of the sync word.             */
    size_t n_symbols;    /**< despread data symbols produced.             */
    double est_freq_hz;  /**< estimated residual Doppler (Hz).            */
    double est_rate_hz;  /**< estimated Doppler rate (Hz/s).              */
    double est_cn0_dbhz; /**< Carrier-to-noise DENSITY, dB-Hz, referred to
                              the CHANNEL: the realized symbol estimate
                              `1/est_n0` lifted by the symbol rate and
                              corrected for the despreading loss that
                              @c est_timing_chips measures. Sample-rate
                              invariant, so it survives a front-end
                              rate change, and it does not move when the
                              receiver's own timing is off by a fraction
                              of a chip -- which a realized SNR does, by
                              4.8 dB at half a chip. Zero until a demod()
                              produces a frame.
                              DEGRADED BY RESIDUAL PHASE, by construction:
                              @c est_n0 reads the noise off the quadrature,
                              and a rotation puts signal there, so an
                              untracked Doppler rate reads as a worse link
                              -- 101 dB worse on a noiseless input. The
                              realized link IS worse, so the number is not
                              wrong; it simply cannot say WHICH of the two
                              happened. dp_burst_demod_symbols() can, because
                              only Q against I separates them
                              (doppler#1087). See doppler#1304.          */
    double est_timing_chips; /**< Burst-start error the demodulator MEASURED,
                              in chips, signed, relative to the @c start
                              given to set_prior(). The whole-SAMPLE part of
                              it was removed before despreading; only the
                              fraction of a sample that a shift cannot
                              remove is still a loss, and only that fraction
                              is taken out of @c est_cn0_dbhz. The symbols
                              keep the loss either way. Acquisition resolves
                              a start to one SAMPLE, so a residual here is
                              structural rather than a caller's error.
                              BOUNDED at half a chip, which is the metric's
                              limit and not a budget: a whole-chip slip
                              re-aligns every partial against its
                              neighbour's chip and reads as no slip at all.
                              A magnitude AT that bound therefore means the
                              search saturated -- the start is further out
                              than this object can measure, and
                              @c est_cn0_dbhz is then a floor rather than a
                              measurement. Fixing that is acquisition's,
                              which is what resolves a start.              */
  } dp_burst_demod_state_t;

  /**
   * @brief The Python binding's constructor: @ref dp_burst_demod_create_desc
   *        without the `why` out-parameter.
   *
   * An object's generated constructor has no channel for a reason, so a
   * refused description surfaces as the manifest's `create_error_message`,
   * which names the rules. C callers that want the reason call the `_desc`
   * form.
   *
   * @param data_code      the data spreading code, 0/1 chips.
   * @param data_code_len  its length (the spreading factor).
   * @param frame          the description (`const wfm_frame_desc_t *`).
   * @param spc            samples per chip.
   * @param chip_rate      chips per second.
   * @param carrier_hz     the carrier the baseband is offset by, Hz.
   * @param max_rate       the Doppler rate searched, cycles/sample^2.
   * @param est_segments   partials per acquisition period for the estimate.
   * @return the demodulator, or NULL.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> from doppler.wfm import Frame
   * >>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50
   * >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
   * >>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(
   * ...     np.uint8)
   * >>> dcode = ((np.arange(data_sf) * 40503 >> 7) & 1).astype(np.uint8)
   * >>> payload = ((np.arange(64) * 7 + 3) & 1).astype(np.uint8)
   * >>> desc = Frame(sync=sync, payload=payload, crc="crc16")
   * >>> frame = desc.bits()    # sync | payload | CRC-16: ONE description
   * >>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)
   * >>> chips = ([np.tile(csign(acode), reps)]
   * ...          + [csign(b) * csign(dcode) for b in frame])
   * >>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)
   * >>> n = np.arange(len(bb))
   * >>> f0 = 0.012
   * >>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)
   * >>> d = BurstDemod(dcode, desc, spc=spc, chip_rate=1e6)
   * >>> d.set_preamble(acode, reps)   # unmodulated (f0, rate) preamble
   * >>> d.set_prior(f0, 0)            # coarse Doppler + preamble start
   * >>> bits = d.demod(x)      # estimate -> dechirp -> despread -> slice
   * >>> bool(np.array_equal(bits, frame))   # the FRAME, not the payload
   * True
   *
   * @endcode
   */
  dp_burst_demod_state_t *dp_burst_demod_create_frame (
      const uint8_t *data_code, size_t data_code_len,
      const wfm_frame_desc_t *frame, size_t spc, double chip_rate,
      double carrier_hz, double max_rate, size_t est_segments);

  /**
   * @brief Create a demodulator from the frame DESCRIPTION the transmitter
   *        spread, in place of a sync word and a hand-counted `frame_syms`.
   *
   * The sync word is the description's field 0 and `frame_syms` is its
   * layout's length, both read by @ref dp_wfm_frame_desc_rx, so the receiver
   * and the transmitter are told the same thing by the same code. The
   * description is read here and not kept: the sync bits are copied into the
   * demodulator, so the caller may free `frame` at once. It is the one
   * constructor: the demodulator is then seeded with set_preamble() and
   * set_prior(), and demod() is called once per burst.
   *
   * @param data_code      the data spreading code, 0/1 chips.
   * @param data_code_len  its length (the spreading factor).
   * @param frame          the description (`const wfm_frame_desc_t *`).
   * @param spc            samples per chip.
   * @param chip_rate      chips per second.
   * @param carrier_hz     the carrier the baseband is offset by, Hz.
   * @param max_rate       the Doppler rate searched, cycles/sample^2; 0
   *                       selects the single-FFT estimate.
   * @param est_segments   partials per acquisition period for the estimate.
   * @param why            on a NULL return, receives a static sentence
   *                       naming the fix (a description refused by
   *                       @ref dp_wfm_frame_desc_rx, or a bad parameter);
   *                       may be `NULL`.
   * @return the demodulator, or NULL with @p why set.
   *
   * @code
   * const uint8_t    sb[3] = { 1, 0, 1 }, dcode[4] = { 1, 0, 1, 1 };
   * const wfm_seq_t  sync  = { .kind = WFM_SEQ_LITERAL, .bits = sb,
   *                            .len = 3 };
   * const wfm_seq_t  data  = { .kind = WFM_SEQ_DATA, .len = 8 };
   * wfm_frame_desc_t f;
   * dp_wfm_frame_fixed (&f, NULL, 0, &sync, &data, 1); // sync|data:8|crc16
   * const char *why = NULL;
   * dp_burst_demod_state_t *d = dp_burst_demod_create_desc (
   *     dcode, 4, &f, 4, 1e6, 0.0, 0.0, 10, &why);
   * if (!d || dp_burst_demod_llrs_max_out (d, 1) != 3 + 8 + 16)
   *   return 1;
   * dp_burst_demod_destroy (d);
   * @endcode
   */
  dp_burst_demod_state_t *dp_burst_demod_create_desc (
      const uint8_t *data_code, size_t data_code_len,
      const wfm_frame_desc_t *frame, size_t spc, double chip_rate,
      double carrier_hz, double max_rate, size_t est_segments,
      const char **why);

  /** @brief Destroy a demodulator.  @param state May be NULL. */
  void dp_burst_demod_destroy (dp_burst_demod_state_t *state);

  /**
   * @brief Clear the per-burst read-backs, leaving the configuration intact.
   *
   * Zeros the after-demod fields (@c frame_offset,
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
   * >>> from doppler.wfm import Frame
   * >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
   * ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
   * >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
   * >>> d.reset()          # clears the estimates, keeps the config
   * >>> d.frame_offset
   * 0
   *
   * @endcode
   */
  void dp_burst_demod_reset (dp_burst_demod_state_t *state);

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
   * >>> from doppler.wfm import Frame
   * >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
   * ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
   * >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
   * >>> acode = (np.arange(500) & 1).astype(np.uint8)  # unmodulated
   * >>> d.set_preamble(acode, reps=5)  # 5 reps drive the (f0, rate) fit
   *
   * @endcode
   */
  void dp_burst_demod_set_preamble (dp_burst_demod_state_t *state,
                                 const uint8_t *acq_code, size_t acq_code_len,
                                 size_t reps);

  /**
   * @brief LLRs the last demod() wrote — the frame's soft bits.
   *
   * `crealf(sym * derot)` IS the log-likelihood ratio up to a scale, and it
   * was computed, sliced to one bit and freed on every burst. A hard
   * decision throws away roughly 2 dB of the coding gain a soft-input
   * decoder exists to deliver (`mpsk_soft_demap`'s own docstring), so this
   * is what makes a coded burst worth coding.
   *
   * **The convention is not a new one**: `mpsk_soft_demap`'s, which is
   * `mpsk_demap`'s decision rule seen a second way. Positive means bit 0,
   * so `L < 0` reproduces exactly the bits demod() returned — asserted in
   * the tests rather than assumed.
   *
   * Spans the WHOLE frame, not just the payload, because a code covers what
   * its description says it covers and a decoder needs the bits the code
   * protects. The payload's own span is `field_off`/`field_bits` of the
   * layout.
   *
   * Scaled by @c est_n0 rather than left raw: a Viterbi is invariant to a
   * positive scale, but LLRs from different bursts are not comparable
   * without one, and combining across bursts needs them to be.
   *
   * @param state    Demodulator handle.
   * @param n        Ignored — the count is the last demod()'s frame.
   * @param out      Receives the LLRs, one per frame bit.
   * @param max_out  Capacity of @p out; see dp_burst_demod_llrs_max_out().
   * @return LLRs written — `min(frame bits, max_out)`, or 0 if the last
   *         demod() produced no frame.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> dcode = (np.arange(50) & 1).astype(np.uint8)
   * >>> from doppler.wfm import Frame
   * >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
   * ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
   * >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
   * >>> d.llrs_max_out(1)          # one per frame symbol
   * 93
   *
   * @endcode
   */
  size_t dp_burst_demod_llrs (dp_burst_demod_state_t *state, size_t n, float *out,
                           size_t max_out);

  /**
   * @brief Max LLRs dp_burst_demod_llrs() writes: the frame's length in bits.
   *
   * @param state  Demodulator handle.
   * @param n      Ignored — the count is the last demod()'s frame.
   */
  size_t dp_burst_demod_llrs_max_out (dp_burst_demod_state_t *state, size_t n);

  /**
   * @brief The last demod()'s DEROTATED complex symbols — the constellation
   *        the LLRs are the real part of.
   *
   * Same span and same normalisation as dp_burst_demod_llrs(): the whole frame,
   * scaled to unit mean-|Re| by the burst's own estimate, so
   * `crealf(symbols[k])` is that bit's LLR up to @c est_n0.
   *
   * The quadrature is why this exists. After derotation the real axis
   * carries the signal and the imaginary axis carries noise alone, so Q is
   * diagnostic: a residual phase error scales Re by `cos(phi)` WITHOUT
   * adding noise, which makes it indistinguishable from a genuine amplitude
   * or SNR loss in mean |LLR|, in LLR spread and in BER alike. Measured over
   * 20000 BPSK symbols, a 30 degree phase error and an amplitude loss of
   * `cos(30 deg)` agreed to three decimals in all three, and differed only in
   * Q/I energy — 0.386 against 0.077 (doppler#1087). That is the difference
   * between a pointing problem and a link-budget one, on a burst this object
   * already characterised well enough to know.
   *
   * @param state    Demodulator handle.
   * @param n        Ignored — the count is the last demod()'s frame.
   * @param out      Receives the symbols, one per frame bit.
   * @param max_out  Capacity of @p out; see dp_burst_demod_symbols_max_out().
   * @return Symbols written — `min(frame bits, max_out)`, or 0 if the last
   *         demod() produced no frame.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> dcode = (np.arange(50) & 1).astype(np.uint8)
   * >>> from doppler.wfm import Frame
   * >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
   * ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
   * >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
   * >>> d.symbols_max_out(1)       # one per frame symbol, as llrs()
   * 93
   *
   * @endcode
   */
  size_t dp_burst_demod_symbols (dp_burst_demod_state_t *state, size_t n,
                              float _Complex *out, size_t max_out);

  /**
   * @brief Max symbols dp_burst_demod_symbols() writes: the frame's length.
   *
   * @param state  Demodulator handle.
   * @param n      Ignored — the count is the last demod()'s frame.
   */
  size_t dp_burst_demod_symbols_max_out (dp_burst_demod_state_t *state, size_t n);

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
   * >>> from doppler.wfm import Frame
   * >>> desc = Frame(sync=np.zeros(13, np.uint8), payload=np.zeros(64, np.uint8),
   * ...              crc="crc16")      # 13 + 64 + 16 = 93 symbols
   * >>> d = BurstDemod(dcode, desc, spc=4, chip_rate=1e6)
   * >>> d.set_prior(0.012, start=0)   # coarse Doppler + start, from acq
   *
   * @endcode
   */
  void dp_burst_demod_set_prior (dp_burst_demod_state_t *state, double f0_coarse,
                              size_t start);

  /** @brief Max output bits = frame_syms (caller sizes the buffer). */
  size_t dp_burst_demod_demod_max_out (dp_burst_demod_state_t *state);

  /**
   * @brief Demodulate one burst end to end and write the frame's bits.
   *
   * Runs the whole feedforward chain on the supplied samples: estimate the
   * (frequency, chirp-rate) from the preamble, dechirp, despread the data
   * section to soft symbols, sync-align and derotate, and slice `frame_syms`
   * symbols to bits. It writes the frame as received — sync word first — and
   * makes no claim about what those bits are for: undoing the frame needs a
   * description, and that is a caller's, not this object's. The soft twin of
   * the same decisions is dp_burst_demod_llrs().
   *
   * On return the read-back fields report the outcome — @c frame_offset,
   * @c n_symbols, and the @c est_freq_hz / @c est_rate_hz /
   * @c est_cn0_dbhz / @c est_timing_chips estimates. The templates and prior must already be set via
   * set_preamble() and set_prior().
   *
   * The C function returns the number of bits written; the Python binding
   * returns those bits as an array (a view into a reused buffer unless an
   * @p out buffer is supplied).
   *
   * @param state    Demodulator handle.
   * @param x        Burst samples (complex baseband at spc*chip_rate).
   * @param x_len    Number of input samples.
   * @param out      Caller-provided output buffer for the frame's bits.
   * @param max_out  Capacity of @p out, in bits.
   * @return Number of frame bits written (0 on failure / too-short burst).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import BurstDemod
   * >>> spc, acq_sf, reps, data_sf = 4, 500, 5, 50
   * >>> sync = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)
   * >>> acode = ((np.arange(acq_sf) * 2654435761 >> 13) & 1).astype(
   * ...     np.uint8)
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
   * >>> crc_bits = np.array(
   * ...     [(crc >> (15 - j)) & 1 for j in range(16)], np.uint8)
   * >>> frame = np.concatenate([sync, payload, crc_bits])
   * >>> csign = lambda b: np.where(np.asarray(b) & 1, -1.0, 1.0)
   * >>> chips = ([np.tile(csign(acode), reps)]
   * ...          + [csign(b) * csign(dcode) for b in frame])
   * >>> bb = np.repeat(np.concatenate(chips), spc).astype(np.complex64)
   * >>> n = np.arange(len(bb))
   * >>> f0 = 0.012
   * >>> x = (bb * np.exp(2j * np.pi * f0 * n)).astype(np.complex64)
   * >>> from doppler.wfm import Frame
   * >>> desc = Frame(sync=sync, payload=np.zeros(64, np.uint8), crc="crc16")
   * >>> d = BurstDemod(dcode, desc, spc=spc, chip_rate=1e6)
   * >>> d.set_preamble(acode, reps)
   * >>> d.set_prior(f0, 0)
   * >>> bits = d.demod(x)
   * >>> bool(np.array_equal(bits, frame))     # sync | payload | CRC, as sent
   * True
   * >>> from doppler.wfm import crc16
   * >>> int(crc16(bits[13:77])) == crc        # the CHECK is the caller's
   * True
   *
   * @endcode
   */
  size_t dp_burst_demod_demod (dp_burst_demod_state_t *state, const float _Complex *x,
                            size_t x_len, uint8_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* BURST_DEMOD_CORE_H */
