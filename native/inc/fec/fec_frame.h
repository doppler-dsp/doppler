/**
 * @file fec_frame.h
 * @brief The CCSDS frame assembler — where the ASM goes, and the one place
 * the stages' disagreements about what they cover become visible.
 *
 * The four kernels in `fec/` are each separately falsifiable against a
 * published value, and each of them is *right* on its own. What none of them
 * can be wrong about alone is the thing this file exists for: **the stages do
 * not all cover the same bits.**
 *
 * ```text
 *   transfer frame                      223 * I octets
 *     -> R-S (255,223) E=16, depth I    4.3, 4.4.1  -> codeblock
 *     -> pseudo-randomiser              10.3.2      -> randomised codeblock
 *     -> ASM 0x1ACFFC1D prepended       9.4.1       -> CADU (table 9-1)
 *     -> convolutional K=7, r=1/2       3.2.1       -> channel symbols
 * ```
 *
 * Read as a pipeline that is four stages long and correct. Read as *what each
 * stage covers* it is not, because the marker enters third and one of the two
 * stages after it reaches back over it:
 *
 * | stage                     | covers the ASM | 131.0-B-3          |
 * | ------------------------- | -------------- | ------------------ |
 * | Reed-Solomon (outer)      | no             | 9.5.1, 9.2.1.5     |
 * | pseudo-randomiser         | no             | 10.3.2, 10.3.4 n.1 |
 * | convolutional (inner)     | **yes**        | 3.2.1, 9.2.1.4     |
 *
 * 9.2.1.5 states both halves in one sentence — *"the ASM shall be encoded by
 * the inner code but not by the outer code"* — and 10.3.4's first NOTE states
 * the third outright: *"The ASM was not randomized and is not derandomized."*
 *
 * That is why @ref fec_frame_layout_t reports a **span per stage** rather
 * than an order. An order is the representation that cannot express this: any
 * chain of optional transforms applied to "the frame" is right at three stage
 * boundaries and wrong at the fourth, and wrong in the direction that still
 * encodes, still decodes against itself, and syncs to nothing. A span makes
 * the disagreement a value a test can assert, which is what
 * `test_fec_ccsds_frame` does against all three rows above.
 *
 * ## The packed/unpacked boundary lives here
 *
 * `fec_rs.h` takes **packed** symbols, because a Reed-Solomon symbol is a
 * byte; `fec_ccsds.h` takes **unpacked** bits, one per byte, because a
 * randomiser and a convolutional coder are bit machines. Both are right, and
 * the conversion between them belongs to exactly one place rather than being
 * hidden inside a kernel that then only works for one caller.
 *
 * This is that place: @ref fec_frame_encode takes a Transfer Frame as packed
 * octets and returns unpacked channel symbols, the representation
 * `wfm_frame_bits` and the spreader already pass around. Octets go on the
 * wire **MSB-first** — figure 9-1 numbers the first transmitted bit of the
 * ASM as the most significant bit of `0x1A`, and 4.3.9.2 orders an R-S symbol
 * the same way.
 *
 * ## What is not here
 *
 * Virtual fill (4.4.2's shortened codeblock) is not implemented, so a frame
 * whose length is not exactly `223 * I` octets is **refused** rather than
 * padded. Silently padding would produce a codeblock a receiver configured
 * for the full length cannot parse, which is the failure this whole slice is
 * built to avoid.
 *
 * @see fec_ccsds.h for the ASM pattern, the randomiser and the inner code.
 * @see fec_rs.h for the outer code and the interleaver.
 */
#ifndef FEC_FRAME_H
#define FEC_FRAME_H

