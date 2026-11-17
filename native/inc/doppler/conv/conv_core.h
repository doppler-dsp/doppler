/**
 * @file conv_core.h
 * @brief Convolutional codes: the code description, the encoder, and the
 * maximum-likelihood decoder that reads the same description.
 *
 * A rate-1/n convolutional code is four numbers — a constraint length, an
 * output count, a generator polynomial per output, and which outputs are
 * inverted. This file holds that description once and derives everything from
 * it, so an encoder and a decoder cannot disagree about what the code is.
 * A **puncturing pattern** is the fifth part of the description: it deletes
 * symbols from the mother code's output to raise the rate, and the decoder
 * puts them back as erasures. It lives here for the same reason the
 * polynomials do, so the two directions read one pattern.
 *
 * ## The one expression
 *
 * @ref dp_conv_outputs is what the family of codes emits, and it is the only
 * place that says so. @ref dp_conv_encode calls it to produce symbols; a Viterbi
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
 * The CCSDS configuration lives in `ccsds_tm/ccsds_tm.h` as @c dp_CCSDS_TM_CONV,
 * because a channel-coding standard picking a code is not the same fact as the
 * code existing. Point this at the deep-space rate-1/6 code, at a K = 9
 * experiment, or at whatever a caller brings — the trellis is identical and
 * only the table changes.
 *
 * ## Conventions
 *
 * - **Bits are unpacked**, one per byte in the LSB, matching
 *   `dp_wfm_frame_assemble`,
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

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"

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
   *
   * ## Puncturing
   *
   * A pattern repeats every @c punct_period input bits. Bit `i * n + j` of
   * @c punct_mask says whether output @c j of the @c i-th input bit of the
   * period is **transmitted** (1) or deleted (0), so the mask reads in the
   * order the encoder emits symbols. Both fields zero is no puncturing, which
   * is what a zero-initialized code means, so a code written before the
   * fields existed is unchanged. `punct_period * n` must not exceed 32, and
   * every input bit must keep at least one symbol: an input that transmits
   * nothing would leave a decoder unable to say how many input bits a
   * received stream holds.
   *
   * The pattern is part of the code, not of a caller's framing. A decoder
   * given a different pattern than the encoder used decodes a stream it was
   * never sent, and nothing in the symbols says so.
   */
  typedef struct
  {
    unsigned k;                  /**< constraint length, 2..CONV_K_MAX   */
    unsigned n;                  /**< outputs per input, 1..CONV_N_MAX   */
    uint32_t poly[CONV_N_MAX];   /**< generator polynomials, k bits each */
    uint32_t invert;             /**< bit j: output j is inverted        */
    unsigned punct_period;       /**< input bits per pattern; 0 = none   */
    uint32_t punct_mask;         /**< bit i*n+j: transmit output j of
                                      input i of the period            */
  } conv_code_t;

  /**
   * @brief Is @p c a code this file can represent?
   *
   * @param c  The code.
   * @return   Non-zero if usable: `k` in `[2, CONV_K_MAX]`, `n` in
   *           `[1, CONV_N_MAX]`, and every polynomial within `k` bits and
   *           non-zero. A zero polynomial is an output that carries no
   *           information, which is a typo rather than a code. A puncturing
   *           pattern must be both-zero (none) or satisfy the rules on
   *           @ref conv_code_t.
   */
  int dp_conv_code_valid (const conv_code_t *c);

  /**
   * @brief Number of trellis states, `2^(k-1)`.
   *
   * @param c  The code; `k` must already be valid (see
   *           dp_conv_code_valid()), it is not checked here.
   * @return   `1 << (k-1)`.
   */
  JM_FORCEINLINE uint32_t
  conv_states (const conv_code_t *c)
  {
    return 1u << (c->k - 1u);
  }

  /**
   * @brief The output word for one branch — **the** expression of the code.
   *
   * Output @c j is bit @c j of the result, matching the order the
   * polynomials are given in and the order @ref dp_conv_encode emits them — so
   * for CCSDS, bit 0 is C1 and bit 1 is C2.
   *
   * @param c      The code.
   * @param state  Trellis state: the `k-1` previous input bits.
   * @param bit    The new input bit (0 or 1).
   * @return       `n` bits, output @c j in bit @c j, inversion applied.
   */
  unsigned dp_conv_outputs (const conv_code_t *c, uint32_t state, unsigned bit);

  /**
   * @brief The state reached from @p state on @p bit.
   *
   * Shifts @p bit in at the high stage (bit `k-2` of the result) and drops
   * the oldest input from bit 0.
   *
   * @param c      The code (only `k` is read).
   * @param state  Current trellis state: the `k-1` previous input bits.
   * @param bit    The new input bit; only bit 0 is used.
   * @return       The next state, in `[0, 2^(k-1))`.
   */
  JM_FORCEINLINE uint32_t
  conv_next_state (const conv_code_t *c, uint32_t state, unsigned bit)
  {
    return (((bit & 1u) << (c->k - 1u)) | state) >> 1;
  }

  /* ── puncturing ──────────────────────────────────────────────────────── */

  /**
   * @brief Does the code delete any of the mother code's symbols?
   *
   * @param c  A valid code.
   * @return   Non-zero if @c punct_period is set and the mask leaves at least
   *           one symbol out. An all-ones mask is a pattern that punctures
   *           nothing, and is treated as none.
   */
  int dp_conv_punctured (const conv_code_t *c);

  /**
   * @brief Symbols transmitted for the input bit at pattern position
   *        @p phase.
   *
   * @param c      A valid code.
   * @param phase  Position within the period, `[0, punct_period)`; ignored
   *               for an unpunctured code.
   * @return       `n` unpunctured, otherwise the mask bits set for that
   *               input.
   */
  unsigned dp_conv_kept (const conv_code_t *c, unsigned phase);

  /**
   * @brief Symbols transmitted for @p n_in input bits starting at @p phase.
   *
   * The exact count the encoder writes, so a caller sizes its buffer with
   * this and not with `n_in * n`.
   *
   * @param c      A valid code.
   * @param phase  Pattern position of the first input bit.
   * @param n_in   Input bits.
   * @return       Symbols transmitted.
   */
  size_t dp_conv_coded_len (const conv_code_t *c, unsigned phase, size_t n_in);

  /**
   * @brief How many input bits @p n_syms received symbols hold, from
   *        @p phase.
   *
   * The inverse of @ref dp_conv_coded_len. Every input bit keeps at least one
   * symbol, so the answer is unique when it exists.
   *
   * @param c        A valid code.
   * @param phase    Pattern position of the first input bit.
   * @param n_syms   Received symbols.
   * @param[out] steps  Input bits those symbols cover.
   * @return  Non-zero if @p n_syms is exactly a whole number of input bits'
   *          symbols, zero if it ends part way through one.
   */
  int dp_conv_steps_for (const conv_code_t *c, unsigned phase, size_t n_syms,
                         size_t *steps);

  /**
   * @brief Pattern position after @p n_in more input bits.
   *
   * @param c      A valid code.
   * @param phase  Position now.
   * @param n_in   Input bits consumed.
   * @return       `(phase + n_in) mod punct_period`, or 0 unpunctured.
   */
  unsigned dp_conv_phase_after (const conv_code_t *c, unsigned phase,
                                size_t n_in);

  /* ── the encoder ─────────────────────────────────────────────────────── */

  /**
   * @brief Encoder state: the shift register, and nothing else.
   *
   * Held in a struct rather than passed by value because the encoder is
   * **continuous** — a caller encoding a long record in chunks must carry the
   * register across calls or introduce a discontinuity at every chunk
   * boundary that no decoder expects. The puncturing phase is carried for the
   * same reason: a chunk that ends mid-pattern must not restart it.
   */
  typedef struct
  {
    uint32_t reg;   /**< the k-1 previous inputs; newest in the high stage */
    uint32_t phase; /**< position in the puncturing pattern; 0 unpunctured  */
  } conv_enc_t;

  /**
   * @brief Reset the encoder to the all-zero state.
   *
   * @param[out] s  Encoder state to clear.  Must be non-NULL.
   */
  void dp_conv_enc_init (conv_enc_t *s);

  /**
   * @brief Encode @p n_in bits, emitting `n_in * c->n` symbols, fewer when
   *        the code is punctured.
   *
   * A punctured code emits @ref dp_conv_coded_len symbols: each input bit's
   * outputs in polynomial order, less the ones the pattern deletes.
   *
   * @param s       Encoder state, carried across calls.
   * @param c       The code.
   * @param in      @p n_in unpacked input bits.
   * @param n_in    Number of input bits.
   * @param out     Receives the symbols, one per byte, outputs in
   *                polynomial order per input bit.
   * @param max_out Capacity of @p out.
   * @return        Symbols written, or 0 if the code is invalid or @p max_out
   *                is smaller than @ref dp_conv_coded_len — in which case
   *                @p out is untouched.
   */
  size_t dp_conv_encode (conv_enc_t *s, const conv_code_t *c, const uint8_t *in,
                      size_t n_in, uint8_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* CONV_CORE_H */
