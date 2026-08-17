/**
 * @file conv_core.h
 * @brief Convolutional codes: the code description, the encoder, and the
 * maximum-likelihood decoder that reads the same description.
 *
 * A rate-1/n convolutional code is four numbers — a constraint length, an
 * output count, a generator polynomial per output, and which outputs are
 * inverted. This file holds that description once and derives everything from
 * it, so an encoder and a decoder cannot disagree about what the code is.
 *
 * ## The one expression
 *
 * @ref conv_outputs is what the family of codes emits, and it is the only
 * place that says so. @ref conv_encode calls it to produce symbols; a Viterbi
 * decoder calls it to build the trellis it searches. An encoder that computed
 * the outputs and a decoder that computed them *again* would be two
 * implementations of one primitive, and the detail that drifts between them is
 * never the arithmetic — it is a convention.
 *
 * CCSDS is the worked example and the warning: 131.0-B-3 inverts the second
 * output and most codes invert nothing. Omitting that inversion produces a
 * code that decodes its own output perfectly and interoperates with nothing;
 * measured on the CCSDS code, a decoder that omits it gets **39.2 % of bits
 * wrong**. As a field of @ref conv_code_t the mistake is a wrong argument. As
 * a constant inside an encoder it is a wrong encoder, and the matching decoder
 * hides it.
 *
 * ## Nothing here is CCSDS
 *
 * The CCSDS configuration lives in `ccsds_tm/ccsds_tm.h` as @c CCSDS_TM_CONV,
 * because a channel-coding standard picking a code is not the same fact as the
 * code existing. Point this at the deep-space rate-1/6 code, at a K = 9
 * experiment, or at whatever a caller brings — the trellis is identical and
 * only the table changes.
 *
 * ## Conventions
 *
 * - **Bits are unpacked**, one per byte in the LSB, matching `wfm_frame_bits`,
 *   `dp_crc16_ccitt` and the `ccsds_tm` kernels.
 * - **The register holds the newest input in the high stage**:
 *   `reg = (reg >> 1) | (b << (k-1))`. A *state* is the `k-1` bits that
 *   survive, so `state + bit -> reg = (bit << (k-1)) | state`, and the next
 *   state is `reg >> 1`. Deriving this the other way round yields a trellis
 *   that is perfectly self-consistent and decodes nothing a conforming encoder
 *   produced, which is why `test_conv_core.c` pins the two against each other
 *   rather than each against itself.
 * - **Polynomials are written as the standard writes them**, left to right
 *   with the newest input at the left: CCSDS's `G1 = 1111001` is `0171`.
 *
 * @see docs/design/viterbi.md for the decoder's design and its measurements.
 */
#ifndef CONV_CORE_H
#define CONV_CORE_H

#include "clib_common.h"
#include "dp_state.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Largest constraint length; 2^(k-1) states, so 256 at k = 9. */
#define CONV_K_MAX 9

  /** @brief Largest number of outputs per input bit (rate 1/n). */
