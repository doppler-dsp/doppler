/**
 * @file fec_ccsds.h
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
 * The ASM is the reason `covers` exists rather than a stage order. The
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
 * @see docs/design/framing.md
 */
#ifndef FEC_CCSDS_H
#define FEC_CCSDS_H

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
#define FEC_CCSDS_ASM_BITS 32

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
   * fec_ccsds_randomise (frame, sizeof frame);   // now the published sequence
   * fec_ccsds_randomise (frame, sizeof frame);   // ...and back to zeros
   * @endcode
   */
  void fec_ccsds_randomise (uint8_t *bits, size_t n);

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
  void fec_ccsds_rand_seq (uint8_t *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* FEC_CCSDS_H */
