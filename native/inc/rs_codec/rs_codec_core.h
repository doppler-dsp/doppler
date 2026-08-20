/**
 * @file rs_codec_core.h
 * @brief The Reed-Solomon codec, as an object over `rs`.
 *
 * `rs` owns the CODE — the field, the derived tables, the systematic
 * encoder, the syndromes and the Berlekamp-Massey / Chien / Forney decoder,
 * for any RS code over `GF(2^J)`. This owns the OBJECT built over one: the
 * five numbers that name a code, bound to the tables derived from them, so a
 * caller cannot pair the wrong two.
 *
 * **It is not a second implementation.** Every function here calls the
 * matching `rs_*` kernel. Two Reed-Solomon implementations for one code
 * family is how a root offset or a basis convention comes to differ between
 * them — and both of those are invisible to a round trip, because a matched
 * decoder inverts whatever the encoder did.
 *
 * ## Why this exists at all
 *
 * `rs_encode`, `rs_syndromes` and `rs_codeword_ok` were reachable from
 * Python only through `wfm_frame_desc_t`'s Reed-Solomon stage, which binds
 * to `ccsds_tm_frame_ops` and carries an interleaving depth rather than a
 * code. So Python could run exactly ONE Reed-Solomon code — CCSDS's — and
 * only inside a frame (doppler#900).
 *
 * ## Matching the algebra is not matching the wire
 *
 * The five numbers here are the CODE. A standard adds conventions that are
 * not properties of it, and CCSDS adds two: symbols travel in the **dual
 * (Berlekamp) basis** (131.0-B 4.3.9) and codewords are **interleaved**
 * (4.4.1). `ccsds_tm/ccsds_tm_rs.h` holds both. Construct this with CCSDS's
 * five numbers and the arithmetic is right and the wire format is not; a
 * conventional-basis codeword is self-consistent and matches no spacecraft.
 *
 * ## Conventions, inherited from `rs`
 *
 * - **Symbols are packed, one per byte** — an RS symbol *is* a byte at
 *   `J = 8`. This differs from `conv_enc` and the randomiser, which take
 *   unpacked bits; the boundary belongs to the frame assembler.
 * - **A codeword is `k` information symbols then `nroots` parity**, index 0
 *   first on the wire.
 * - `n` is `2^J - 1` by construction, so a SHORTENED code is not expressible
 *   — DVB's RS(204,188) and CCSDS 4.4.2's shortened codeblock are the full
 *   codes with leading zeros the sender never transmits, and that virtual
 *   fill is
 *   [gh-813](https://github.com/doppler-dsp/doppler/issues/813).
 *
 * Lifecycle: `create -> [encode / decode / syndromes / codeword_ok]* ->
 * destroy`.
 *
 * @see rs/rs_core.h for the code description and every kernel.
 * @see ccsds_tm/ccsds_tm_rs.h for CCSDS's configuration and its two
 *      conventions.
 * @see docs/design/reed-solomon.md for the algebra.
 */
#ifndef RS_CODEC_CORE_H
#define RS_CODEC_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "rs/rs_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A code and the tables derived from it.
 *
 * One `rs_t` and nothing else. The derived sizes — `n`, `k`, `e` — are read
 * back through the accessors below rather than mirrored into fields here,
 * because two copies of a derived number is how they come to disagree.
 *
 * Allocate with rs_codec_create().
 */
typedef struct
{
  rs_t rs;
  /*<<property_struct_fields>>*/
} rs_codec_state_t;

/**
 * @brief Create a codec for the code named by the five arguments.
 *
 * Two of them are VALIDATED rather than trusted, because both produce
 * arithmetic that is entirely self-consistent — a round trip against a
 * matching encoder cannot see either: @p field_poly must be primitive, and
 * @p root_stride must be coprime with `n`, or the `nroots` roots are not
 * distinct and the code corrects fewer errors than its parity count claims.
 *
 * @param nroots       Parity symbols `2E`; even, >= 2, leaving `k >= 1`.
 * @param symbol_bits  `J`, 2..8.
 * @param field_poly   `F(x)`, low `J` bits, `x^J` implicit; PRIMITIVE.
 * @param first_root   `j0`: the first root is `a^(root_stride * j0)`.
 * @param root_stride  `s`; coprime with `n`.
 * @return Heap-allocated state, or NULL if the five do not name a usable
 *         code.
 * @note Caller must call rs_codec_destroy() when done.
 *
 * @code
 * >>> from doppler.coding import ReedSolomon
 * >>> rs = ReedSolomon(nroots=32)      # RS(255,223) over the usual GF(256)
 * >>> rs.n, rs.k, rs.e
 * (255, 223, 16)
 * >>> ReedSolomon(nroots=4, symbol_bits=4, field_poly=0b0011).n
 * 15
 * @endcode
 */
