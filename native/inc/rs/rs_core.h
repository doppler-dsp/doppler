/**
 * @file rs_core.h
 * @brief Reed-Solomon codes: the code description, the encoder, the syndromes
 * and the decoder that corrects — all reading the same description.
 *
 * A Reed-Solomon code over `GF(2^J)` is five numbers — a symbol width, a
 * field polynomial, a parity count, a first root and a root stride. This file
 * holds that description once and derives everything from it, so an encoder,
 * a checker and a decoder cannot disagree about what the code is.
 *
 * ## Nothing here is CCSDS
 *
 * The CCSDS configuration lives in `ccsds_tm/ccsds_tm_rs.h` as
 * @c CCSDS_TM_RS, beside
 * the two things 131.0-B-3 adds that are *not* properties of the code: the
 * dual-basis symbol representation (4.3.9) and the interleaver (4.4.1). A
 * standard picking a code is not the same fact as the code existing. Point
 * this at RS(255,239) — the mother code DVB shortens — at RS(15,11) to check
 * something by hand, or at whatever a caller brings: the arithmetic is
 * identical and only the table changes.
 *
 * One thing the table does **not** change is `n`, which is `2^J - 1` by
 * construction. So a SHORTENED code is not expressible here: DVB's own
 * RS(204,188) and CCSDS 4.4.2's shortened codeblock are RS(255,239) and
 * RS(255,223) with leading zeros the sender never transmits, and that
 * virtual fill is
 * [gh-813](https://github.com/doppler-dsp/doppler/issues/813).
 *
 * ## Two fields that are validated rather than trusted
 *
 * Both of these produce arithmetic that is entirely self-consistent, so a
 * round trip against a matching encoder cannot see either:
 *
 * - **`field_poly` must be primitive.** If `a = x` returns to 1 before `n`
 *   steps the polynomial generates a subgroup rather than the field, and
 *   @ref rs_init refuses.
 * - **`gcd(root_stride, n)` must be 1**, or the `nroots` roots are not
 *   distinct and the code corrects fewer errors than its parity count claims.
 *   CCSDS 4.3.4 states this as a note about `a^11`; for a general
 *   implementation it is a condition to check.
 *
 * ## Conventions
 *
 * - **Symbols are packed, one per byte** — a Reed-Solomon symbol *is* a byte
 *   at `J = 8`, and at `J < 8` it is a byte with the top bits clear. This
 *   differs from the bit-oriented `conv` and `ccsds_tm` kernels, and the
 *   boundary
 *   between the two belongs to the frame assembler.
 * - **A codeword is `k` information symbols followed by `nroots` parity**,
 *   index 0 first on the wire, so index `i` carries `x^(n-1-i)`.
 * - **The conventional basis throughout.** A symbol representation is a
 *   transmission convention, not arithmetic; a caller whose standard uses
 *   another one transforms at its own boundary.
 *
 * @see docs/design/reed-solomon.md for the algebra, the two offsets a
 * textbook omits, and what a decode refusal does and does not mean.
 * @see ccsds_tm/ccsds_tm_rs.h for the CCSDS configuration.
 */
#ifndef RS_CORE_H
#define RS_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Largest symbol width; symbols are held one per byte. */
#define RS_SYMBOL_BITS_MAX 8

  /** @brief Largest number of parity symbols this can represent. */
#define RS_NROOTS_MAX 64

  /** @brief Largest codeword, `2^RS_SYMBOL_BITS_MAX - 1`. */
