/**
 * @file dp_interleave.h
 * @brief Block interleaving — the permutation, and nothing else.
 *
 * A block interleaver writes its input by ROWS into a @p rows x @p cols
 * matrix and reads it back by COLUMNS. That is the whole transform. It
 * carries no state, adds no redundancy and detects nothing; what it buys is
 * that a burst of errors on the wire arrives at the decoder spread out.
 *
 * The two numbers are a link budget, not a tuning pair. Write one CODEWORD
 * per row, @p cols units long, @p rows of them:
 *
 * - a burst of up to @p rows consecutive OUTPUT units touches each codeword
 *   AT MOST ONCE;
 * - two originally adjacent INPUT units land @p rows apart on the wire.
 *
 * So @p rows is the longest burst fully spread and @p cols is the codeword
 * length. An outer code correcting @c t units per codeword survives a burst
 * of @c t * @p rows.
 *
 * The invariant is one-per-CODEWORD, not a minimum index separation. Two
 * burst positions either side of a column boundary can land as close as
 * @p cols - 1 apart while still being in different codewords, which is what
 * matters; a first draft of this header claimed the separation instead and
 * `test_dp_interleave.c` refused it.
 *
 * The UNIT is a parameter and not a detail. Interleaving octets is what
 * spreads a burst across the codewords of a symbol-oriented code such as
 * Reed-Solomon over GF(256); interleaving bits inside such a code spreads a
 * burst WITHIN a symbol, which buys nothing, because the symbol is already
 * wrong. Match the unit to the code the interleaver protects.
 *
 * Not the CCSDS interleaver. `ccsds_tm/rs.c` also interleaves, and it is a
 * different transform sharing a name: depth-I interleaving is intrinsic to
 * the Reed-Solomon codeblock layout (131.0-B-6 4.4.1) and is fused into
 * encode and decode, not a permutation applied afterwards. Neither can be
 * written in terms of the other.
 *
 * Not a convolutional (Forney) interleaver. That is a different structure
 * with different latency and memory, and it is deliberately absent rather
 * than pending — see doppler#1031.
 *
 * Header-only, like `dp_crc16.h` and for the same reason: the frame stage
 * kernel and the `Interleaver` object both need it, and neither should grow
 * a link-line dependency for arithmetic.
 */
#ifndef DP_INTERLEAVE_H
#define DP_INTERLEAVE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Where input unit @p i lands in the interleaved output.
 *
 * Unit @p i sits at row `i / cols`, column `i % cols` of the write-by-rows
 * matrix; reading by columns puts it at `(i % cols) * rows + (i / cols)`.
 *
 * @param i     Input unit index, below `rows * cols`.
 * @param rows  Interleaving depth.
 * @param cols  Block span.
 * @return The output unit index.
 */
static inline size_t
dp_interleave_index (size_t i, size_t rows, size_t cols)
{
  return (i % cols) * rows + (i / cols);
}

/**
 * @brief Where output unit @p o came from — the inverse permutation.
 *
 * Identical to `dp_interleave_index` with @p rows and @p cols exchanged,
 * which is a fact about the transform rather than a coincidence: reading a
 * `rows x cols` matrix by columns is writing a `cols x rows` one by rows. It
 * is why every function below undoes itself by swapping two arguments, and
 * why a SQUARE block is its own inverse.
 *
 * @param o     Output unit index, below `rows * cols`.
 * @param rows  Interleaving depth, as given to the forward transform.
 * @param cols  Block span, as given to the forward transform.
 * @return The input unit index it came from.
 */
static inline size_t
dp_deinterleave_index (size_t o, size_t rows, size_t cols)
{
  return dp_interleave_index (o, cols, rows);
}

/**
 * @brief Units in one block — `rows * cols`.
 *
 * The length every call below consumes and produces, in UNITS. Multiply by
 * the unit size for elements.
 *
 * @param rows  Interleaving depth.
 * @param cols  Block span.
 * @return The block size in units.
 */
