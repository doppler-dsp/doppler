/**
 * @file ccsds_tm.h
 * @brief CCSDS TM channel coding — the transforms a transfer frame passes
 * through on its way to symbols.
 *
 * This is the coding layer doppler did not have. Nothing in the tree encoded
 * anything before it: no convolutional code, no Reed-Solomon, no interleaver
 * and no randomiser, which is the gap between a test-vector generator and a
 * link waveform.
 *
 * ## Normative references
 *
 * - **CCSDS 131.0-B-5**, *TM Synchronization and Channel Coding*, Blue Book,
 *   September 2023 — the current issue, and what this implements.
 * - **CCSDS 130.1-G**, *TM Synchronization and Channel Coding—Summary of
 *   Concept and Rationale*, Green Book — the worked examples.
 *
 * Section numbers below are cited from **131.0-B-3** (September 2017), which
 * is the issue that could be read in full while writing this; it is marked
 * HISTORICAL and superseded by B-5. The coding itself is unchanged between
 * them — B-5's additions are a turbo channel interleaver and a reorganisation
 * of slicing — but **a section number is not a value to trust across an
 * issue.** Re-check any citation here against the issue in hand before
 * relying on it.
 *
 * CCSDS is the prototype because one frame exercises every element at once,
 * and because its stages disagree about what they cover — which is the
 * property a fixed pipeline cannot express:
 *
 * ```text
 *   transfer frame
 *     -> RS(255,223) E=16, interleave depth I   expands, interleaves  (4.3)
 *     -> pseudo-randomiser                      length-preserving     (10)
 *     -> ASM 0x1ACFFC1D prepended               NOT randomised        (9)
 *     -> convolutional K=7 r=1/2                expands               (4.1)
 * ```
 *
 * The ASM is the reason the assembler reports a span per stage
 * (@ref ccsds_tm_frame_layout_t, in ccsds_tm_frame.h) rather than a stage
 * order. The
 * randomiser is scoped to "the codeblock, codeword, or Transfer Frame"
 * (10.4.2) and the ASM merely *precedes* the codeblock (9.4.1), so it falls
 * outside — stated outright in a NOTE: *"The ASM was not randomized and is
 * not derandomized."* A chain of optional stages applied to "the frame" is
 * therefore wrong at exactly one stage boundary and right everywhere else.
 *
 * That one boundary is worth the care: 8.2.2.2's NOTE, discussing the LDPC
 * CSM, contains the phrase "the ASM is randomized", which reads as the
 * opposite. It is loose wording about why a CSM and an ASM pattern do not
 * collide at the codeblock synchronization level, and the two explicit NOTEs
 * govern. A convention this easy to read backwards is exactly the kind a
 * round-trip test agrees with and a published vector refuses.
 *
 * **Every kernel here is falsified by a published vector, not by a round
 * trip.** Encode-then-decode agrees with itself for a great many wrong
 * implementations — a mis-ordered tap, a swapped generator polynomial, the
 * wrong field representation — because the decoder inverts whatever the
 * encoder did. The check that bites is the one the standard prints.
 *
 * Bit convention: every function here takes and returns **unpacked** bits,
 * one per byte in the LSB, which is what `wfm_frame_bits`, `dp_crc16_ccitt`
 * and the spreader already pass around. Packed byte streams are a separate
 * (and wanted) representation; conflating them silently is how MSB-first came
 * to be hardcoded in three places that agree by luck.
 *
 * @see ccsds_tm_frame.h for the assembler, which is where the four stages meet
 * and where the packed/unpacked boundary is crossed.
 */
#ifndef CCSDS_TM_H
#define CCSDS_TM_H

#include "conv/conv_core.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief The CCSDS Attached Sync Marker, `0x1ACFFC1D`.
   *
   * 32 bits, transmitted MSB-first, prepended AFTER randomisation. A receiver
   * correlates against it to find the frame, which is precisely why it must
   * not be randomised — it has to look the same in every frame.
   */
#define FEC_CCSDS_ASM 0x1ACFFC1DuL

  /** @brief Length of @ref FEC_CCSDS_ASM in bits. */
#define CCSDS_TM_ASM_BITS 32

  /**
   * @brief Period of the pseudo-randomising sequence, in bits (10.4.2).
   *
   * An 8-stage maximal-length generator, so 255 and not 256. It is named
   * because it is what lets a consumer XOR the sequence onto a run of any
   * length from a fixed 255-entry table instead of holding one the size of
   * the data — `test_ccsds_tm_rand.c` pins that equivalence against
   * @ref ccsds_tm_randomise rather than leaving it as arithmetic a reader
   * has to trust.
   */
#define CCSDS_TM_RAND_PERIOD 255

  /**
   * @brief Constraint length of the inner code (3.3.1): 7.
   *
   * `K - 1` is the encoder's memory in bits, and that is the quantity a
   * caller reasons with: it is how far into a frame a restarted register can
   * still be wrong, and how much of a stream a decoder needs before its state
   * is determined by the data rather than by where it started.
   */