#define RS_N_MAX 255

  /**
   * @brief A Reed-Solomon code over `GF(2^J)`.
   *
   * @c first_root and @c root_stride together fix the generator's roots:
   * `g(x) = prod (x - a^(root_stride * j))` for `j` in
   * `[first_root, first_root + nroots)`. The textbook choice is a stride of
   * 1; CCSDS 4.3.4 uses 11, which is a legitimate choice precisely because
   * `a^11` is itself primitive, and a code no receiver expecting consecutive
   * powers of `a` can decode.
   */
  typedef struct
  {
    unsigned symbol_bits; /**< `J`, 2..RS_SYMBOL_BITS_MAX            */
    uint16_t field_poly;  /**< `F(x)` low `J` bits, `x^J` implicit   */
    unsigned nroots;      /**< parity symbols `2E`, 2..RS_NROOTS_MAX */
    unsigned first_root;  /**< `j0`: the first root is `a^(s*j0)`    */
    unsigned root_stride; /**< `s`, coprime with `n`                 */
  } rs_code_t;

  /**
   * @brief A code plus the tables derived from it.
   *
   * Transparent and allocation-free so a caller can put one on the stack or
   * in its own state, and so the derived sizes are readable without an
   * accessor. Build it with @ref rs_init and then treat it as read-only: it
   * carries no running state, and every function below takes it as `const`.
   */
  typedef struct
  {
    rs_code_t code;               /**< as given to @ref rs_init          */
    unsigned  n;                  /**< symbols per codeword, `2^J - 1`   */
    unsigned  k;                  /**< information symbols, `n - nroots` */
    unsigned  e;                  /**< correctable symbols, `nroots / 2` */
    uint8_t   exp[2 * RS_N_MAX];  /**< `a^i`, doubled to avoid a modulo  */
    uint8_t   log[RS_N_MAX + 1];  /**< `log_a`, index 0 unused           */
    uint8_t   gen[RS_NROOTS_MAX + 1]; /**< `g(x)`, `gen[i]` for `x^i`    */
  } rs_t;

  /**
   * @brief Is @p c a code this file can represent and decode?
   *
   * Checks the ranges, that `nroots` is even and leaves room for at least one
   * information symbol, and that `gcd(root_stride, n) == 1`. It does **not**
   * check that @c field_poly is primitive — that costs the table build, so
   * @ref rs_init reports it instead.
   *
   * @param c  The code.
   * @return   Non-zero if usable.
   */
  int rs_code_valid (const rs_code_t *c);

  /**
   * @brief Build the tables for @p c into @p rs.
   *
   * @param rs  Receives the code and its derived tables.
   * @param c   The code; see @ref rs_code_valid.
   * @return    Non-zero on success. Zero if @p c is not valid or if
   *            @c field_poly is not primitive, in which case @p rs is
   *            unusable and must not be passed to anything below.
   *
   * @code
   * rs_t rs;
   * const rs_code_t code = { .symbol_bits = 8, .field_poly = 0x1D,
   *                          .nroots = 16, .first_root = 1,
   *                          .root_stride = 1 };
   * if (!rs_init (&rs, &code))
   *   return 1;  // not a field, or not a code
   * @endcode
   */
  int rs_init (rs_t *rs, const rs_code_t *c);

  /**
   * @brief The `nroots + 1` coefficients of `g(x)`, `gen[i]` for `x^i`.
   *
   * Exposed because standards publish them — CCSDS 131.0-B-3 Annex G prints
   * all 33 for `E = 16` — so a caller, or a test, can check an implementation
   * against the standard rather than against itself.
   *
   * @param rs  An initialised code.
   * @return    Pointer into @p rs, valid as long as it is.
   */
  const uint8_t *rs_generator (const rs_t *rs);

  /**
   * @brief Encode: `k` information symbols in, `nroots` parity symbols out.
   *
   * Systematic — the information symbols are not touched. The parity is the
   * remainder of `info(x) * x^nroots` modulo `g(x)`, highest-order coefficient
   * first, which is the order it is transmitted in.
   *
   * @param rs      An initialised code.
   * @param info    @c rs->k information symbols, in transmission order.
   * @param parity  Receives @c rs->code.nroots parity symbols.
   */
  void rs_encode (const rs_t *rs, const uint8_t *info, uint8_t *parity);

  /**
   * @brief The `nroots` syndromes of @p codeword.
   *
   * `S_m = C(a^(s*(j0+m)))`, evaluated over the codeword as a polynomial with
   * index `i` carrying `x^(n-1-i)`. All zero is the DEFINING property of the
   * code — it needs no encoder and no decoder, which is what makes it usable
   * as a test oracle and as a receiver's error detector.
   *
   * @param rs        An initialised code.
   * @param codeword  @c rs->n symbols: information then parity.
   * @param syn       Receives @c rs->code.nroots syndromes.
   */
  void rs_syndromes (const rs_t *rs, const uint8_t *codeword, uint8_t *syn);

  /**
   * @brief Is this a valid codeword? — every syndrome zero.
   *
   * @param rs        An initialised code.
   * @param codeword  @c rs->n symbols.
   * @return          Non-zero when every syndrome is zero.
   */
  int rs_codeword_ok (const rs_t *rs, const uint8_t *codeword);

  /**
   * @brief Correct up to `E` symbol errors, in place.
   *
   * Berlekamp-Massey for the error locator, Chien for the positions and
   * Forney for the magnitudes — see `docs/design/reed-solomon.md` for the
   * derivation, and in particular for the two factors a textbook omits when
   * `first_root != 1` or `root_stride != 1`, both of which produce a decoder
   * that decodes its own encoder perfectly and interoperates with nothing.
   *
   * **It either refuses or returns a codeword.** When it corrects, the key
   * equation has zeroed every syndrome by construction, so the result passes
   * @ref rs_codeword_ok. There is no third outcome.
   *
   * A refusal is not the same claim as "more than `E` errors": beyond `E` a
   * bounded-distance decoder can land inside another codeword's sphere and
   * miscorrect — a property of the code, not of this implementation. The
   * protection is accounting at the frame level, which is why this reports a
   * count rather than a verdict.
   *
   * @param rs        An initialised code.
   * @param codeword  @c rs->n symbols, corrected in place on success and
   *                  left untouched on refusal.
   * @return          Symbols corrected, 0 for an already-valid codeword, or
   *                  -1 if the word could not be decoded.
   *
   * @code
   * const int fixed = rs_decode (&rs, word);
   * if (fixed < 0)
   *   ;  // too far from every codeword to name one
   * @endcode
   */
  int rs_decode (const rs_t *rs, uint8_t *codeword);

#ifdef __cplusplus
}
#endif

#endif /* RS_CORE_H */
