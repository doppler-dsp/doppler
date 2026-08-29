/**
 * @file cvt_core.h
 * @brief Cvt module — public C API.
 */
#ifndef CVT_CORE_H
#define CVT_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declare module-level functions here. */

  /**
   * @brief Bit order within a byte, for the bit/value conversions below.
   *
   * The name and the values follow numpy's `packbits`/`unpackbits`
   * `bitorder=` argument, because that is the convention anyone writing this
   * conversion has already met. It is a DIFFERENT axis from the `endian`
   * (`le`/`be`) the BLUE writer takes, which selects a file's BYTE order --
   * the `"EEEI"` / `"IEEE"` field of a type-1000 header. A literal's digit
   * order already fixes which byte comes first; what is left to choose is the
   * order of bits inside one. Overloading one word for both would let a sync
   * word and a sample stream disagree silently.
   */
  typedef enum
  {
    DP_BITORDER_BIG    = 0, /**< MSB of each byte first -- as written */
    DP_BITORDER_LITTLE = 1  /**< LSB of each byte first               */
  } dp_bitorder_t;

  /**
   * @brief The unit both directions walk in: 8 bits, then whatever is left.
   *
   * Written once so the value and the string forms cannot disagree about
   * where a short final unit begins -- the only place they could drift, and
   * the place a marker would then be expanded two ways.
   */
  static inline size_t
  cvt_unit_width (size_t done, size_t total)
  {
    const size_t left = total - done;
    return (left >= 8u) ? 8u : left;
  }

  /** @brief Where the i-th bit of a unit lands under @p bitorder. */
  static inline size_t
  cvt_bit_slot (size_t i, size_t width, int bitorder)
  {
    return (bitorder == DP_BITORDER_BIG) ? i : (width - 1u - i);
  }

/**
 * @brief Expand the low @p n_bits of an integer to unpacked bits.
 *
 * The form a frame field literal usually wants, and the one to reach for
 * first: exact, compiler-checked, with no failure mode a typo can reach.
 * @ref hex_to_bin is for the two cases this cannot serve -- a literal wider
 * than 64 bits, and text arriving from outside.
 *
 * Bit 0 out is the MOST significant of the @p n_bits requested under
 * @ref DP_BITORDER_BIG, which is what makes `int_to_bin(0x1A, 8, ...)` read
 * `0,0,0,1,1,0,1,0`. Only the low @p n_bits are read, so a caller need not
 * mask first.
 *
 * @param v         the value.
 * @param n_bits    1..64.
 * @param out       receives @p n_bits bytes, each 0 or 1.
 * @param out_len   capacity of @p out in bits.
 * @param bitorder  @ref DP_BITORDER_BIG or @ref DP_BITORDER_LITTLE.
 * @return @p n_bits, or 0 on refusal -- @p out untouched.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.cvt import int_to_bin
 * >>> b = np.zeros(8, np.uint8)
 * >>> int_to_bin(0x1A, 8, b, 0)          # 0 = big, MSB of each byte first
 * 8
 * >>> b.tolist()
 * [0, 0, 0, 1, 1, 0, 1, 0]
 *
 * @endcode
 */
size_t int_to_bin(uint64_t v, uint32_t n_bits, uint8_t *out, size_t out_len, int bitorder);
/**
 * @brief Expand a hex string to unpacked bits, one per byte.
 *
 * For what @ref int_to_bin cannot serve: a literal wider than 64 bits, or one
 * arriving as TEXT from a CLI flag or a JSON record. Each digit contributes
 * 4 bits and digits read left to right, so an ODD number of digits is
 * accepted and yields a 4-bit tail.
 *
 * A bad digit is a REFUSAL, never a skipped one: a typo'd marker that
 * silently shortens is the failure this exists to prevent, and it syncs to
 * nothing rather than failing loudly.
 *
 * @param hex       NUL-terminated `0-9a-fA-F`. No `0x`, no separators.
 * @param out       receives `4 * strlen(hex)` bytes, each 0 or 1.
 * @param out_len   capacity of @p out in bits.
 * @param bitorder  @ref DP_BITORDER_BIG or @ref DP_BITORDER_LITTLE.
 * @return bits written, or 0 on refusal -- @p out untouched.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.cvt import hex_to_bin
 * >>> b = np.zeros(32, np.uint8)
 * >>> hex_to_bin("1ACFFC1D", b, 0)       # the CCSDS attached sync marker
 * 32
 * >>> b[:8].tolist()
 * [0, 0, 0, 1, 1, 0, 1, 0]
 *
 * @endcode
 */