static inline size_t
dp_interleave_block_units (size_t rows, size_t cols)
{
  return rows * cols;
}

/**
 * @brief Interleave one block of opaque fixed-size units.
 *
 * The generic kernel the typed wrappers below call. @p in and @p out must
 * not overlap: a block interleave is a transpose, so doing it in place needs
 * cycle-following and is a different algorithm, not an option here.
 *
 * @param in         `rows * cols * unit_bytes` bytes of input.
 * @param out        Where to write the same number of bytes.
 * @param rows       Interleaving depth.
 * @param cols       Block span.
 * @param unit_bytes Bytes per interleaved unit; 0 writes nothing.
 */
static inline void
dp_interleave_raw (const void *in, void *out, size_t rows, size_t cols,
                   size_t unit_bytes)
{
  const unsigned char *s = (const unsigned char *)in;
  unsigned char       *d = (unsigned char *)out;
  for (size_t r = 0; r < rows; r++)
    for (size_t c = 0; c < cols; c++)
      memcpy (d + (c * rows + r) * unit_bytes,
              s + (r * cols + c) * unit_bytes, unit_bytes);
}

/**
 * @brief Interleave one block of unpacked bits or octets.
 *
 * The array form doppler's frame paths use: one bit per byte. @p unit is in
 * BYTES of that array, so `unit == 1` interleaves bits and `unit == 8`
 * interleaves octets of a bit-per-byte stream.
 *
 * @param in    `rows * cols * unit` bytes of input.
 * @param out   Where to write them; must not overlap @p in.
 * @param rows  Interleaving depth.
 * @param cols  Block span.
 * @param unit  Bytes per interleaved unit.
 */
static inline void
dp_interleave_u8 (const uint8_t *in, uint8_t *out, size_t rows, size_t cols,
                  size_t unit)
{
  dp_interleave_raw (in, out, rows, cols, unit);
}

/**
 * @brief Undo `dp_interleave_u8` over a block of the same geometry.
 *
 * @param in    `rows * cols * unit` bytes of interleaved input.
 * @param out   Where to write them; must not overlap @p in.
 * @param rows  Interleaving depth, as given to the forward transform.
 * @param cols  Block span, as given to the forward transform.
 * @param unit  Bytes per interleaved unit.
 */
static inline void
dp_deinterleave_u8 (const uint8_t *in, uint8_t *out, size_t rows, size_t cols,
                    size_t unit)
{
  dp_interleave_raw (in, out, cols, rows, unit);
}

/**
 * @brief Interleave one block of soft values.
 *
 * @param in    `rows * cols * unit` floats of input.
 * @param out   Where to write them; must not overlap @p in.
 * @param rows  Interleaving depth.
 * @param cols  Block span.
 * @param unit  Floats per interleaved unit.
 */
static inline void
dp_interleave_f32 (const float *in, float *out, size_t rows, size_t cols,
                   size_t unit)
{
  dp_interleave_raw (in, out, rows, cols, unit * sizeof (float));
}

/**
 * @brief Undo `dp_interleave_f32` — the soft-decision receive path.
 *
 * This is the one a receiver actually needs. `dsss_burst_receiver`'s `llrs`
 * span the whole frame, and an outer decoder wants them de-interleaved
 * BEFORE it runs; hard-decision de-interleaving would throw away the
 * confidence the soft output exists to carry.
 *
 * @param in    `rows * cols * unit` floats of interleaved input.
 * @param out   Where to write them; must not overlap @p in.
 * @param rows  Interleaving depth, as given to the forward transform.
 * @param cols  Block span, as given to the forward transform.
 * @param unit  Floats per interleaved unit.
 */
static inline void
dp_deinterleave_f32 (const float *in, float *out, size_t rows, size_t cols,
                     size_t unit)
{
  dp_interleave_raw (in, out, cols, rows, unit * sizeof (float));
}

#ifdef __cplusplus
}
#endif

#endif /* DP_INTERLEAVE_H */
