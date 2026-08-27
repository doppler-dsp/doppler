/**
 * @file ccsds_tm_frame.h
 * @brief The CCSDS frame assembler — where the ASM goes, and the one place
 * the stages' disagreements about what they cover become visible.
 *
 * The four kernels in `ccsds_tm/` are each separately falsifiable against a
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
 * That is why @ref ccsds_tm_frame_layout_t reports a **span per stage** rather
 * than an order. An order is the representation that cannot express this: any
 * chain of optional transforms applied to "the frame" is right at three stage
 * boundaries and wrong at the fourth, and wrong in the direction that still
 * encodes, still decodes against itself, and syncs to nothing. A span makes
 * the disagreement a value a test can assert, which is what
 * `test_ccsds_tm_frame` does against all three rows above.
 *
 * ## The packed/unpacked boundary lives here
 *
 * `ccsds_tm_rs.h` takes **packed** symbols, because a Reed-Solomon symbol is a
 * byte; `ccsds_tm.h` takes **unpacked** bits, one per byte, because a
 * randomiser and a convolutional coder are bit machines. Both are right, and
 * the conversion between them belongs to exactly one place rather than being
 * hidden inside a kernel that then only works for one caller.
 *
 * This is that place: @ref ccsds_tm_frame_encode takes a Transfer Frame as
 * packed octets and returns unpacked channel symbols, the representation
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
 * @see ccsds_tm.h for the ASM pattern, the randomiser and the inner code.
 * @see ccsds_tm_rs.h for the outer code and the interleaver.
 */
#ifndef CCSDS_TM_FRAME_H
#define CCSDS_TM_FRAME_H