size_t hex_to_bin(const char * hex, uint8_t *out, size_t out_len, int bitorder);
/**
 * @brief Read unpacked bits back into an integer -- inverse of int_to_bin.
 *
 * Returns the value rather than a status, because that is the shape a
 * binding can carry. 0 is therefore both "the value zero" and "refused",
 * which is acceptable only because every refusal here is a programming error
 * in the WIDTH the caller chose (0, or over 64) or the bit order it named --
 * never a property of the data.
 *
 * @param bits      1..64 unpacked bits; any non-zero byte reads as 1.
 * @param bits_len  number of bits.
 * @param bitorder  @ref DP_BITORDER_BIG or @ref DP_BITORDER_LITTLE.
 * @return the value, or 0 on refusal.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.cvt import bin_to_int
 * >>> bits = np.array([0, 0, 0, 1, 1, 0, 1, 0], np.uint8)
 * >>> hex(bin_to_int(bits, 0))
 * '0x1a'
 *
 * @endcode
 */
uint64_t bin_to_int(const uint8_t *bits, size_t bits_len, int bitorder);
/**
 * @brief Render unpacked bits to hex digits -- inverse of hex_to_bin.
 *
 * The digits come back as ASCII BYTES rather than a string: jm has no string
 * out-parameter, and `uint8_t` is the same type as the `unsigned char` a C
 * caller would use anyway. A NUL is written after the digits.
 *
 * @param bits      unpacked bits; any non-zero byte reads as 1.
 * @param bits_len  number of bits; must be a multiple of 4.
 * @param out       receives the digits plus a NUL.
 * @param out_len   capacity of @p out in bytes, NUL included.
 * @param bitorder  @ref DP_BITORDER_BIG or @ref DP_BITORDER_LITTLE.
 * @return digits written, NOT counting the NUL, or 0 on refusal.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.cvt import hex_to_bin, bin_to_hex
 * >>> b = np.zeros(32, np.uint8)
 * >>> hex_to_bin("1acffc1d", b, 0)
 * 32
 * >>> h = np.zeros(16, np.uint8)
 * >>> n = bin_to_hex(b, h, 0)
 * >>> bytes(h[:n]).decode()
 * '1acffc1d'
 *
 * @endcode
 */
size_t bin_to_hex(const uint8_t *bits, size_t bits_len, uint8_t *out, size_t out_len, int bitorder);
/**
 * @brief Map unpacked bits to bipolar NRZ symbols: 0 -> +1, 1 -> -1.
 *
 * That is `1 - 2*b`, and the convention's HOME is `mpsk_core.h`: BPSK is
 * M-PSK at m = 2, where phi0 is 0, so label 0 lands at +1 and label 1 at -1.
 * This states the same thing in the form a per-bit loop can afford, and
 * `test_cvt_core` asserts the two agree rather than trusting them to. A
 * mapper that disagreed with the receiver's would decode every bit INVERTED
 * while looking perfectly locked -- which a round-trip test cannot see.
 *
 * @param bits      unpacked bits; any non-zero byte reads as 1.
 * @param bits_len  number of bits.
 * @param out       receives @p bits_len symbols, each +1.0f or -1.0f.
 * @param out_len   capacity of @p out in symbols.
 * @return symbols written, or 0 on refusal.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.cvt import bin_to_nrz
 * >>> bits = np.array([0, 1, 1, 0], np.uint8)
 * >>> sym = np.zeros(4, np.float32)
 * >>> bin_to_nrz(bits, sym)
 * 4
 * >>> sym.tolist()
 * [1.0, -1.0, -1.0, 1.0]
 *
 * @endcode
 */
size_t bin_to_nrz(const uint8_t *bits, size_t bits_len, float *out, size_t out_len);
/**
 * @brief Hard-decide NRZ symbols back to bits -- inverse of bin_to_nrz.
 *
 * Negative is a 1; zero and positive are a 0, matching `1 - 2*b`. Exactly
 * zero decides to 0 rather than a coin toss, so the mapping is TOTAL and a
 * round trip is exact. A caller that wants an erasure handled as an erasure
 * wants a soft demapper, not this.
 *
 * @param nrz       symbols.
 * @param nrz_len   number of symbols.
 * @param out       receives @p nrz_len bytes, each 0 or 1.
 * @param out_len   capacity of @p out in bits.
 * @return bits written, or 0 on refusal.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.cvt import nrz_to_bin
 * >>> sym = np.array([1.0, -1.0, -1.0, 1.0], np.float32)
 * >>> bits = np.zeros(4, np.uint8)
 * >>> nrz_to_bin(sym, bits)
 * 4
 * >>> bits.tolist()
 * [0, 1, 1, 0]
 *
 * @endcode
 */
size_t nrz_to_bin(const float *nrz, size_t nrz_len, uint8_t *out, size_t out_len);
#ifdef __cplusplus
}
#endif

#endif /* CVT_CORE_H */