#define CONV_N_MAX 6

  /**
   * @brief A rate-1/n convolutional code.
   *
   * @c invert is a bitmask over outputs, not a flag: bit @c j set means
   * output @c j is transmitted inverted. CCSDS sets bit 1 and nothing else.
   */
  typedef struct
  {
    unsigned k;                  /**< constraint length, 2..CONV_K_MAX   */
    unsigned n;                  /**< outputs per input, 1..CONV_N_MAX   */
    uint32_t poly[CONV_N_MAX];   /**< generator polynomials, k bits each */
    uint32_t invert;             /**< bit j: output j is inverted        */
  } conv_code_t;

  /**
   * @brief Is @p c a code this file can represent?
   *
   * @param c  The code.
   * @return   Non-zero if usable: `k` in `[2, CONV_K_MAX]`, `n` in
   *           `[1, CONV_N_MAX]`, and every polynomial within `k` bits and
   *           non-zero. A zero polynomial is an output that carries no
   *           information, which is a typo rather than a code.
   */
  int conv_code_valid (const conv_code_t *c);

  /** @brief Number of trellis states, `2^(k-1)`. */
  JM_FORCEINLINE uint32_t
  conv_states (const conv_code_t *c)
  {
    return 1u << (c->k - 1u);
  }

  /**
   * @brief The output word for one branch — **the** expression of the code.
   *
   * Output @c j is bit @c j of the result, matching the order the
   * polynomials are given in and the order @ref conv_encode emits them — so
   * for CCSDS, bit 0 is C1 and bit 1 is C2.
   *
   * @param c      The code.
   * @param state  Trellis state: the `k-1` previous input bits.
   * @param bit    The new input bit (0 or 1).
   * @return       `n` bits, output @c j in bit @c j, inversion applied.
   */
  unsigned conv_outputs (const conv_code_t *c, uint32_t state, unsigned bit);

  /** @brief The state reached from @p state on @p bit. */
  JM_FORCEINLINE uint32_t
  conv_next_state (const conv_code_t *c, uint32_t state, unsigned bit)
  {
    return (((bit & 1u) << (c->k - 1u)) | state) >> 1;
  }

  /* ── the encoder ─────────────────────────────────────────────────────── */

  /**
   * @brief Encoder state: the shift register, and nothing else.
   *
   * Held in a struct rather than passed by value because the encoder is
   * **continuous** — a caller encoding a long record in chunks must carry the
   * register across calls or introduce a discontinuity at every chunk
   * boundary that no decoder expects.
   */
  typedef struct
  {
    uint32_t reg; /**< the k-1 previous inputs; newest in the high stage */
  } conv_enc_t;

  /** @brief Reset the encoder to the all-zero state. */
  void conv_enc_init (conv_enc_t *s);

  /**
   * @brief Encode @p n_in bits, emitting `n_in * c->n` symbols.
   *
   * @param s       Encoder state, carried across calls.
   * @param c       The code.
   * @param in      @p n_in unpacked input bits.
   * @param n_in    Number of input bits.
   * @param out     Receives `n_in * c->n` unpacked symbols, outputs in
   *                polynomial order per input bit.
   * @param max_out Capacity of @p out.
   * @return        Symbols written, or 0 if the code is invalid or @p max_out
   *                is too small — in which case @p out is untouched.
   */
  size_t conv_encode (conv_enc_t *s, const conv_code_t *c, const uint8_t *in,
                      size_t n_in, uint8_t *out, size_t max_out);

  /* ── the decoder ─────────────────────────────────────────────────────── */

  /**
   * @brief A streaming maximum-likelihood (Viterbi) decoder.
   *
   * Opaque and heap-allocated: the path metrics and the traceback ring are
   * sized from the code and the depth, and both are wanted contiguous.
   */
  typedef struct viterbi_state_t viterbi_state_t;

  /**
   * @brief Build a decoder for @p c with traceback depth @p depth.
   *
   * @param c      The code. Copied, so the caller's may be temporary.
   * @param depth  Traceback depth in input bits. A decision is emitted only
   *               after this many further bits have been seen, which is the
   *               decoder's latency and the dominant term in its memory.
   *               **60 is the measured choice for CCSDS's K = 7 rate-1/2
   *               code** — `5*K = 35`, the textbook number, sits 33 % above
   *               the achievable BER (docs/design/viterbi.md section 4). It is
   *               a default for other codes, not a law.
   * @return       The decoder, or NULL if @p c is invalid, @p depth is 0, or
   *               allocation failed.
   */
  viterbi_state_t *viterbi_create (const conv_code_t *c, size_t depth);

  /** @brief Free a decoder. NULL is a no-op. */
  void viterbi_destroy (viterbi_state_t *s);

  /**
   * @brief Return to the start state, discarding the traceback history.
   *
   * The all-zero state is given the winning metric, matching an encoder that
   * starts from a reset register.
   */
  void viterbi_reset (viterbi_state_t *s);

  /**
   * @brief Decode soft symbols into bits.
   *
   * @p llr carries one value per channel symbol in the convention
   * `mpsk_soft_demap` produces: `L = log(P(0)/P(1))`, so **positive means
   * symbol 0**. The branch metric for an expected symbol @c e is `+L` when
   * `e == 0` and `-L` otherwise and the survivor maximises the sum, which
   * makes the decoder agree with `mpsk_demap` on hard decisions by
   * construction rather than by a second convention.
   *
   * A maximum-likelihood path cannot move when every metric is scaled by a
   * positive constant, so **the LLRs need no accurate scaling** — a caller
   * with no SNR estimate may pass unscaled values.
   *
   * Streaming: the first `depth` bits of a stream produce no output, and
   * thereafter one bit is emitted per @c n symbols consumed.
   *
   * @param s       The decoder.
   * @param llr     Soft symbols; @p n_llr must be a multiple of `c->n`.
   * @param n_llr   Number of soft symbols.
   * @param out     Receives the decided bits, one per byte.
   * @param max_out Capacity of @p out.
   * @return        Bits written. 0 with nothing written if @p n_llr is not a
   *                multiple of `n`, or if @p max_out cannot hold the bits this
   *                call would emit.
   */
  size_t viterbi_decode (viterbi_state_t *s, const float *llr, size_t n_llr,
                         uint8_t *out, size_t max_out);

  /**
   * @brief Bits @ref viterbi_decode will emit for @p n_llr soft symbols.
   *
   * Accounts for the fill still owed at the start of a stream, so a caller
   * can size a buffer exactly rather than conservatively.
   */
  size_t viterbi_decode_max_out (const viterbi_state_t *s, size_t n_llr);

  /** @brief The code this decoder was built for. */
  const conv_code_t *viterbi_code (const viterbi_state_t *s);

  /** @brief Its traceback depth, in input bits. */
  size_t viterbi_depth (const viterbi_state_t *s);