#define FEC_CONV_K 7

  /**
   * @brief Write the ASM as @ref CCSDS_TM_ASM_BITS unpacked bits.
   *
   * Figure 9-1 numbers the first transmitted bit of the marker as the most
   * significant bit of `0x1A`, so `out[0]` is that bit and `out[31]` is the
   * least significant bit of `0x1D`.
   *
   * It is a function rather than a table because the marker is wanted at both
   * ends — the assembler prepends it, a receiver correlates against it — and
   * an MSB-first expansion written out twice is a transcription that can
   * disagree with itself. One expression, one direction.
   *
   * @param out  Receives @ref CCSDS_TM_ASM_BITS bits, one per byte.
   */
  void ccsds_tm_asm_bits (uint8_t *out);

  /**
   * @brief Where an ASM was found, and in which polarity.
   *
   * @c inverted is not a curiosity. A BPSK carrier recovered by a loop with a
   * 180-degree ambiguity delivers the whole stream complemented, and the
   * marker is the only thing in a CADU that can say so — the randomiser does
   * not cover it, so it looks the same in every frame and in exactly one
   * polarity.
   */
  typedef struct
  {
    size_t   offset;   /**< Bit index where the marker starts       */
    int      inverted; /**< The stream is complemented              */
    unsigned errors;   /**< Hamming distance to the marker there    */
  } ccsds_tm_asm_hit_t;

  /**
   * @brief Find the first ASM in a run of unpacked bits, either polarity.
   *
   * Correlates @ref FEC_CCSDS_ASM against every bit offset and against its
   * complement, and reports the **first** offset whose Hamming distance is at
   * most @p max_errors.
   *
   * First rather than best, and the difference matters: a best-match search
   * has to see the whole stream before it can answer, which a frame
   * synchroniser reading a live capture cannot do. First-below-threshold is
   * what is implementable in both settings, so it is what this promises.
   * @p max_errors is the whole of the trade — 0 finds only a clean marker and
   * misses a frame the channel touched, while a value near half the marker
   * length invites a false hit on random data.
   *
   * @param bits        Unpacked bits, one per byte.
   * @param n_bits      Number of bits.
   * @param max_errors  Largest tolerated Hamming distance, in bits.
   * @param hit         Receives the location; untouched when nothing matched.
   * @return            Non-zero if a marker was found.
   *
   * @code
   * uint8_t       cadu[32 + 64] = { 0 };
   * ccsds_tm_asm_hit_t hit;
   * ccsds_tm_asm_bits (cadu);
   * if (ccsds_tm_asm_find (cadu, sizeof cadu, 4u, &hit))
   *   printf ("marker at bit %zu\n", hit.offset);   // marker at bit 0
   * @endcode
   */
  int ccsds_tm_asm_find (const uint8_t *bits, size_t n_bits,
                          unsigned max_errors, ccsds_tm_asm_hit_t *hit);

  /**
   * @brief Apply the CCSDS pseudo-randomiser to a bit run, in place.
   *
   * 131.0-B-3 section 10.4.1: an 8-stage generator over
   * `h(x) = x^8 + x^7 + x^5 + x^3 + 1`, XORed bit-for-bit onto the data. It
   * is its own inverse, so the receive side calls the same function.
   *
   * Two properties from 10.4.2 that a caller can get wrong: the generator is
   * **initialised to all ones at the start of each** codeblock, codeword or
   * Transfer Frame — not once per stream — and the sequence **repeats after
   * 255 bits**. Both are handled here because this function owns a whole run;
   * a caller that chunks its data and calls this per chunk would restart the
   * sequence at every chunk boundary and produce a frame no receiver can
   * derandomise.
   *
   * Its ABSENCE is a measurement hazard rather than a missing feature: a PN
   * payload is already maximally random, so a test built on one cannot tell a
   * present randomiser from a missing one. A run of constant data can, which
   * is why the test for this uses zeros.
   *
   * @param bits  Unpacked bits (one per byte, LSB); modified in place.
   * @param n     Number of bits.
   *
   * @code
   * uint8_t frame[1784] = { 0 };
   * ccsds_tm_randomise (frame, sizeof frame);   // now the published sequence
   * ccsds_tm_randomise (frame, sizeof frame);   // ...and back to zeros
   * @endcode
   */
  void ccsds_tm_randomise (uint8_t *bits, size_t n);

  /**
   * @brief The CCSDS inner code, as a @ref conv_code_t.
   *
   * 131.0-B-3 section 3.3.1: the non-systematic rate-1/2 K = 7 code with
   * `G1 = 1111001` (171 octal), `G2 = 1011011` (133 octal), and — the part
   * that is easy to miss — **symbol inversion on the output path of G2**,
   * which is why `invert` is `0x2` and not `0`.
   *
   * This is a **configuration, not an implementation**: `conv_encode` and
   * `viterbi_decode` do the work and neither knows anything about CCSDS. A
   * standard choosing a code is a different fact from the code existing, and
   * keeping them apart is what stops the polynomials from being written down
   * twice — once in an encoder and once in a decoder, where the inversion is
   * exactly the detail that would drift.
   *
   * `test_ccsds_tm_conv.c` holds it to the standard's printed impulse
   * response: C1 must trace `G1`, and C2 the **complement** of `G2`.
   *
   * @code
   * conv_enc_t s;
   * conv_enc_init (&s);
   * conv_encode (&s, &CCSDS_TM_CONV, bits, n, sym, sizeof sym);
   * @endcode
   */
  extern const conv_code_t CCSDS_TM_CONV;

  /** @brief Symbols the CCSDS inner code writes for @p n input bits. */
  static inline size_t
  ccsds_tm_conv_max_out (size_t n)
  {
    return 2u * n;
  }

  /**
   * @brief Generate the first @p n bits of the randomiser sequence.
   *
   * Exposed separately because the sequence itself is what CCSDS 131.0-B
   * publishes (`FF 48 0E C0 9A ...`), so this is the surface a vector test
   * can check directly rather than inferring it from a XOR.
   *
   * @param out  Receives @p n unpacked bits.
   * @param n    Number of bits to generate.
   */
  void ccsds_tm_rand_seq (uint8_t *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_TM_H */