rs_codec_state_t *rs_codec_create (uint32_t nroots, uint32_t symbol_bits,
                                   uint32_t field_poly, uint32_t first_root,
                                   uint32_t root_stride);

/**
 * @brief Destroy a codec and release all memory.
 * @param state  May be NULL.
 */
void rs_codec_destroy (rs_codec_state_t *state);

/**
 * @brief Symbols @ref rs_codec_encode writes for @p n_in information
 * symbols: a whole codeword, `n`.
 */
size_t rs_codec_encode_max_out (rs_codec_state_t *state, size_t n_in);

/**
 * @brief Encode `k` information symbols into a whole `n`-symbol codeword.
 *
 * Systematic: the information symbols are copied through untouched and the
 * `nroots` parity symbols follow them, which is the order they are
 * transmitted in. `rs_encode` computes the parity; this places it.
 *
 * The WHOLE codeword rather than the parity alone, because that is the unit
 * every other method here takes — @ref rs_codec_decode,
 * @ref rs_codec_syndromes and @ref rs_codec_codeword_ok all read `n`
 * symbols, and a caller who wants the parity by itself can take the last
 * `nroots` of the answer. (`rs_encode` is the other split, and is still
 * there for a frame assembler that has already placed the information.)
 *
 * @p out may alias @p in — `rs_codec_encode (rs, buf, k, buf, n)` appends
 * the parity to a buffer that already holds the information, which is the
 * call a frame assembler makes and the one `rs_encode` exists for.
 *
 * @param state   The codec.
 * @param in      Exactly `k` information symbols.
 * @param n_in    Number of symbols in @p in.
 * @param out     Receives `n` symbols; may be @p in.
 * @param max_out Capacity of @p out.
 * @return `n` on success, or 0 if @p n_in is not exactly `k` or @p out is
 *         too small — refusing rather than truncating, since a short
 *         codeword is not a codeword.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.coding import ReedSolomon
 * >>> rs = ReedSolomon(nroots=32)
 * >>> info = np.arange(rs.k, dtype=np.uint8)
 * >>> word = rs.encode(info)
 * >>> word.size, bool(np.array_equal(word[: rs.k], info))
 * (255, True)
 * >>> rs.codeword_ok(word)
 * 1
 * @endcode
 */
size_t rs_codec_encode (rs_codec_state_t *state, const uint8_t *in,
                        size_t n_in, uint8_t *out, size_t max_out);

/**
 * @brief Correct up to `E` symbol errors, IN PLACE.
 *
 * `rs_decode`, over the caller's own buffer: the corrected symbols land in
 * @p codeword itself, which is why the binding demands a writable array
 * rather than quietly working on a copy the caller would then discard.
 *
 * **It either refuses or leaves a codeword.** On success the key equation
 * has zeroed every syndrome by construction, so the result passes
 * @ref rs_codec_codeword_ok. On refusal @p codeword is untouched.
 *
 * A refusal is not the same claim as "more than `E` errors". Beyond `E` a
 * bounded-distance decoder can land inside another codeword's sphere and
 * miscorrect — a property of the code, not of this implementation — which is
 * why this reports a COUNT rather than a verdict, and why frame-level
 * accounting is the protection.
 *
 * @param state         The codec.
 * @param codeword      `n` symbols, corrected in place.
 * @param codeword_len  Number of symbols in @p codeword.
 * @return Symbols corrected, 0 for an already-valid codeword, **-1** when
 *         the word is too far from every codeword to name one, or **-2**
 *         when @p codeword_len is not `n`. Two negative codes rather than
 *         one because they are different kinds of fact: -1 is the channel's
 *         answer and -2 is the caller's mistake.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.coding import ReedSolomon
 * >>> rs = ReedSolomon(nroots=32)
 * >>> word = rs.encode(np.arange(rs.k, dtype=np.uint8))
 * >>> word[3] ^= 0xFF          # one symbol, however many bits it moved
 * >>> word[40] ^= 0x01
 * >>> rs.decode(word)          # corrected in place
 * 2
 * >>> bool(np.array_equal(word[: rs.k], np.arange(rs.k, dtype=np.uint8)))
 * True
 * @endcode
 */
int rs_codec_decode (rs_codec_state_t *state, uint8_t *codeword,
                     size_t codeword_len);