/* ── the state bytes interface ───────────────────────────────────────────
 *
 * The decoder carries running state across calls — a path metric per state,
 * the traceback ring, and where the ring is — so it speaks the standard
 * bytes interface like every other stateful object in the tree. A decoder
 * sits inside a chain (behind the receiver, in front of the R-S decoder),
 * and one link that cannot be checkpointed is enough to make the chain
 * un-resumable. See docs/design/state-serialization.md.
 */

/** @brief Blob type tag: "VTRB". */
#define VITERBI_STATE_MAGIC DP_FOURCC ('V', 'T', 'R', 'B')
/** @brief Blob format version. */
#define VITERBI_STATE_VERSION 1u

  /**
   * @brief Bytes @ref viterbi_get_state writes: envelope, code identity,
   *        ring cursor, the path metrics and the traceback ring.
   *
   * Depends on the configuration (`2^(k-1)` metrics and a
   * `depth x 2^(k-1)` ring), so it is not a constant across decoders.
   */
  size_t viterbi_state_bytes (const viterbi_state_t *s);

  /**
   * @brief Serialize @p s into @p blob, which must hold
   *        @ref viterbi_state_bytes bytes.
   *
   * The ring travels in its stored order with the cursor beside it rather
   * than rotated into a canonical one — the rotation would cost a pass and
   * buy nothing, since only @ref viterbi_set_state reads it back.
   */
  void viterbi_get_state (const viterbi_state_t *s, void *blob);

  /**
   * @brief Restore @p s from @p blob.
   *
   * The code and the depth are configuration, restored by
   * @ref viterbi_create rather than carried in the payload — but they are
   * *stamped* in it and checked here, because a size match is not a
   * configuration match: two codes with the same `k` and `n` differing only
   * in a polynomial or in @c invert produce blobs of identical length, and
   * reinterpreting one as the other yields a decoder that is confidently
   * wrong rather than one that refuses.
   *
   * @return @c DP_OK, or @c DP_ERR_INVALID if the envelope, the code, the
   *         depth, or the ring cursor does not match this decoder — in which
   *         case @p s is untouched.
   */
  int viterbi_set_state (viterbi_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CONV_CORE_H */
