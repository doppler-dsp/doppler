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
 * - **CCSDS 131.0-B-6**, *TM Synchronization and Channel Coding*, Blue Book,
 *   April 2026 (errata corrected 1) — the current issue.
 * - **CCSDS 130.1-G-3**, *TM Synchronization and Channel Coding—Summary of
 *   Concept and Rationale*, Green Book, June 2020 — the rationale, and three
 *   issues behind the Blue Book it explains.
 *
 * Most section numbers below are still cited from **131.0-B-3** (September
 * 2017), which is the issue this component was written against. That is a
 * known debt, tracked as gh-865, and it is exactly what the next paragraph
 * warns about: **a section number is not a value to trust across an issue.**
 *
 * B-6 has been read, and the numbers moved while the content did not —
 * 4.3.9 became 5.3.9, 4.4.1 became 5.4.1, 4.4.2 became 5.3.8.2, while figure
 * 9-1 and section 10.3 held. What was checked against it directly and found
 * unchanged: both dual-basis matrices, the sync marker pattern, the legacy
 * randomiser's published prefix, and the field polynomial.
 *
 * Two things B-6 DID change, and only one is adopted here:
 *
 * - **the randomiser default is now the 131071-bit sequence** (10.4.1), and
 *   the 255-bit one is kept only "for backward compatibility with legacy
 *   systems" (10.4.2). Both ship; see @ref CCSDS_TM_RAND.
 * - **the ASM is called the CSM** (Code Sync Marker) throughout. The pattern
 *   is unchanged; the naming here is not, and renaming reaches a CLI flag
 *   and a Python surface. gh-865.
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
#include "dp_syncword.h"
#include "viterbi/viterbi_core.h"

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
#define CCSDS_TM_ASM 0x1ACFFC1DuL

  /** @brief Length of @ref CCSDS_TM_ASM in bits. */
#define CCSDS_TM_ASM_BITS 32

  /**
   * @brief A pseudo-randomiser: a maximal-length generator and its preset.
   *
   * 131.0-B-6 section 10.4 specifies **two**, and which one a mission uses is
   * a choice rather than a property of the coding — so it is a configuration,
   * exactly as the inner code and the outer code are. One implementation
   * serves both; only the table changes.
   *
   * @p taps is a mask over the register: bit `i` set means stage `i` feeds
   * back. The mask is DERIVED from the characteristic polynomial rather than
   * transcribed from its exponents, and the difference is not cosmetic —
   * `rand.c` records that writing the exponents produced a generator which
   * walked to the all-zero fixed point and passed every structural check
   * except the published prefix.
   */
  typedef struct
  {
    uint32_t taps;   /**< feedback mask over @p stages bits           */
    uint32_t seed;   /**< preset, loaded at the start of every run    */
    unsigned stages; /**< register width                              */
    size_t   period; /**< `2^stages - 1`, since both are maximal      */
  } ccsds_tm_rand_t;

  /**
   * @brief The randomiser 131.0-B-6 10.4.1 requires: 131071 bits, degree 17.
   *
   * `h(x) = x^17 + x^14 + 1`, preset `11000111000111000`, and it is the
   * `shall`. **This is the default and what @ref ccsds_tm_randomise applies.**
   *
   * The preset is loaded so the LAST bit of that printed string is emitted
   * FIRST — the string reads along the register in figure 10-2, and the stage
   * that leaves first is the far end. Nothing forced that question before,
   * because the legacy preset is all ones and reads the same either way; the
   * published 40-bit prefix is what settles it, and is what
   * `test_ccsds_tm_rand.c` holds it to.
   */
  extern const ccsds_tm_rand_t CCSDS_TM_RAND;

  /**
   * @brief The randomiser 10.4.2 keeps: 255 bits, degree 8.
   *
   * `h(x) = x^8 + x^7 + x^5 + x^3 + 1`, preset all ones — what every issue
   * through B-3 specified outright, and what B-6 keeps only *"for backward
   * compatibility with legacy systems"*.
   *
   * B-6 says why it stopped being the default, and it is a link-budget
   * matter rather than a coding one: the short period *"may introduce
   * spectral lines at 1/255 of the symbol rate"* and *"could not guarantee
   * full compliance with ITU power flux density limits"*. Reach for it to
   * talk to something old, not to build something new.
   */
  extern const ccsds_tm_rand_t CCSDS_TM_RAND_LEGACY;

  /** @brief Period of the DEFAULT sequence, in bits (10.4.1). */
