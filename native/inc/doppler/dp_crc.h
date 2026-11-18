/**
 * @file dp_crc.h
 * @brief One parametric CRC: the Rocksoft model over a bit stream.
 *
 * A CRC is a model, not an algorithm: a width, a generator polynomial, an
 * initial register, whether each input octet is reflected, whether the final
 * register is, and a value XORed into the result. Every named CRC is those
 * six numbers. This header
 * holds the one loop that computes any of them and the named instances the
 * tree uses, so a frame's FCS (CRC-32) and the burst frame's trailer
 * (CRC-16-CCITT) are rows of data and cannot grow copies of the arithmetic.
 * (The SIG fields' CRC-8 parameters are not a named row: they are the
 * protocol's to state, see the last paragraph.)
 *
 * Header-only, for the reason `dp_crc16.h` was: no component grows a
 * link-line dependency for a ten-line kernel.
 *
 * ## The input is a bit stream; `refin` is for the caller holding octets
 *
 * The library moves bits unpacked, one per byte in the LSB (`conv`, the
 * interleaver, a frame's stages). dp_crc_bits() takes exactly that and
 * shifts the first bit presented into the register first, MSB-first. The
 * catalogue's `refin` ("reflect each input byte") is a field of the model
 * for a caller holding octets: dp_crc_bytes() feeds each octet LSB-first when
 * `refin` is set, and calls the same bit loop. dp_crc_bits() never reads it:
 * a stream already in bit order has nothing left to reflect, so for it
 * `refin` is a property of the stream the caller builds. `refout` acts on the
 * result, in both.
 *
 * ## Where the check values come from
 *
 * The named instances below carry the catalogue's names and parameters
 * (the CRC RevEng catalogue, https://reveng.sourceforge.io/crc-catalogue/),
 * and `test_dp_crc.c` pins each to the catalogue's check value, the CRC of
 * the nine ASCII characters `123456789`. Those values define the named CRC.
 * That is what the test holds: each named model's check value, nothing
 * more.
 *
 * **A named instance is not a claim that a standard uses it.** Which model a
 * given protocol field takes, and in what bit order the value is
 * transmitted, are facts in that protocol's text. Nothing here says so.
 */
#ifndef DP_CRC_H
#define DP_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A CRC model: width, polynomial, initial register, output
 *        reflection, output XOR and input reflection.
 *
 * The polynomial is written in normal form, MSB-first, without the top
 * term: CRC-32's `x^32 + x^26 + ... + 1` is `0x04C11DB7`, and CRC-8's
 * `x^8 + x^2 + x + 1` is `0x07`.
 */
typedef struct
{
  unsigned width;  /**< register width in bits, 1..64                    */
  uint64_t poly;   /**< generator polynomial, normal form, `width` bits  */
  uint64_t init;   /**< initial register, `width` bits                   */
  int      refout; /**< non-zero: bit-reverse the register before xorout */
  uint64_t xorout; /**< XORed into the (possibly reflected) register     */
  int      refin;  /**< non-zero: dp_crc_bytes() feeds each octet LSB-first;
                        the catalogue's `refin`. dp_crc_bits() ignores it:
                        its stream is already in bit order. Last, so a
                        literal written before this field existed compiles
                        as `refin = 0`: write models with designated
                        initializers.                                     */
} dp_crc_t;

/** @brief CRC-32/ISO-HDLC (Ethernet, zlib). Check value 0xCBF43926. Its
 *         catalogue `refin` is true: feed octets LSB-first, which
 *         dp_crc_bytes() does for it. */
#define DP_CRC_32_ISO_HDLC \
  ((dp_crc_t){ .width = 32u, .poly = 0x04C11DB7u, .init = 0xFFFFFFFFu, \
               .refout = 1, .xorout = 0xFFFFFFFFu, .refin = 1 })
/** @brief CRC-32/BZIP2: the same polynomial, not reflected. Check
 *         0xFC891918, `refin` false. */
#define DP_CRC_32_BZIP2 \
  ((dp_crc_t){ .width = 32u, .poly = 0x04C11DB7u, .init = 0xFFFFFFFFu, \
               .refout = 0, .xorout = 0xFFFFFFFFu, .refin = 0 })
/** @brief CRC-32C (Castagnoli, iSCSI). Check 0xE3069283, `refin` true. */
#define DP_CRC_32C \
  ((dp_crc_t){ .width = 32u, .poly = 0x1EDC6F41u, .init = 0xFFFFFFFFu, \
               .refout = 1, .xorout = 0xFFFFFFFFu, .refin = 1 })
/** @brief CRC-16/IBM-3740, formerly CRC-16/CCITT-FALSE: the burst frame's
 *         trailer. Check 0x29B1, `refin` false. */
#define DP_CRC_16_IBM_3740 \
  ((dp_crc_t){ .width = 16u, .poly = 0x1021u, .init = 0xFFFFu, \
               .refout = 0, .xorout = 0u, .refin = 0 })
/** @brief CRC-16/XMODEM. Check 0x31C3, `refin` false. */
#define DP_CRC_16_XMODEM \
  ((dp_crc_t){ .width = 16u, .poly = 0x1021u, .init = 0u, \
               .refout = 0, .xorout = 0u, .refin = 0 })
/** @brief CRC-16/KERMIT. Check 0x2189, `refin` true. */
#define DP_CRC_16_KERMIT \
  ((dp_crc_t){ .width = 16u, .poly = 0x1021u, .init = 0u, \
               .refout = 1, .xorout = 0u, .refin = 1 })
