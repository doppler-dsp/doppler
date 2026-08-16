/**
 * @file fec_rs.h
 * @brief CCSDS Reed-Solomon (255,223) — the outer code, and the one with the
 * conventions that only a published value catches.
 *
 * CCSDS 131.0-B-3 section 4.3. `J = 8` bits per symbol, `E = 16` correctable
 * symbols, so `n = 255`, `2E = 32` parity symbols and `k = 223`. Systematic.
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
 * `g(x)`; for the third it is the pair of matrices in 4.3.9.3, whose
 * transcription is checked by requiring the two transforms to invert each
 * other across all 256 symbols.
 *
 * Bit convention follows the rest of `fec/`: **packed symbols**, one byte per
 * R-S symbol, because a Reed-Solomon symbol IS a byte. That differs from the
 * randomiser and the convolutional coder, which take unpacked bits — the
 * boundary between the two is real and belongs to the frame assembler, not
 * hidden inside a kernel.
 *
 * @see fec_ccsds.h for the randomiser, the ASM and the inner code.
 */
#ifndef FEC_RS_H
#define FEC_RS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Symbols per codeword, `n = 2^J - 1` (4.3.2b). */
#define FEC_RS_N 255
/** @brief Information symbols per codeword when `E = 16` (4.3.2d). */
#define FEC_RS_K 223
/** @brief Parity symbols per codeword, `2E` (4.3.2c). */
#define FEC_RS_2E 32

  /**
   * @brief Convert one symbol from the conventional basis to the dual basis.
   *
   * 4.3.9.3, first equation: `[z0..z7] = [u7..u0] T`. The returned byte holds
   * `z0` in its most significant bit, because 4.3.9.2 fixes `z0` as the first
   * bit transmitted and this codebase writes MSB-first.
   */
  uint8_t fec_rs_conv_to_dual (uint8_t u);

  /**
   * @brief Convert one symbol from the dual basis back to conventional.
   *
   * 4.3.9.3, second equation. Exact inverse of @ref fec_rs_conv_to_dual, and
   * the test asserts that across all 256 values — which is what catches a
   * single mis-transcribed bit in either matrix.
   */
  uint8_t fec_rs_dual_to_conv (uint8_t z);

  /**
   * @brief The 33 coefficients of `g(x)`, in conventional representation.
   *
   * Exposed because Annex G publishes them, so a caller — or a test — can
   * check this implementation against the standard rather than against
   * itself. `g[i]` is the coefficient of `x^i`; the sequence is palindromic.
   *
   * @return Pointer to `FEC_RS_2E + 1` bytes, valid for the process lifetime.
   */
  const uint8_t *fec_rs_generator (void);

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
  void fec_rs_encode (const uint8_t *info, uint8_t *parity);

#ifdef __cplusplus
}
#endif

#endif /* FEC_RS_H */
