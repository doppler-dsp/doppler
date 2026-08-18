/**
 * @file ccsds_tm_rs.h
 * @brief CCSDS Reed-Solomon (255,223) — the outer code as a CONFIGURATION,
 * and the conventions that only a published value catches.
 *
 * CCSDS 131.0-B-3 section 4.3. `J = 8` bits per symbol, `E = 16` correctable
 * symbols, so `n = 255`, `2E = 32` parity symbols and `k = 223`. Systematic.
 *
 * **The algebra is not here.** `rs/rs_core.h` owns the field, the encoder,
 * the syndromes and the Berlekamp-Massey / Chien / Forney decoder, for any
 * Reed-Solomon code; this file holds @ref CCSDS_TM_RS — the five numbers
 * 131.0-B-3 picked — plus the two things the standard adds that are *not*
 * properties of the code: the **dual basis** symbols travel in (4.3.9) and
 * the **interleaver** (4.4.1). A standard choosing a code is a different
 * fact from the code existing, and keeping them apart is what stops the
 * conventions below from being written down twice.
 *
 * Three things here are NOT the textbook Reed-Solomon a reader will expect,
 * and each is invisible to an encode/decode round trip because a matched
 * decoder inverts whatever the encoder did:
 *
 * 1. **The field is not the usual one.** `F(x) = x^8 + x^7 + x^2 + x + 1`
 *    (4.3.3), where most implementations reach for `x^8 + x^4 + x^3 + x^2 + 1`
 *    out of habit.
 * 2. **The generator's roots are powers of `a^11`, not of `a`** —
 *    `g(x) = prod (x - a^(11j))` for `j = 128-E .. 127+E` (4.3.4). The
 *    standard notes `a^11` is itself primitive, which is what makes this a
 *    legitimate but unusual choice. Consecutive powers of `a` give a
 *    perfectly good (255,223) code that no CCSDS receiver can decode.
 * 3. **Symbols travel in the DUAL (Berlekamp) basis** — 4.3.9.1 says it
 *    *shall* be used. A conventional-basis codeword is self-consistent and
 *    matches no spacecraft.
 *
 * The oracle for the first two is Annex G, which prints every coefficient of
 * `g(x)`; for the third it is **the two matrices 4.3.9.3 prints**, both
 * transcribed into `test_ccsds_tm_rs.c` and checked row by row and across
 * all 256 values.
 *
 * That check is what settles the basis, and it is worth saying why the
 * obvious one does not. Requiring the two transforms to invert each other is
 * a CONSISTENCY test: any invertible matrix and its inverse pass it,
 * including the two equations read the wrong way round — the
 * transcription error a reader is most likely to make, and one that leaves
 * an exact inverse pair.
 *
 * A second, DERIVED check sits beside the published one and is not redundant
 * with it. Every GF(2)-linear functional on the field is `u -> Tr(c*u)` for
 * a unique `c`, so the eight output bits are eight field elements; the test
 * solves for them and asserts the structure a dual basis has — `c_0 = 1`,
 * `c_j = c_1^j`, and `Tr(c_i * beta_j) = delta_ij` read through the other
 * matrix. Measured, `c_1 = a^117`, which is not primitive
 * (`gcd(117, 255) = 3`) and does not need to be. The transcription says
 * these are CCSDS's matrices; the derivation says they are a dual basis at
 * all, and would still catch a pair that was transcribed consistently wrong
 * in both this file and the test.
 *
 * Bit convention follows the rest of `ccsds_tm/`: **packed symbols**, one
 * byte per
 * R-S symbol, because a Reed-Solomon symbol IS a byte. That differs from the
 * randomiser and the convolutional coder, which take unpacked bits — the
 * boundary between the two is real and belongs to the frame assembler, not
 * hidden inside a kernel.
 *
 * @see ccsds_tm.h for the randomiser, the ASM and the inner code.
 * @see rs/rs_core.h for the code family this configures.
 * @see docs/design/reed-solomon.md for the decoder's algebra.
 */
