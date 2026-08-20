/**
 * @file conv_enc_core.h
 * @brief The convolutional encoder, as a stateful object over `conv`.
 *
 * `conv` owns the CODE — the description, the trellis arithmetic, and the
 * `conv_encode` kernel that turns bits into symbols. This owns the ENCODER
 * built over one: a code and the shift register that must survive between
 * calls, bound together so a caller cannot pair the wrong two.
 *
 * **It is not a second implementation.** @ref conv_enc_encode calls
 * `conv_encode`, exactly as @ref viterbi_decode's object calls its own
 * kernel. Two encoders for one code family is how a rounding rule or an
 * inversion comes to differ between them.
 *
 * ## Why this exists at all
 *
 * `Viterbi` accepts any rate-1/n code, and until this the library could
 * produce symbols for exactly one of them — CCSDS's, and only inside a
 * `wfm_frame_desc_t`, whose stage kinds bind to `ccsds_tm_frame_ops` and
 * carry a depth rather than a polynomial. Nothing in doppler exposed an
 * `encode()` at all (doppler#900). A decoder whose matching encoder cannot be
 * reached is a decoder that can only be tested against itself, which is the
 * failure `conv`'s own tests are built to refuse.
 *
 * ## The register is the whole state, and it is load-bearing
 *
 * 3.3.2's shape, in the general case: the output is one uninterrupted symbol
 * sequence, so encoding a long record in chunks must carry the `k-1` previous
 * inputs across every boundary. An encoder that restarted per chunk emits
 * `k-1` wrong symbols at each one — self-consistent, decodable by a receiver
 * of one's own construction, and not what any standard says. That is why the
 * register lives here rather than being passed in, and why this object
 * serializes.
 *
 * Bit convention follows `conv` and the rest of the coding chain: **unpacked**
 * bits, one per byte in the LSB, in and out.
 *
 * @see conv/conv_core.h for the code description and the kernel.
 * @see viterbi/viterbi_core.h for the other direction.
 *
 * @code
 * const uint32_t poly[2] = { 0171u, 0133u };
 * conv_enc_state_t *e = conv_enc_create (poly, 2, 7u, 0x2u);  // CCSDS
 * uint8_t sym[2 * N];
 * const size_t n = conv_enc_encode (e, bits, N, sym, sizeof sym);
 * conv_enc_destroy (e);
 * @endcode
 */
#ifndef CONV_ENC_CORE_H
#define CONV_ENC_CORE_H

