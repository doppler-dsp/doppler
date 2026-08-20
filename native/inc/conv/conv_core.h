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

#ifdef __cplusplus
}
#endif

#endif /* CONV_CORE_H */