#include "fec/fec_ccsds.h"
#include "fec/fec_rs.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Which coding is applied to one Transfer Frame.
   *
   * Every stage is optional because the standard makes it so: 9.2.1.1 has
   * ASMs between Transfer Frames with no coding at all, 3.2.2 makes the
   * randomiser conditional on the system designer, and Reed-Solomon and the
   * convolutional code are separate sections a mission selects between. The
   * combination of all four is *concatenated coding* (section 5), which is
   * the case the tests exercise end to end.
   */
  typedef struct
  {
    unsigned rs_depth;      /**< Interleaving depth; 0 for no outer code */
    int      randomise;     /**< Apply the section-10 pseudo-randomiser */
    int      attach_asm;    /**< Prepend the ASM, making the unit a CADU */
    int      convolutional; /**< Apply the section-3 inner code */
  } fec_frame_cfg_t;

  /**
   * @brief A run of CADU bits, as a half-open range `[first, first + n)`.
   *
   * A stage that did not run reports `n == 0`, and its `first` is then zero
   * as well; `first` is meaningful only for a stage that ran.
   */
  typedef struct
  {
    size_t first; /**< First CADU bit index the stage covers */
    size_t n;     /**< Bits covered, or 0 if the stage did not run */
  } fec_frame_span_t;

  /**
   * @brief The shape of one CADU, and what each stage covered.
   *
   * The three stage spans are the point: @ref inner starts at bit 0 while
   * @ref outer and @ref randomised start after the marker, and that single
   * difference is the whole content of 9.2.1.5 and 10.3.4.
   */
  typedef struct
  {
    size_t block_bits; /**< The codeblock — or the frame, with no outer code */
    size_t cadu_bits;  /**< Marker plus block, i.e. the whole CADU */
    size_t out_bits;   /**< Channel symbols @ref fec_frame_encode writes */

    fec_frame_span_t marker;     /**< The ASM itself (9.4.1) */
    fec_frame_span_t outer;      /**< The R-S encoded data space (9.5.1) */
    fec_frame_span_t randomised; /**< What the randomiser covered (10.3.2) */
    fec_frame_span_t inner;      /**< What the inner code covered (3.2.1) */
  } fec_frame_layout_t;

  /**
   * @brief Work out the CADU shape for a config, without encoding anything.
   *
   * This is both the buffer-sizing call and the description of the coverage
   * the encoder will apply, which is deliberate: a caller that sizes its
   * buffer from one function and reasons about coverage from a comment is a
   * caller whose two beliefs can drift apart.
   *
   * @param cfg        The coding to apply.
   * @param frame_len  Transfer Frame length in **octets**.
   * @param out        Receives the layout; may be `NULL` to ask only for the
   *                   output length.
   * @return The number of channel symbols @ref fec_frame_encode will write,
   *         or 0 if the configuration is refused — an interleaving depth
   *         outside 4.3.5.1's `{1, 2, 3, 4, 5, 8}`, an empty frame, or a
   *         frame that is not exactly `FEC_RS_K * rs_depth` octets when the
   *         outer code is in use.
   *
   * @code
   * const fec_frame_cfg_t cfg
   *     = { .rs_depth = 5, .randomise = 1, .attach_asm = 1,
   *         .convolutional = 1 };
   * const size_t n = fec_frame_layout (&cfg, 223 * 5, NULL);
   * uint8_t *sym = malloc (n);          // n == (32 + 255 * 5 * 8) * 2
   * @endcode
   */
  size_t fec_frame_layout (const fec_frame_cfg_t *cfg, size_t frame_len,
                           fec_frame_layout_t *out);

  /**
   * @brief Encode one Transfer Frame into channel symbols.
   *
   * Runs whichever of the four stages @p cfg selects, each over the bits it
   * covers and no others.
   *
   * @param cfg        The coding to apply.
   * @param frame      @p frame_len **packed** octets, MSB-first on the wire.
   * @param frame_len  Transfer Frame length in octets.
   * @param out        Receives the **unpacked** channel symbols, one per
   *                   byte; must hold at least @ref fec_frame_layout bytes.
   * @return The number of symbols written, or 0 if the configuration is
   *         refused — in which case @p out is untouched.
   *
   * @code
   * uint8_t frame[223 * 5];
   * uint8_t sym[(32 + 255 * 5 * 8) * 2];
   * const fec_frame_cfg_t cfg
   *     = { .rs_depth = 5, .randomise = 1, .attach_asm = 1,
   *         .convolutional = 1 };
   * const size_t n = fec_frame_encode (&cfg, frame, sizeof frame, sym);
   * @endcode
   */
  size_t fec_frame_encode (const fec_frame_cfg_t *cfg, const uint8_t *frame,
                           size_t frame_len, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* FEC_FRAME_H */