/** @brief CRC-8/SMBUS. Check 0xF4, `refin` false. */
#define DP_CRC_8_SMBUS \
  ((dp_crc_t){ .width = 8u, .poly = 0x07u, .init = 0u, \
               .refout = 0, .xorout = 0u, .refin = 0 })
/** @brief CRC-8/ROHC. Check 0xD0, `refin` true. */
#define DP_CRC_8_ROHC \
  ((dp_crc_t){ .width = 8u, .poly = 0x07u, .init = 0xFFu, \
               .refout = 1, .xorout = 0u, .refin = 1 })
/** @brief CRC-64/ECMA-182. Check 0x6C40DF5F0B497347, `refin` false. */
#define DP_CRC_64_ECMA_182 \
  ((dp_crc_t){ .width = 64u, .poly = 0x42F0E1EBA9EA3693ull, .init = 0u, \
               .refout = 0, .xorout = 0u, .refin = 0 })

/* The register's mask: `width` low bits set. */
static inline uint64_t
dp_crc_mask (unsigned width)
{
  return width == 64u ? ~(uint64_t)0 : (((uint64_t)1 << width) - 1u);
}

/**
 * @brief Is @p p a model this header can compute?
 *
 * @param p  The model.
 * @return   Non-zero if `width` is in `[1, 64]` and `poly`, `init` and
 *           `xorout` fit in `width` bits. A polynomial of zero is allowed
 *           (it is the degenerate CRC that shifts its register out).
 */
static inline int
dp_crc_valid (const dp_crc_t *p)
{
  if (p == NULL || p->width < 1u || p->width > 64u)
    return 0;
  const uint64_t mask = dp_crc_mask (p->width);
  return (p->poly & ~mask) == 0u && (p->init & ~mask) == 0u
         && (p->xorout & ~mask) == 0u;
}

/** @brief The register's bits, reversed within @p width. */
static inline uint64_t
dp_crc_reflect (uint64_t v, unsigned width)
{
  uint64_t r = 0;
  for (unsigned i = 0; i < width; i++)
    r |= ((v >> i) & 1u) << (width - 1u - i);
  return r;
}

/**
 * @brief Shift one bit into the register: the only place the model's
 *        arithmetic is written.
 *
 * dp_crc_bits() and dp_crc_bytes() both call this, so there is one step and
 * the two entry points cannot disagree about it.
 *
 * @param p    A valid model.
 * @param reg  The register, `width` bits.
 * @param bit  The bit; only the LSB is used.
 * @return     The register after the bit.
 */
static inline uint64_t
dp_crc_step (const dp_crc_t *p, uint64_t reg, unsigned bit)
{
  const uint64_t top = (reg >> (p->width - 1u)) & 1u;
  reg                = (reg << 1) & dp_crc_mask (p->width);
  return (top ^ (uint64_t)(bit & 1u)) ? reg ^ p->poly : reg;
}

/** @brief The register's result: reflected if the model says so, then
 *         XORed. The one place that happens. */
static inline uint64_t
dp_crc_finish (const dp_crc_t *p, uint64_t reg)
{
  if (p->refout)
    reg = dp_crc_reflect (reg, p->width);
  return reg ^ p->xorout;
}

/**
 * @brief The CRC of a bit stream: first bit presented is shifted in first.
 *
 * Each input byte carries ONE bit (0/1) in its LSB, the form the frame paths
 * pass around. The register shifts MSB-first, so presenting the bits in
 * transmission order gives the usual CRC of a transmitted message, and an
 * `refin = true` catalogue model needs each octet's bits presented LSB-first
 * (see dp_crc_bytes()).
 *
 * The value is returned right-aligned in `width` bits. How it is transmitted,
 * MSB-first or otherwise, is the protocol's, not this function's.
 *
 * @param p     A valid model (see dp_crc_valid(); not checked here).
 * @param bits  @p n bytes, each 0 or 1 (only the LSB is used).
 * @param n     Number of bits.
 * @return      The CRC, in the low `p->width` bits.
 */
static inline uint64_t
dp_crc_bits (const dp_crc_t *p, const uint8_t *bits, size_t n)
{
  uint64_t reg = p->init;
  for (size_t i = 0; i < n; i++)
    reg = dp_crc_step (p, reg, bits[i]);
  return dp_crc_finish (p, reg);
}

/**
 * @brief The CRC of octets, the model's `refin` choosing each octet's bit
 *        order.
 *
 * Presents every octet's bits to the same step as dp_crc_bits(): LSB-first
 * when `p->refin` is non-zero (the reflected CRCs: ISO-HDLC, CRC-32C, KERMIT,
 * ROHC), MSB-first when it is zero.
 *
 * @param p      A valid model.
 * @param bytes  @p n octets.
 * @param n      Number of octets.
 * @return       The CRC, in the low `p->width` bits.
 */
static inline uint64_t
dp_crc_bytes (const dp_crc_t *p, const uint8_t *bytes, size_t n)
{
  uint64_t reg = p->init;
  for (size_t i = 0; i < n; i++)
    for (unsigned k = 0; k < 8u; k++)
      reg = dp_crc_step (p, reg,
                         p->refin ? (bytes[i] >> k) & 1u
                                  : (bytes[i] >> (7u - k)) & 1u);
  return dp_crc_finish (p, reg);
}

#ifdef __cplusplus
}
#endif

#endif /* DP_CRC_H */