#include "clib_common.h"
#include "conv/conv_core.h"
#include "dp_state.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief A code and the register encoding it, together.
   *
   * Pointer-free and small — the code is copied, so the caller's may be
   * temporary and the encoder cannot be invalidated by something it does not
   * own.
   */
  typedef struct
  {
    conv_code_t code; /**< the code; copied at create           */
    conv_enc_t  enc;  /**< the k-1 previous inputs, newest high */
    /*<<property_struct_fields>>*/
  } conv_enc_state_t;

  /**
   * @brief Build an encoder for the code the polynomials describe.
   *
   * The array IS the code: its length gives the number of outputs per input
   * bit, so `[0o171, 0o133]` is a rate-1/2 code and a three-element array is
   * rate 1/3. @p k is the constraint length, which fixes the register width
   * at `k - 1`.
   *
   * @p invert is a mask over the outputs, and it is not decoration: CCSDS
   * complements G2 and most codes complement nothing. An encoder built
   * without it round-trips perfectly against a decoder built without it, and
   * interoperates with nothing — which is why it is a parameter here rather
   * than a property of any one standard's configuration.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import ConvEncoder
   * >>> e = ConvEncoder([0o171, 0o133], k=7, invert=0x2)
   * >>> e.encode(np.zeros(8, dtype=np.uint8)).size
   * 16
   * @endcode
   *
   * @param poly      Generator polynomials, one per output. The array IS the
   *                  code; `poly_len` gives `n`.
   * @param poly_len  Number of polynomials, 1 to `CONV_N_MAX`.
   * @param k         Constraint length, 2 to `CONV_K_MAX`.
   * @param invert    Bit `j` complements output `j`.
   * @return          Heap-allocated state, or NULL if the code is unusable.
   * @note Caller must call conv_enc_destroy() when done.
   */
  conv_enc_state_t *conv_enc_create (const uint32_t *poly, size_t poly_len,
                                     uint32_t k, uint32_t invert);

  /**
   * @brief Build an encoder from a code already assembled.
   *
   * The declared `conv_enc_create` takes the polynomials directly, because a
   * struct pointer is not expressible in a manifest. Callers that already
   * hold a @ref conv_code_t — the CCSDS configuration, the validators — use
   * this.
   *
   * @param c  The code. Copied, so the caller's may be temporary.
   * @return   The encoder, or NULL if @p c is invalid.
   */
  conv_enc_state_t *conv_enc_create_code (const conv_code_t *c);

  /**
   * @brief Free an encoder. NULL is a no-op.
   * @param state  May be NULL.
   */
  void conv_enc_destroy (conv_enc_state_t *state);

  /**
   * @brief Return the register to all-zero, keeping the code.
   *
   * The boundary between two independent records, not a reconfiguration. The
   * next encode starts from the same state a freshly created encoder is in,
   * which is what makes a reset stream byte-identical to a fresh one.
   *
   * @param state  Must be non-NULL.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import ConvEncoder
   * >>> e = ConvEncoder([0o171, 0o133], k=7)
   * >>> e.reset()
   * @endcode
   */
  void conv_enc_reset (conv_enc_state_t *state);

  /**
   * @brief Symbols @ref conv_enc_encode writes for @p n_in input bits.
   *
   * Exactly `n_in * n` — a convolutional code has no fill and no latency on
   * the encode side, which is the asymmetry with @ref viterbi_decode_max_out,
   * where the traceback still owes bits at the start of a stream.
   *
   * @param state  The encoder.
   * @param n_in   Number of input bits.
   * @return       Symbols that call will write.
   */
  size_t conv_enc_encode_max_out (const conv_enc_state_t *state, size_t n_in);

  /**
   * @brief Encode information bits into channel symbols.
   *
   * The register carries across calls, so a long record may be fed in blocks
   * and the symbol sequence is identical to one call — which is the property
   * a standard fixes and a chunked encoder silently breaks.
   *
   * Outputs are emitted in polynomial order per input bit: for `[G1, G2]`,
   * `out[2i]` is `G1`'s symbol for input bit `i` and `out[2i+1]` is `G2`'s.
   *
   * @param state    The encoder.
   * @param in       @p n_in unpacked input bits, one per byte.
   * @param n_in     Number of input bits.
   * @param out      Receives `n_in * n` unpacked symbols, one per byte.
   * @param max_out  Capacity of @p out. Short is a refusal, not a truncation:
   *                 half a codeword is not a shorter codeword.
   * @return         Symbols written, or 0 if @p max_out is too small — in
   *                 which case @p out is untouched.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import ConvEncoder, Viterbi
   * >>> bits = np.array([1, 0, 1, 1, 0, 0, 1, 0] * 40, dtype=np.uint8)
   * >>> sym = ConvEncoder([0o171, 0o133], k=7).encode(bits)
   * >>> llr = np.where(sym, -8.0, 8.0).astype(np.float32)
   * >>> out = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)
   * >>> bool(np.array_equal(out, bits[: out.size]))
   * True
   * @endcode
   */
  size_t conv_enc_encode (conv_enc_state_t *state, const uint8_t *in,
                          size_t n_in, uint8_t *out, size_t max_out);

  /** @brief The code this encoder was built for. */
  const conv_code_t *conv_enc_code (const conv_enc_state_t *s);

  /* ── the state bytes interface ─────────────────────────────────────────
   *
   * The register is running state that survives between calls, so this
   * speaks the standard bytes interface like every other stateful object in
   * the tree. An encoder is a link in a chain, and one link that cannot be
   * checkpointed is enough to make the chain un-resumable. See
   * docs/design/state-serialization.md.
   */

  /** @brief Blob type tag: "CVEN". */
#define CONV_ENC_STATE_MAGIC DP_FOURCC ('C', 'V', 'E', 'N')
  /** @brief Blob format version. */
#define CONV_ENC_STATE_VERSION 1u

  /**
   * @brief Bytes @ref conv_enc_get_state writes: envelope, code identity and
   *        the register.
   *
   * A constant for this object, unlike the decoder's, whose ring is sized
   * from the configuration.
   */
  size_t conv_enc_state_bytes (const conv_enc_state_t *s);

  /**
   * @brief Serialize the register into @p blob.
   * @param s     The encoder.
   * @param blob  At least @ref conv_enc_state_bytes bytes.
   */
  void conv_enc_get_state (const conv_enc_state_t *s, void *blob);

  /**
   * @brief Restore a register from @p blob.
   *
   * The code identity travels in the blob and is CHECKED rather than
   * restored: `create()` already fixed the code, and a blob from a different
   * one describes a register that means something else. Refusing is the only
   * answer that cannot silently produce a stream no decoder matches.
   *
   * @param s     The encoder.
   * @param blob  A blob from @ref conv_enc_get_state.
   * @return      `DP_OK`, or `DP_ERR_INVALID` for a blob that is not this
   *              encoder's.
   */
  int conv_enc_set_state (conv_enc_state_t *s, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CONV_ENC_CORE_H */