#define CCSDS_TM_RAND_PERIOD 131071

  /**
   * @brief A generator part-way through a run.
   *
   * Exposed because a consumer that is already walking the data — the frame
   * decoder packs bits to octets and derandomises in the same pass — cannot
   * hand a mutable run to @ref ccsds_tm_randomise, and must not hold a
   * sequence the size of the data either. Stepping the generator alongside
   * costs one word and works for any period; the alternative was a table
   * indexed modulo the period, which is 128 KB at 10.4.1's and is longer than
   * any CADU.
   */
  typedef struct
  {
    uint32_t reg;
    uint32_t taps;
    unsigned stages;
  } ccsds_tm_rand_state_t;

  /**
   * @brief Load @p r's preset, ready to emit its first bit.
   *
   * @param s  Receives the state.
   * @param r  The randomiser; `NULL` selects @ref CCSDS_TM_RAND.
   */
  void ccsds_tm_rand_init (ccsds_tm_rand_state_t *s,
                           const ccsds_tm_rand_t *r);

  /**
   * @brief Emit one bit and advance.
   *
   * @param s  A state from @ref ccsds_tm_rand_init.
   * @return   The next sequence bit, 0 or 1.
   */
  uint8_t ccsds_tm_rand_step (ccsds_tm_rand_state_t *s);

  /**
   * @brief Constraint length of the inner code (3.3.1): 7.
   *
   * `K - 1` is the encoder's memory in bits, and that is the quantity a
   * caller reasons with: it is how far into a frame a restarted register can
   * still be wrong, and how much of a stream a decoder needs before its state
   * is determined by the data rather than by where it started.
   */
#define CCSDS_TM_CONV_K 7

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
   * An alias for @ref dp_syncword_hit_t rather than a struct of its own: two
   * declarations of the same three fields are two things that can drift, and
   * the polarity flag in particular is one a caller reads across the
   * boundary between the general search and this configuration of it.
   *
   * @c inverted is not a curiosity. A BPSK carrier recovered by a loop with a
   * 180-degree ambiguity delivers the whole stream complemented, and the
   * marker is the only thing in a CADU that can say so — the randomiser does
   * not cover it, so it looks the same in every frame and in exactly one
   * polarity.
   */
  typedef dp_syncword_hit_t ccsds_tm_asm_hit_t;

  /**
   * @brief Find the first ASM in a run of unpacked bits, either polarity.
   *
   * `dp_syncword_find` configured with @ref CCSDS_TM_ASM — the standard
   * picks the pattern, and the search is not the standard's. Everything
   * about the search itself, including why it reports the FIRST offset under
   * threshold rather than the best one, is documented there.
   *
   * **Choose @p max_errors against the search window, not the marker
   * length.** Half of 32 is 16, so 8 sounds safe, and 8 finds the marker at
   * its true offset only 58 % of the time on a stream with no channel errors
   * at all (doppler#897). Call `dp_syncword_max_errors` with the window
   * the synchroniser actually reads, or reach for the numbers the
   * certification measured for this marker: **`t = 4` survives both tails,
   * and `t = 6` if the link is bad** — over the 96-bit lead-in §2.3 used.
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
   * Applies @ref CCSDS_TM_RAND — 131.0-B-6 section 10.4.1's degree-17
   * generator, `h(x) = x^17 + x^14 + 1`, preset `11000111000111000`, period
   * 131071 — XORed bit-for-bit onto the data. It is its own inverse, so the
   * receive side calls the same function. Use
   * @ref ccsds_tm_randomise_with to reach 10.4.2's legacy degree-8 sequence
   * instead; the two are not interchangeable on the wire.
   *
   * (This docblock described the LEGACY generator — 8 stages, all-ones
   * preset, 255-bit period — until the `ccsds_tm` certification read it
   * against the code. Every one of those three facts belonged to the other
   * randomiser, and the difference is not academic: measured, the legacy
   * sequence puts a 91 dB line at 1/255 of the symbol rate on constant data
   * where this one puts none, which is precisely why B-6 demoted it. See
   * `src/doppler/tests/validation/ccsds_tm/results.md` §2.4.)
   *
   * Two properties a caller can get wrong: the generator is **reloaded from
   * its preset at the start of each** codeblock, codeword or Transfer Frame
   * (10.4.3) — not once per stream — and the sequence **repeats after its
   * period**. Both are handled here because this function owns a whole run;
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
   * @brief @ref ccsds_tm_randomise with a chosen randomiser.
   *
   * @param r     The randomiser; `NULL` selects @ref CCSDS_TM_RAND.
   * @param bits  Unpacked bits (one per byte, LSB); modified in place.
   * @param n     Number of bits.
   */
  void ccsds_tm_randomise_with (const ccsds_tm_rand_t *r, uint8_t *bits,
                                size_t n);

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

  /**
   * @brief @ref ccsds_tm_rand_seq with a chosen randomiser.
   *
   * @param r    The randomiser; `NULL` selects @ref CCSDS_TM_RAND.
   * @param out  Receives @p n unpacked bits.
   * @param n    Number of bits to generate.
   */
  void ccsds_tm_rand_seq_with (const ccsds_tm_rand_t *r, uint8_t *out,
                               size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_TM_H */
