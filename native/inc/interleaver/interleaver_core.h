/**
 * @file interleaver_core.h
 * @brief Block interleaving as an object — the geometry, held.
 *
 * The transform itself is `dp_interleave.h`, header-only and stateless.
 * **This is not a second implementation**: every method here calls that one.
 * The split is `conv`/`conv_enc`'s — one owns the arithmetic, the other owns
 * the configured thing a caller holds and reuses.
 *
 * What holding it buys is that the geometry is DECLARED rather than inferred
 * from whatever length arrives. A block interleaver only works if the
 * transmitter and the receiver agree on the permutation, and deriving `cols`
 * from the input length means a truncated frame silently produces a
 * DIFFERENT permutation instead of an error.
 *
 * Stateless, deliberately, and therefore not serializable. Interleaving is
 * per-frame: carrying a partial block across frames would add frame-latency
 * and break per-frame decoding, so a call takes a whole number of blocks or
 * is refused. Nothing survives between calls, so there is nothing to
 * checkpoint — the exemption `docs/design/state-serialization.md` grants a
 * pure converter.
 *
 * @code
 *   interleaver_state_t *il = interleaver_create (8, 32, 1);
 *   uint8_t tx[256], rx[256];
 *   interleaver_interleave (il, bits, 256, tx, sizeof tx);
 *   interleaver_deinterleave (il, tx, 256, rx, sizeof rx);
 *   interleaver_destroy (il);
 * @endcode
 */
#ifndef INTERLEAVER_CORE_H
#define INTERLEAVER_CORE_H