#ifndef CCSDS_TM_RS_H
#define CCSDS_TM_RS_H

#include "rs/rs_core.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Symbols per codeword, `n = 2^J - 1` (4.3.2b). */
#define CCSDS_TM_RS_N 255
/** @brief Information symbols per codeword when `E = 16` (4.3.2d). */
#define CCSDS_TM_RS_K 223
/** @brief Correctable symbols per codeword (4.3.2d). */
#define CCSDS_TM_RS_E 16
/** @brief Parity symbols per codeword, `2E` (4.3.2c). */
#define CCSDS_TM_RS_2E 32
/** @brief Largest interleaving depth 4.3.5.1 allows. */
#define CCSDS_TM_RS_MAX_DEPTH 8

  /**
   * @brief The five numbers 131.0-B-3 section 4.3 picks.
   *
   * The field polynomial (4.3.3), the parity count (4.3.2c), and the roots'
   * first index and stride (4.3.4). Everything the code *does* comes from
   * `rs/rs_core.h` reading this; nothing in that file knows what CCSDS is.
   *
   * `test_ccsds_tm_rs.c` holds it to Annex G, which publishes every
   * coefficient
   * of the `g(x)` these five numbers produce — a value this repository
   * cannot choose, and the only kind of check a code with a matched decoder
   * cannot pass by agreeing with itself.
   */
  extern const rs_code_t CCSDS_TM_RS;

  /**
   * @brief Convert one symbol from the conventional basis to the dual basis.
   *
   * 4.3.9.3, first equation: `[z0..z7] = [u7..u0] T`. The returned byte holds
   * `z0` in its most significant bit, because 4.3.9.2 fixes `z0` as the first
   * bit transmitted and this codebase writes MSB-first.
   */
  uint8_t ccsds_tm_rs_conv_to_dual (uint8_t u);

  /**
   * @brief Convert one symbol from the dual basis back to conventional.
   *
   * 4.3.9.3, second equation, and the test holds it to that matrix as
   * printed. Exact inverse of @ref ccsds_tm_rs_conv_to_dual across all 256
   * values — but note that inversion ALONE catches only a single
   * mis-transcribed bit, never a wrong pair consistent with itself. The two
   * checks that separate those are described at the top of this file.
   */
  uint8_t ccsds_tm_rs_dual_to_conv (uint8_t z);

  /**
   * @brief The 33 coefficients of `g(x)`, in conventional representation.
   *
   * Exposed because Annex G publishes them, so a caller — or a test — can
   * check this implementation against the standard rather than against
   * itself. `g[i]` is the coefficient of `x^i`; the sequence is palindromic.
   *
   * @return Pointer to `CCSDS_TM_RS_2E + 1` bytes, valid for the process
   *         lifetime.
   */
  const uint8_t *ccsds_tm_rs_generator (void);

  /**
   * @brief Is this a valid codeword? — all 32 syndromes zero.
   *
   * The DEFINING property of the code: a codeword polynomial evaluates to
   * zero at every root of `g(x)`. Checking it needs no decoder and is not a
   * round trip against the encoder's own logic, which is what makes it usable
   * as a test oracle and, later, as a receiver's error detector.
   *
   * @param codeword  255 symbols in the dual basis: 223 information
   *                  followed by 32 parity, exactly as transmitted.
   * @return Non-zero when every syndrome is zero.
   */
  int ccsds_tm_rs_codeword_ok (const uint8_t *codeword);

  /**
   * @brief Correct up to `E = 16` symbol errors in one codeword, in place.
   *
   * The decode is `rs_decode`'s; this transforms the codeword out of the
   * dual basis on the way in and back on the way out (4.3.9, figure F-1).
   * Correcting in the transmitted basis instead would produce a decoder that
   * repairs its own encoder's output perfectly and interoperates with
   * nothing — the same failure the field polynomial and the root stride each
   * offer, and the reason this transform is not optional.
   *
   * It either refuses or returns a codeword; see `rs_decode` for what a
   * refusal does and does not mean.
   *
   * @param codeword  255 symbols in the dual basis, corrected in place on
   *                  success and left untouched on refusal.
   * @return          Symbols corrected, 0 if the codeword was already valid,
   *                  or -1 if it could not be decoded.
   */
  int ccsds_tm_rs_decode (uint8_t *codeword);

  /**
   * @brief What @ref ccsds_tm_rs_decode_block found in one codeblock.
   *
   * `codewords - uncorrectable` is how many are good afterwards, and
   * @ref symbols is the repair work the outer code actually did — the
   * quantity that says whether the inner code is delivering what the outer
   * one was sized for.
   */
  typedef struct
  {
    unsigned codewords;     /**< Codewords in the block, i.e. the depth   */
    unsigned corrected;     /**< How many needed and received repair      */
    unsigned uncorrectable; /**< How many the decoder refused             */
    unsigned symbols;       /**< Symbol errors repaired across the block  */
  } ccsds_tm_rs_block_rx_t;

  /**
   * @brief Decode an interleaved codeblock in place (4.3.5, 4.4.1).
   *
   * The mirror of @ref ccsds_tm_rs_encode_block, over the same S1/S2
   * rotation —
   * written once, here, so the two directions cannot come to disagree about
   * which symbol belongs to which codeword. A rotated de-interleave is
   * invisible against an all-zero payload, whose codewords are identical, so
   * the test that pins this uses structured data.
   *
   * @param block  `CCSDS_TM_RS_N * depth` symbols, dual basis, corrected in
   *               place: both the information and the check sections of any
   *               codeword that was repaired.
   * @param depth  Interleaving depth; 4.3.5.1 allows 1, 2, 3, 4, 5 and 8.
   * @param rx     Receives the per-codeword outcomes; may be `NULL`.
   * @return       The number of information symbols,
   *               `CCSDS_TM_RS_K * depth`, or
   *               0 if @p depth is not allowed.
   */
  size_t ccsds_tm_rs_decode_block (uint8_t *block, unsigned depth,
                              ccsds_tm_rs_block_rx_t *rx);

  /**
   * @brief Encode an interleaved codeblock (4.3.5, 4.4.1).
   *
   * Depth @p depth means @p depth codewords are encoded in parallel, with
   * switch S1 handing successive input symbols to successive encoders. Two
   * consequences worth stating because they are what the tests assert:
   *
   * - the information section comes out **unchanged** — 4.4.1 has S2
   *   reassembling the information "in the same way as they entered", so only
   *   the check symbols are rearranged;
   * - `depth == 1` is the un-interleaved code, which 4.3.5.1 notes outright.
   *
   * Interleaving is what makes the outer code burst-tolerant: a contiguous
   * burst of `B` symbols lands as `ceil(B / depth)` errors in each codeword,
   * so depth trades no rate at all for a `depth`-fold longer correctable
   * burst.
   *
   * @param info   `CCSDS_TM_RS_K * depth` information symbols, dual basis.
   * @param depth  Interleaving depth; 4.3.5.1 allows 1, 2, 3, 4, 5 and 8.
   * @param out    Receives `CCSDS_TM_RS_N * depth` symbols: the information
   *               verbatim, then `CCSDS_TM_RS_2E * depth` interleaved check
   *               symbols.
   * @return The number of symbols written, or 0 if @p depth is not allowed.
   */
  size_t ccsds_tm_rs_encode_block (const uint8_t *info, unsigned depth,
                              uint8_t *out);

  /**
   * @brief Encode one codeword: 223 information symbols in, 32 parity out.
   *
   * Both @p info and @p parity are in the **dual basis**, i.e. exactly what
   * goes on the wire (4.3.9). The conventional-basis arithmetic and the
   * pre/post transformation of figure F-1 happen inside.
   *
   * @param info    223 information symbols, in transmission order.
   * @param parity  Receives 32 parity symbols, following the information.
   */
  void ccsds_tm_rs_encode (const uint8_t *info, uint8_t *parity);

#ifdef __cplusplus
}
#endif

#endif /* CCSDS_TM_RS_H */