#include "ccsds_tm/ccsds_tm.h"
#include "ccsds_tm/ccsds_tm_rs.h"
#include "wfm/wfm_frame.h"

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
  } ccsds_tm_frame_cfg_t;

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
  } ccsds_tm_frame_span_t;

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
    size_t out_bits; /**< Channel symbols the encode writes         */

    ccsds_tm_frame_span_t marker;     /**< The ASM itself (9.4.1) */
    /** The R-S encoded data space (9.5.1)   */
    ccsds_tm_frame_span_t outer;
    /** What the randomiser covered (10.3.2) */
    ccsds_tm_frame_span_t randomised;
    /** What the inner code covered (3.2.1)  */
    ccsds_tm_frame_span_t inner;
  } ccsds_tm_frame_layout_t;

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
   * @return The number of channel symbols the encode will write,
   *         or 0 if the configuration is refused — an interleaving depth
   *         outside 4.3.5.1's `{1, 2, 3, 4, 5, 8}`, an empty frame, or a
   *         frame that is not exactly `CCSDS_TM_RS_K * rs_depth` octets when
   *         the
   *         outer code is in use.
   *
   * @code
   * const ccsds_tm_frame_cfg_t cfg
   *     = { .rs_depth = 5, .randomise = 1, .attach_asm = 1,
   *         .convolutional = 1 };
   * const size_t n = ccsds_tm_frame_layout (&cfg, 223 * 5, NULL);
   * uint8_t *sym = malloc (n);          // n == (32 + 255 * 5 * 8) * 2
   * @endcode
   */
  size_t ccsds_tm_frame_layout (const ccsds_tm_frame_cfg_t *cfg,
                               size_t              frame_len,
                           ccsds_tm_frame_layout_t *out);

  /**
   * @brief Encode one Transfer Frame into channel symbols.
   *
   * Runs whichever of the four stages @p cfg selects, each over the bits it
   * covers and no others.
   *
   * ## The inner encoder belongs to the CALLER, because it is continuous
   *
   * 3.3.2 fixes the output as one uninterrupted symbol sequence with no
   * per-frame flush, so the register carries from the last bit of one CADU
   * into the first bit of the next. @p conv is where it lives. Pass the same
   * one to every call in a stream; pass `NULL` for a frame encoded on its own.
   *
   * The difference is small and it is exactly where it hurts: measured on
   * depth 1, encoding two frames with `NULL` differs from the continuous
   * stream in **6 of 8288 symbols**, all of them in the first 7 symbols of
   * frame 2 — the `K - 1 = 6` bits of register memory, landing on the ASM a
   * receiver is trying to correlate. A matched Viterbi absorbs it, which is
   * what makes this the same class as the inversion on G2 and the dual basis:
   * self-consistent, decodes against a receiver of one's own construction,
   * and not what the standard says.
   *
   * @param cfg        The coding to apply.
   * @param conv       Inner-encoder state (@ref conv_enc_t) carried across
   *                   frames, or `NULL` to
   *                   start from the all-zero register. Ignored when
   *                   `cfg->convolutional` is 0.
   * @param frame      @p frame_len **packed** octets, MSB-first on the wire.
   * @param frame_len  Transfer Frame length in octets.
   * @param out        Receives the **unpacked** channel symbols, one per byte.
   * @param max_out    Capacity of @p out in symbols. The CADU is assembled in
   *                   the TAIL of this buffer, so a short one is not a
   *                   truncated result but a write past the end — hence a
   *                   capacity rather than a comment telling you to call
   *                   @ref ccsds_tm_frame_layout first.
   * @return The number of symbols written, or 0 if the configuration is
   *         refused or @p max_out is too small — in which case @p out is
   *         untouched.
   *
   * @code
   * uint8_t frame[223 * 5];
   * uint8_t sym[(32 + 255 * 5 * 8) * 2];
   * const ccsds_tm_frame_cfg_t cfg
   *     = { .rs_depth = 5, .randomise = 1, .attach_asm = 1,
   *         .convolutional = 1 };
   * conv_enc_t conv;
   * conv_enc_init (&conv);
   * const size_t n
   *     = ccsds_tm_frame_encode (&cfg, &conv, frame, sizeof frame, sym,
   *                         sizeof sym);
   * @endcode
   */
  size_t ccsds_tm_frame_encode (const ccsds_tm_frame_cfg_t *cfg,
                               conv_enc_t               *conv,
                           const uint8_t *frame, size_t frame_len,
                           uint8_t *out, size_t max_out);

  /**
   * @brief What @ref ccsds_tm_frame_decode found on the way through.
   *
   * The outer code **corrects** (`rs/rs_core.h`), so @ref rs_ok counts the
   * codewords that are good *afterwards* — clean or repaired — and
   * `rs_ok < rs_codewords` means the returned frame is **wrong in a way this
   * function knows about**: at least one codeword was too far from any
   * codeword to name. That is reported rather than folded into the return
   * value because a caller doing frame accounting wants the count, and a
   * caller wanting only good frames can compare the two.
   *
   * @ref rs_corrected and @ref rs_symbols are the work the outer code
   * actually did. They are the honest measure of how hard the link is
   * running: `rs_ok == rs_codewords` with a rising @ref rs_symbols is a
   * margin being spent, and it is spent before it is lost.
   */
  typedef struct
  {
    size_t   frame_len;    /**< Transfer Frame octets written             */
    unsigned rs_codewords; /**< Codewords decoded; 0 with no outer code   */
    unsigned rs_ok;        /**< How many are valid after decoding         */
    unsigned rs_corrected; /**< How many of those needed repair           */
    unsigned rs_symbols;   /**< Symbol errors repaired across the block   */
  } ccsds_tm_frame_rx_t;

  /**
   * @brief Recover a Transfer Frame from the bits of one CADU.
   *
   * The mirror of @ref ccsds_tm_frame_encode, over the same spans and reading
   * the same @ref ccsds_tm_frame_cfg_t — so the two cannot disagree about
   * which stage covered what, which is the failure `ccsds_tm_frame.h` opens
   * by describing.
   *
   * ## Where the inner code is, and why it is not here
   *
   * This begins **after** the inner decode and after frame synchronisation:
   * @p cadu is one marker-plus-codeblock, already Viterbi-decoded and already
   * aligned by @ref ccsds_tm_asm_find. That is not an omission, it is the
   * only place the boundary can go. A Viterbi is streaming and emits its
   * decisions `depth` bits late, so the bits of one CADU are not a function
   * of that CADU's symbols alone; and the marker that says where a CADU
   * *starts* is only readable once the inner code has been undone. A function
   * taking channel symbols would therefore have to own a decoder, a search
   * window and a buffer — that is a streaming receiver object, and this is
   * the pure per-frame chain it would call.
   *
   * Consistent with the encoder, where @ref conv_enc_t belongs to the caller
   * for the same reason: the inner code is continuous and the frame is not.
   *
   * ## What it undoes
   *
   * The marker is skipped, the randomiser is re-applied over the block span
   * (10.3.4 — it is involutive, so the same call serves both directions), the
   * block is packed back to octets MSB-first, and with an outer code each of
   * the @c rs_depth interleaved codewords is **decoded** — up to `E = 16`
   * symbol errors per codeword repaired, in place, before anything reads the
   * frame. The Transfer Frame is the information section, which 4.4.1 keeps
   * in the order it entered.
   *
   * @param cfg       The coding that was applied. Must match the transmitter.
   * @param cadu      @p n_cadu unpacked CADU bits, one per byte.
   * @param n_cadu    Number of CADU bits; must equal the layout's
   *                  `cadu_bits` for this configuration.
   * @param frame     Receives the recovered Transfer Frame, packed octets.
   * @param max_frame Capacity of @p frame in octets.
   * @param rx        Receives what was found; may be `NULL`.
   * @return          Transfer Frame octets written, or 0 if the configuration
   *                  is refused, @p n_cadu is not the layout's CADU length,
   *                  or @p max_frame is too small — in which case @p frame is
   *                  untouched.
   *
   * @code
   * const ccsds_tm_frame_cfg_t cfg
   *     = { .rs_depth = 5, .randomise = 1, .attach_asm = 1,
   *         .convolutional = 1 };
   * ccsds_tm_frame_layout_t lay;
   * ccsds_tm_frame_layout (&cfg, 223 * 5, &lay);
   *
   * uint8_t        frame[223 * 5];
   * ccsds_tm_frame_rx_t rx;
   * // `cadu` is lay.cadu_bits of Viterbi output, ASM-aligned.
   * const size_t n = ccsds_tm_frame_decode (&cfg, cadu, lay.cadu_bits, frame,
   *                                    sizeof frame, &rx);
   * printf ("%zu octets, R-S %u/%u ok, %u symbols repaired\n", n, rx.rs_ok,
   *         rx.rs_codewords, rx.rs_symbols);
   * @endcode
   */
  size_t ccsds_tm_frame_decode (const ccsds_tm_frame_cfg_t *cfg,
                               const uint8_t            *cadu,
                           size_t n_cadu, uint8_t *frame, size_t max_frame,
                           ccsds_tm_frame_rx_t *rx);

  /**
   * @brief This CADU as a @ref wfm_frame_desc_t — the standard as DATA.
   *
   * The same fact `CCSDS_TM_CONV` states about the inner code and
   * `CCSDS_TM_RS` about the outer one, at the level of the frame: 131.0-B-3
   * section 9 is a CONFIGURATION of a general description, not a framer of
   * its own. Three fields — the marker, the Transfer Frame, and the check
   * symbols the outer code derives — and three stages whose covers are the
   * whole of the coverage table this file opens with.
   *
   * The dependency runs THIS way on purpose. `wfm/wfm_frame.h` knows nothing
   * about CCSDS; if it called this component's kernels the two would form a
   * cycle, so the kernels travel as @ref ccsds_tm_frame_ops instead.
   *
   * @param cfg         the coding to apply.
   * @param frame_len   Transfer Frame length in **octets**.
   * @param frame_bits  `frame_len * 8` **unpacked** Transfer Frame bits,
   *                    MSB-first — the representation the description works
   *                    in, so the packed/unpacked boundary is crossed by the
   *                    caller and is visible rather than hidden in a kernel.
   *                    May be `NULL` to describe the geometry alone.
   * @param out         receives the description.
   * @return 0, or -1 if the configuration is refused — the same refusals
   *         @ref ccsds_tm_frame_layout applies, for the same reasons.
   */
  int ccsds_tm_frame_describe (const ccsds_tm_frame_cfg_t *cfg,
                               size_t frame_len, const uint8_t *frame_bits,
                               wfm_frame_desc_t *out);

  /**
   * @brief A framed waveform's choices, before they become a description.
   *
   * The wider family `ccsds_tm_frame_describe` is the CADU case of: a frame
   * that may open with a marker, may carry a preamble and a sync word a
   * receiver FINDS, carries a payload, and applies some subset of this
   * standard's four stages to it.
   *
   * **The fields are the caller's; the COVERS are the standard's**, which is
   * the whole reason this lives here rather than in `wfm/wfm_frame.h`. That
   * header knows what a field and a stage are and nothing about which covers
   * which — a general description cannot, because the answer is a
   * specification's:
   *
   *     marker / preamble / sync   found, not decoded — covered by the inner
   *                                code alone, because all three must look
   *                                the same in every frame to be findable
   *     payload / crc / parity     the data group — what the outer code and
   *                                the randomiser reach over
   *     everything                 the inner code
   *
   * The middle row is 10.3.4 generalised: the randomiser does not cover the
   * ASM, and the reason the standard gives — a marker a receiver correlates
   * against must not vary between frames — is exactly as true of a preamble
   * and a sync word.
   *
   * A receiver does NOT hold one of these. It stops at hard and soft
   * decisions; the frame is undone one layer up, by whoever holds the
   * description.
   */
  typedef struct
  {
    /** Non-zero: the frame opens with the ASM (0x1ACFFC1D). */
    int attach_asm;
    /** Preamble, repeated @p preamble_reps times; NULL for none. */
    const uint8_t *preamble;
    size_t         preamble_len;
    size_t         preamble_reps;
    /** Frame-sync word — what a receiver correlates to find the payload. */
    const uint8_t *sync;
    size_t         sync_len;
    /** The payload. May be NULL when only the geometry is wanted. */
    const uint8_t *payload;
    size_t         payload_len;
    /** Non-zero: a CRC-16 trailer over the payload group. */
    int crc;
    /** Outer-code interleaving depth; 0 = no outer code. */
    unsigned rs_depth;
    /** Randomiser generator: 0 = off, else 10.4.1 (1) or 10.4.2 (2). */
    int randomise;
    /** Non-zero: the K=7 rate-1/2 inner code, over the whole frame. */
    int convolutional;

    /** A BLOCK INTERLEAVER over the data group; 0 = none.
     *
     * Not a CCSDS pick, and here for the reason @c crc and @c sync are: this
     * spec is the one frame description every doppler face reaches, and
     * 131.0-B-6's own choices are a CONFIGURATION of it rather than the whole
     * of its vocabulary.
     *
     * Applied AFTER the outer code and the randomiser and before the inner
     * code, which is the only order that buys anything: an interleaver exists
     * so a burst on the CHANNEL arrives spread across the outer code's
     * codewords, so it must be the last thing between them and the wire. */
    unsigned interleave_depth;
    /** Bits per permuted unit; 0 reads as 1. Match it to the outer code's
     *  symbol — 8 for Reed-Solomon over GF(256), because permuting bits
     *  inside a symbol that is already wrong buys nothing. */
    unsigned interleave_unit_bits;
  } ccsds_tm_frame_spec_t;

  /**
   * @brief Turn those choices into a description: fields, stages, covers.
   *
   * The ONE place this standard's coverage table becomes data, so a
   * generator and whatever undoes the frame later hold the same layout
   * rather than each deriving one.
   *
   * @param s  the choices.
   * @param d  receives the description.
   * @return 0, or -1 on NULL, or if the geometry needs more fields or
   *         stages than a description holds.
   */
  int ccsds_tm_frame_desc_of (const ccsds_tm_frame_spec_t *s,
                              wfm_frame_desc_t *d);

  /**
   * @brief The kernels a described CADU is assembled with.
   *
   * The outer code, the randomiser and the inner code, as the transforms
   * @ref wfm_frame_assemble calls. Each one is the same function
   * @ref ccsds_tm_frame_encode calls, so the two paths cannot come to
   * disagree about what a stage does — only about which bits it is handed,
   * and that is what the description states.
   *
   * @param out   receives the table.
   * @param conv  inner-encoder state carried across frames, or `NULL` to
   *              start each frame from the all-zero register. Exactly
   *              @ref ccsds_tm_frame_encode's @p conv, and it matters for
   *              the same reason: 3.3.2 fixes one uninterrupted symbol
   *              sequence.
   */
  void ccsds_tm_frame_ops (wfm_frame_ops_t *out, conv_enc_t *conv);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_TM_FRAME_H */