#include "dp_interleave.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief A block interleaver's geometry.
   *
   * Three numbers and no buffers: the permutation is arithmetic, so there is
   * nothing to allocate per block and nothing to grow.
   */
  typedef struct
  {
    size_t rows;      /**< interleaving depth — codewords interleaved   */
    size_t cols;      /**< block span — units per codeword              */
    size_t unit_bits; /**< bits per interleaved unit; 1 bit, 8 octet    */
  } interleaver_state_t;

  /**
   * @brief Build an interleaver over a @p rows x @p cols block of
   *        @p unit_bits units.
   *
   * @param rows      Interleaving depth; the longest burst fully spread.
   *                  Must be non-zero.
   * @param cols      Units per codeword. Must be non-zero.
   * @param unit_bits Bits per interleaved unit. 1 interleaves bits; 8
   *                  interleaves octets, which is what spreads a burst
   *                  across the codewords of a symbol-oriented code such as
   *                  Reed-Solomon over GF(256). Must be non-zero.
   * @return An interleaver, or NULL if any parameter is zero or the block
   *         would overflow.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import Interleaver
   * >>> il = Interleaver(rows=3, cols=4)
   * >>> il.block_bits, il.burst_len, il.separation
   * (12, 3, 4)
   *
   * @endcode
   */
  interleaver_state_t *interleaver_create (size_t rows, size_t cols,
                                           size_t unit_bits);

  /**
   * @brief The RECEIVE face of the same interleaver.
   *
   * Identical construction — it delegates to @c interleaver_create — and it
   * exists because the two ends of a link are written by different people.
   * Someone working the receive side reaches for a `Deinterleaver`, and a
   * class that is only findable under the transmit name is a class they do
   * not find.
   *
   * The GEOMETRY is why this is a view over one core rather than a second
   * object: `rows`, `cols` and `unit_bits` are exactly what the two ends must
   * agree on, and a mismatch is not an error but a receiver de-interleaving
   * into a different permutation and handing the decoder plausible garbage.
   * One core means one definition of the geometry to get right.
   *
   * @param rows      Interleaving depth, as the transmitter used.
   * @param cols      Units per codeword, as the transmitter used.
   * @param unit_bits Bits per interleaved unit, as the transmitter used.
   * @return An interleaver, or NULL on the same refusals as
   *         @c interleaver_create.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import Interleaver, Deinterleaver
   * >>> tx = Interleaver(rows=3, cols=4)
   * >>> rx = Deinterleaver(rows=3, cols=4)
   * >>> bits = np.arange(12, dtype=np.uint8)
   * >>> wire = np.asarray(tx.interleave(bits))
   * >>> np.array_equal(np.asarray(rx.deinterleave(wire)), bits)
   * True
   *
   * @endcode
   */
  interleaver_state_t *interleaver_create_rx (size_t rows, size_t cols,
                                              size_t unit_bits);

  /**
   * @brief Release an interleaver.
   *
   * A no-op on NULL, like @c free. The object owns nothing but its three
   * numbers, so this is one @c free and there is no buffer to drain first.
   *
   * @param state The interleaver, or NULL.
   *
   * @code
   * >>> from doppler.coding import Interleaver
   * >>> il = Interleaver(rows=2, cols=2)
   * >>> il.destroy()
   *
   * @endcode
   */
  void interleaver_destroy (interleaver_state_t *state);

  /**
   * @brief No-op; an interleaver carries nothing between calls.
   *
   * Present because the object surface has it, and honest about why it does
   * nothing: a reset that pretended to clear something would suggest there
   * was something to clear. The geometry is configuration, not state, so it
   * survives — a reset that cleared THAT would leave every later call
   * refusing.
   *
   * @param state The interleaver.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import Interleaver
   * >>> il = Interleaver(rows=2, cols=3)
   * >>> il.reset()
   * >>> il.block_bits
   * 6
   *
   * @endcode
   */
  void interleaver_reset (interleaver_state_t *state);

  /**
   * @brief Bits in one block — `rows * cols * unit_bits`.
   * @param state The interleaver.
   * @return The block size in bits.
   *
   * @code
   * >>> from doppler.coding import Interleaver
   * >>> Interleaver(rows=8, cols=32, unit_bits=8).block_bits
   * 2048
   *
   * @endcode
   */
  size_t interleaver_get_block_bits (const interleaver_state_t *state);

  /**
   * @brief Output bits for @p n_in input bits — the same number.
   *
   * A permutation moves bits and does not add or remove any, so this is the
   * identity. It exists because the binding asks a method how much room its
   * output needs, and answering "the same" is not something a caller should
   * have to know.
   *
   * @param state The interleaver.
   * @param n_in  Input length in bits.
   * @return @p n_in.
   *
   * @code
   * >>> from doppler.coding import Interleaver
   * >>> Interleaver(rows=4, cols=8).interleave_max_out(32)
   * 32
   *
   * @endcode
   */
  size_t interleaver_interleave_max_out (const interleaver_state_t *state,
                                         size_t n_in);

  /**
   * @brief Output bits for @p n_in input bits — the same number.
   *
   * Identical to @c interleaver_interleave_max_out, and for the same reason:
   * the inverse of a permutation is a permutation.
   *
   * @param state The interleaver.
   * @param n_in  Input length in bits.
   * @return @p n_in.
   *
   * @code
   * >>> from doppler.coding import Interleaver
   * >>> Interleaver(rows=4, cols=8).deinterleave_max_out(32)
   * 32
   *
   * @endcode
   */
  size_t interleaver_deinterleave_max_out (const interleaver_state_t *state,
                                           size_t n_in);

  /**
   * @brief Output values for @p n_in soft input values — the same number.
   *
   * @param state The interleaver.
   * @param n_in  Input length in values.
   * @return @p n_in.
   *
   * @code
   * >>> from doppler.coding import Interleaver
   * >>> Interleaver(rows=4, cols=8).deinterleave_soft_max_out(32)
   * 32
   *
   * @endcode
   */
  size_t
  interleaver_deinterleave_soft_max_out (const interleaver_state_t *state,
                                         size_t n_in);

  /**
   * @brief Interleave a whole number of blocks.
   *
   * @param state   The interleaver.
   * @param in      @p n_in bits, one bit per byte.
   * @param n_in    Input length in bits; must be a non-zero multiple of
   *                @c interleaver_get_block_bits.
   * @param out     Where to write @p n_in bits; must not overlap @p in.
   * @param max_out Room in @p out, in bits.
   * @return @p n_in on success, 0 if the length is not a whole number of
   *         blocks or @p out is too small. A partial block is REFUSED rather
   *         than padded: padding changes the length, and a receiver that
   *         de-interleaved the padded block would recover different bits.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import Interleaver
   * >>> il = Interleaver(rows=3, cols=4)
   * >>> x = np.arange(12, dtype=np.uint8)
   * >>> np.asarray(il.interleave(x)).tolist()
   * [0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11]
   *
   * @endcode
   */
  size_t interleaver_interleave (interleaver_state_t *state,
                                 const uint8_t *in, size_t n_in, uint8_t *out,
                                 size_t max_out);

  /**
   * @brief Undo @ref interleaver_interleave over the same geometry.
   * @param state   The interleaver.
   * @param in      @p n_in interleaved bits, one bit per byte.
   * @param n_in    Input length in bits; a whole number of blocks.
   * @param out     Where to write @p n_in bits; must not overlap @p in.
   * @param max_out Room in @p out, in bits.
   * @return @p n_in, or 0 on a refusal.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import Interleaver
   * >>> il = Interleaver(rows=3, cols=4)
   * >>> x = np.arange(12, dtype=np.uint8)
   * >>> y = np.asarray(il.interleave(x))
   * >>> np.array_equal(np.asarray(il.deinterleave(y)), x)
   * True
   *
   * @endcode
   */
  size_t interleaver_deinterleave (interleaver_state_t *state,
                                   const uint8_t *in, size_t n_in,
                                   uint8_t *out, size_t max_out);

  /**
   * @brief Undo an interleave over SOFT values — the receive path that
   *        matters.
   *
   * `dsss_burst_receiver`'s `llrs` span the whole frame, and an outer decoder
   * wants them de-interleaved BEFORE it runs. Slicing to hard bits first and
   * de-interleaving those throws away the confidence the soft output exists
   * to carry, which is most of what an outer code is for.
   *
   * There is no `interleave_soft`: a transmitter has bits, not LLRs.
   *
   * @param state   The interleaver.
   * @param in      @p n_in soft values, one per interleaved unit-bit.
   * @param n_in    Input length in values; a whole number of blocks.
   * @param out     Where to write @p n_in values; must not overlap @p in.
   * @param max_out Room in @p out, in values.
   * @return @p n_in, or 0 on a refusal.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.coding import Interleaver
   * >>> il = Interleaver(rows=2, cols=3)
   * >>> llr = np.array([1., 2., 3., 4., 5., 6.], dtype=np.float32)
   * >>> np.asarray(il.deinterleave_soft(llr)).tolist()
   * [1.0, 3.0, 5.0, 2.0, 4.0, 6.0]
   *
   * @endcode
   */
  size_t interleaver_deinterleave_soft (interleaver_state_t *state,
                                        const float *in, size_t n_in,
                                        float *out, size_t max_out);

  /**
   * @brief The longest burst this geometry fully spreads — @c rows.
   *
   * A burst of up to this many consecutive units on the wire touches each
   * codeword at most once, so an outer code correcting @c t units per
   * codeword survives a burst of @c t times this.
   *
   * @code
   * >>> from doppler.coding import Interleaver
   * >>> Interleaver(rows=5, cols=51, unit_bits=8).burst_len
   * 5
   *
   * @endcode
   */
  size_t interleaver_get_burst_len (const interleaver_state_t *state);

  /**
   * @brief Units per codeword — @c cols.
   *
   * The other half of the link budget: what @ref interleaver_get_burst_len
   * spreads a burst ACROSS.
   *
   * @code
   * >>> from doppler.coding import Interleaver
   * >>> Interleaver(rows=5, cols=51, unit_bits=8).separation
   * 51
   *
   * @endcode
   */
  size_t interleaver_get_separation (const interleaver_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* INTERLEAVER_CORE_H */