/**
 * @brief Syndromes @ref rs_codec_syndromes writes: `nroots`.
 */
size_t rs_codec_syndromes_max_out (rs_codec_state_t *state, size_t n_in);

/**
 * @brief The `nroots` syndromes of an `n`-symbol word.
 *
 * All zero is the DEFINING property of the code: it needs no encoder and no
 * decoder to check, which is what makes it usable both as a test oracle and
 * as a receiver's error detector. @ref rs_codec_codeword_ok is this reduced
 * to the one bit most callers want.
 *
 * @param state   The codec.
 * @param in      `n` symbols.
 * @param n_in    Number of symbols in @p in.
 * @param out     Receives `nroots` syndromes.
 * @param max_out Capacity of @p out.
 * @return `nroots`, or 0 if @p n_in is not `n` or @p out is too small.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.coding import ReedSolomon
 * >>> rs = ReedSolomon(nroots=32)
 * >>> word = rs.encode(np.zeros(rs.k, dtype=np.uint8))
 * >>> bool(rs.syndromes(word).any())      # a codeword has none
 * False
 * >>> word[7] ^= 0x20
 * >>> bool(rs.syndromes(word).any())
 * True
 * @endcode
 */
size_t rs_codec_syndromes (rs_codec_state_t *state, const uint8_t *in,
                           size_t n_in, uint8_t *out, size_t max_out);

/**
 * @brief Is this a valid codeword? — every syndrome zero.
 *
 * @param state         The codec.
 * @param codeword      `n` symbols.
 * @param codeword_len  Number of symbols in @p codeword.
 * @return 1 when every syndrome is zero, 0 otherwise — including when
 *         @p codeword_len is not `n`, since a word of the wrong length is
 *         not a codeword of this code.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.coding import ReedSolomon
 * >>> rs = ReedSolomon(nroots=32)
 * >>> rs.codeword_ok(np.zeros(rs.n, np.uint8))   # all-zero IS a codeword
 * 1
 * >>> rs.codeword_ok(np.zeros(rs.n - 1, np.uint8))   # at the right size
 * 0
 * @endcode
 */
int rs_codec_codeword_ok (rs_codec_state_t *state, const uint8_t *codeword,
                          size_t codeword_len);

/**
 * @brief The `nroots + 1` coefficients of `g(x)`, `out[i]` for `x^i`.
 *
 * Exposed because standards PUBLISH them — CCSDS 131.0-B Annex G prints all
 * 33 for `E = 16` — so a caller who has just configured a code from a
 * document can check that they read the five numbers correctly, against the
 * document rather than against this implementation.
 *
 * The caller supplies the buffer rather than being handed one, because the
 * length is a property of the CODE and not of the call: `g(x)` has exactly
 * `nroots + 1` coefficients and there is no other number a caller could ask
 * for. A self-sizing method would carry a `count` parameter that means
 * nothing, which is a worse trade than one line of allocation.
 *
 * @param state  The codec.
 * @param out    Receives `nroots + 1` coefficients; `out[i]` is the
 *               coefficient of `x^i`, so `out[nroots]` is 1.
 * @param out_len  Length of @p out; fewer than `nroots + 1` writes nothing.
 * @return `nroots + 1`, or 0 if @p out is too small.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.coding import ReedSolomon
 * >>> rs = ReedSolomon(nroots=32, field_poly=0x87, first_root=112,
 * ...                  root_stride=11)          # CCSDS 131.0-B 4.3
 * >>> g = np.empty(rs.nroots + 1, np.uint8)
 * >>> rs.generator(g)                  # Annex G prints all 33
 * 33
 * >>> int(g[0]), int(g[-1])
 * (1, 1)
 * @endcode
 */
size_t rs_codec_generator (rs_codec_state_t *state, uint8_t *out,
                           size_t out_len);

/** @brief Symbols per codeword, `2^J - 1`. */
size_t rs_codec_get_n (const rs_codec_state_t *state);

/** @brief Information symbols per codeword, `n - nroots`. */
size_t rs_codec_get_k (const rs_codec_state_t *state);

/** @brief Correctable symbols per codeword, `nroots / 2`. */
size_t rs_codec_get_e (const rs_codec_state_t *state);

/** @brief Parity symbols per codeword, `2E`. */
size_t rs_codec_get_nroots (const rs_codec_state_t *state);

/** @brief Symbol width `J`, in bits. */
size_t rs_codec_get_symbol_bits (const rs_codec_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RS_CODEC_CORE_H */
