/**
 * @file test_dp_crc.c
 * @brief Pins dp_crc.h: the catalogue's check values, an independent
 *        runtime's CRCs, the properties a CRC must have, and that
 *        dp_crc16_ccitt is the same arithmetic as it always was.
 *
 * What each section answers to, and what it does not:
 *
 * 1. The CRC RevEng catalogue's check values (the CRC of `123456789`),
 *    which DEFINE the named models. They were confirmed outside this repo
 *    before being pinned: CRC-32/ISO-HDLC against zlib's crc32,
 *    CRC-16/XMODEM and CRC-16/IBM-3740 against Python's binascii.crc_hqx,
 *    and all nine against a separate table-free reflected/non-reflected
 *    implementation of the model. A failing value here means the primitive
 *    is wrong, never the value.
 * 2. Longer messages whose CRC-32 and CRC-16 values come from zlib and
 *    binascii, so a stream longer than the check string is held too.
 * 3. Properties no implementation detail can fake: a CRC-1 is a parity
 *    check (compared with a popcount), every single-bit error and every
 *    burst no longer than the register is detected, and refout is
 *    nothing but a bit-reversal of the result.
 * 4. dp_crc16_ccitt equals the 10-line kernel it replaced, kept here
 *    verbatim as the oracle, on random streams of random lengths: the CRC
 *    the burst frame carries did not move.
 * 5. Refusals: dp_crc_valid() on a width of 0 or 65 and on fields wider
 *    than the width.
 *
 * Nothing here says a protocol uses any model; see dp_crc.h.
 */
#include "doppler/dp_crc.h"
#include "doppler/dp_crc16.h"
#include "dp_rng_test.h"
#include "dp_test.h"

#include <string.h>

/* The 10-line kernel dp_crc16_ccitt used to be, verbatim, as the oracle for
   section 4. It is the previous implementation, not a second one in the
   tree: it lives in this test only. */
static uint16_t
old_crc16_ccitt (const uint8_t *bits, size_t n)
{
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < n; i++)
    {
      crc ^= (uint16_t)((bits[i] & 1u) << 15);
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                            : (uint16_t)(crc << 1);
    }
  return crc;
}

/* Octets to the bit stream a model with this `refin` is fed. */
static size_t
to_bits (const uint8_t *by, size_t n, int refin, uint8_t *bits)
{
  size_t m = 0;
  for (size_t i = 0; i < n; i++)
    for (unsigned k = 0; k < 8u; k++)
      bits[m++] = refin ? (uint8_t)((by[i] >> k) & 1u)
                        : (uint8_t)((by[i] >> (7u - k)) & 1u);
  return m;
}

static int
parity (const uint8_t *bits, size_t n)
{
  int p = 0;
  for (size_t i = 0; i < n; i++)
    p ^= bits[i] & 1;
  return p;
}

int
main (void)
{
  /* The catalogue's message, the nine ASCII digits 1 to 9. Spelled as nine
   * characters because a string initializer would imply a tenth, the NUL, and
   * -Wunterminated-string-initialization (gcc 15, clang 21) warns that a nine
   * byte array drops it. */
  static const uint8_t CHECK[9]
      = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  uint8_t bits[8 * 9];

  /* ── 1. the catalogue's check values, both entry points ────────────── */
  static const struct
  {
    const char *name;
    dp_crc_t    model;
    int         refin;
    uint64_t    check;
  } CAT[] = {
    { "CRC-32/ISO-HDLC", DP_CRC_32_ISO_HDLC, 1, 0xCBF43926u },
    { "CRC-32/BZIP2", DP_CRC_32_BZIP2, 0, 0xFC891918u },
    { "CRC-32C", DP_CRC_32C, 1, 0xE3069283u },
    { "CRC-16/IBM-3740", DP_CRC_16_IBM_3740, 0, 0x29B1u },
    { "CRC-16/XMODEM", DP_CRC_16_XMODEM, 0, 0x31C3u },
    { "CRC-16/KERMIT", DP_CRC_16_KERMIT, 1, 0x2189u },
    { "CRC-8/SMBUS", DP_CRC_8_SMBUS, 0, 0xF4u },
    { "CRC-8/ROHC", DP_CRC_8_ROHC, 1, 0xD0u },
    { "CRC-64/ECMA-182", DP_CRC_64_ECMA_182, 0, 0x6C40DF5F0B497347ull },
  };
  for (size_t c = 0; c < sizeof CAT / sizeof CAT[0]; c++)
    {
      DP_CHECK_MSG (dp_crc_valid (&CAT[c].model), CAT[c].name);
      DP_CHECK_MSG (!CAT[c].model.refin == !CAT[c].refin,
                    "the named model carries the catalogue's refin");
      const uint64_t by = dp_crc_bytes (&CAT[c].model, CHECK, 9);
      DP_CHECK_MSG (by == CAT[c].check, CAT[c].name);
      const size_t nb = to_bits (CHECK, 9, CAT[c].refin, bits);
      DP_CHECK_MSG (dp_crc_bits (&CAT[c].model, bits, nb) == CAT[c].check,
                    CAT[c].name);
    }

  /* ── 2. longer messages, from an independent runtime ───────────────── */
  {
    static const struct
    {
      const char *msg;
      uint32_t    crc32;  /* zlib.crc32 */
      uint16_t    xmodem; /* binascii.crc_hqx (m, 0) */
      uint16_t    ibm;    /* binascii.crc_hqx (m, 0xFFFF) */
    } V[] = {
      { "", 0x00000000u, 0x0000u, 0xFFFFu },
      { "a", 0xE8B7BE43u, 0x7C87u, 0x9D77u },
      { "hello world", 0x0D4A1185u, 0x3BE4u, 0xEFEBu },
      { "The quick brown fox jumps over the lazy dog", 0x414FA339u, 0xF0C8u,
        0x8FDDu },
    };
    for (size_t i = 0; i < sizeof V / sizeof V[0]; i++)
      {
        const uint8_t *m = (const uint8_t *)V[i].msg;
        const size_t   n = strlen (V[i].msg);
        const dp_crc_t a = DP_CRC_32_ISO_HDLC, b = DP_CRC_16_XMODEM,
                       c = DP_CRC_16_IBM_3740;
        DP_CHECK_MSG (dp_crc_bytes (&a, m, n) == V[i].crc32,
                      "CRC-32/ISO-HDLC against zlib");
        DP_CHECK_MSG (dp_crc_bytes (&b, m, n) == V[i].xmodem,
                      "CRC-16/XMODEM against binascii");
        DP_CHECK_MSG (dp_crc_bytes (&c, m, n) == V[i].ibm,
                      "CRC-16/IBM-3740 against binascii");
      }
    /* the 256-octet ramp 0x00..0xFF: zlib.crc32 = 0x29058C73 */
    uint8_t ramp[256];
    for (int i = 0; i < 256; i++)
      ramp[i] = (uint8_t)i;
    const dp_crc_t a = DP_CRC_32_ISO_HDLC;
    DP_CHECK_MSG (dp_crc_bytes (&a, ramp, 256) == 0x29058C73u,
                  "the 256-octet ramp against zlib");
  }

  /* ── 3. properties ────────────────────────────────────────────────── */
  {
    /* CRC-1 with polynomial x + 1 is a parity check: compared with a
       popcount, which shares nothing with the register arithmetic. */
    const dp_crc_t par
        = { .width = 1u, .poly = 1u, .init = 0u, .refout = 0, .xorout = 0u };
    uint32_t st = 20261116u;
    int      ok = 1;
    for (int t = 0; t < 300; t++)
      {
        uint8_t      b[200];
        const size_t n = dp_xs32 (&st) % 200u;
        for (size_t i = 0; i < n; i++)
          b[i] = (uint8_t)(dp_xs32 (&st) & 1u);
        ok &= (int)dp_crc_bits (&par, b, n) == parity (b, n);
      }
    DP_CHECK_MSG (ok, "a CRC-1 is a parity check");

    /* a single flipped bit, and any burst no longer than the register, is
       detected: a property of the generator polynomial, held by every
       model here regardless of how its bits are presented */
    for (size_t c = 0; c < sizeof CAT / sizeof CAT[0]; c++)
      {
        const dp_crc_t *p = &CAT[c].model;
        uint8_t         msg[160];
        for (size_t i = 0; i < sizeof msg; i++)
          msg[i] = (uint8_t)(dp_xs32 (&st) & 1u);
        const uint64_t base = dp_crc_bits (p, msg, sizeof msg);

        int single = 1;
        for (size_t i = 0; i < sizeof msg; i++)
          {
            msg[i] ^= 1u;
            single &= dp_crc_bits (p, msg, sizeof msg) != base;
            msg[i] ^= 1u;
          }
        DP_CHECK_MSG (single, "every single-bit error changes the CRC");

        int burst = 1;
        for (size_t start = 0; start + p->width <= sizeof msg; start += 7)
          for (unsigned len = 2; len <= p->width; len++)
            {
              /* a burst: first and last bit of the span flipped, the
                 middle anything */
              uint8_t cp[160];
              memcpy (cp, msg, sizeof msg);
              cp[start] ^= 1u;
              cp[start + len - 1u] ^= 1u;
              for (unsigned k = 1; k + 1u < len; k++)
                cp[start + k] ^= (uint8_t)(dp_xs32 (&st) & 1u);
              burst &= dp_crc_bits (p, cp, sizeof cp) != base;
            }
        DP_CHECK_MSG (burst, "every burst up to the register width is seen");
      }

    /* refout is a bit-reversal of the result and nothing else */
    {
      const dp_crc_t plain = { .width  = 16u,
                               .poly   = 0x1021u,
                               .init   = 0xFFFFu,
                               .refout = 0,
                               .xorout = 0u };
      const dp_crc_t refl  = { .width  = 16u,
                               .poly   = 0x1021u,
                               .init   = 0xFFFFu,
                               .refout = 1,
                               .xorout = 0u };
      uint8_t        b[64];
      for (size_t i = 0; i < sizeof b; i++)
        b[i] = (uint8_t)(dp_xs32 (&st) & 1u);
      const uint64_t x = dp_crc_bits (&plain, b, sizeof b);
      uint64_t       r = 0;
      for (unsigned i = 0; i < 16u; i++)
        r |= ((x >> i) & 1u) << (15u - i);
      DP_CHECK_MSG (dp_crc_bits (&refl, b, sizeof b) == r,
                    "refout reverses the result's bits");
    }

    /* the bytes entry point and the bit entry point are one loop: the
       same octets, presented as bits in each order, agree */
    {
      uint8_t by[37];
      uint8_t bb[37 * 8];
      for (size_t i = 0; i < sizeof by; i++)
        by[i] = (uint8_t)dp_xs32 (&st);
      for (int refin = 0; refin < 2; refin++)
        {
          dp_crc_t m      = DP_CRC_32_ISO_HDLC;
          m.refin         = refin;
          const size_t nb = to_bits (by, sizeof by, refin, bb);
          DP_CHECK_MSG (dp_crc_bytes (&m, by, sizeof by)
                            == dp_crc_bits (&m, bb, nb),
                        "bytes and bits entry points agree");
        }
    }
  }

  /* ── 4. dp_crc16_ccitt is the arithmetic it always was ─────────────── */
  {
    uint32_t st = 424242u;
    int      ok = 1;
    for (int t = 0; t < 500; t++)
      {
        uint8_t      b[400];
        const size_t n = dp_xs32 (&st) % 400u;
        for (size_t i = 0; i < n; i++)
          b[i] = (uint8_t)(dp_xs32 (&st) & 3u); /* only the LSB counts */
        ok &= dp_crc16_ccitt (b, n) == old_crc16_ccitt (b, n);
      }
    DP_CHECK_MSG (ok, "dp_crc16_ccitt equals the kernel it replaced");
    const size_t nb = to_bits (CHECK, 9, 0, bits);
    DP_CHECK_MSG (dp_crc16_ccitt (bits, nb) == 0x29B1u,
                  "and carries the catalogue's CRC-16/IBM-3740 check value");
  }

  /* ── 5. refusals ──────────────────────────────────────────────────── */
  {
    DP_CHECK (!dp_crc_valid (NULL));
    const dp_crc_t w0
        = { .width = 0u, .poly = 0u, .init = 0u, .refout = 0, .xorout = 0u };
    const dp_crc_t w65
        = { .width = 65u, .poly = 1u, .init = 0u, .refout = 0, .xorout = 0u };
    const dp_crc_t wide = {
      .width = 8u, .poly = 0x107u, .init = 0u, .refout = 0, .xorout = 0u
    }; /* poly needs 9 bits */
    const dp_crc_t wini = {
      .width = 8u, .poly = 0x07u, .init = 0x1FFu, .refout = 0, .xorout = 0u
    }; /* init needs 9 */
    const dp_crc_t wxor = {
      .width = 8u, .poly = 0x07u, .init = 0u, .refout = 0, .xorout = 0x100u
    };
    const dp_crc_t w64 = { .width  = 64u,
                           .poly   = 1u,
                           .init   = ~(uint64_t)0,
                           .refout = 0,
                           .xorout = ~(uint64_t)0 };
    const dp_crc_t w1
        = { .width = 1u, .poly = 1u, .init = 1u, .refout = 1, .xorout = 1u };
    DP_CHECK_MSG (!dp_crc_valid (&w0), "width 0 is not a model");
    DP_CHECK_MSG (!dp_crc_valid (&w65), "width 65 does not fit the register");
    DP_CHECK_MSG (!dp_crc_valid (&wide), "a polynomial wider than the width");
    DP_CHECK_MSG (!dp_crc_valid (&wini), "an init wider than the width");
    DP_CHECK_MSG (!dp_crc_valid (&wxor), "an xorout wider than the width");
    DP_CHECK_MSG (dp_crc_valid (&w64), "width 64 with every bit set is legal");
    DP_CHECK_MSG (dp_crc_valid (&w1), "width 1 is legal");

    /* The header says a polynomial of zero is allowed: it is the degenerate
     * CRC that shifts its register out. Held by derivation: with no
     * polynomial to XOR in, n bits leave the register as init << n (masked),
     * so 0xA5 over four input bits is 0x50 whatever the bits are, and over
     * eight or more it is 0. A control: the same model with a nonzero
     * polynomial is NOT that (its register gets XORed into). */
    const dp_crc_t z = {
      .width = 8u, .poly = 0u, .init = 0xA5u, .refout = 0, .xorout = 0u
    };
    const dp_crc_t nz = {
      .width = 8u, .poly = 0x07u, .init = 0xA5u, .refout = 0, .xorout = 0u
    };
    const uint8_t b4a[4] = { 1, 0, 1, 1 }, b4b[4] = { 0, 0, 0, 0 };
    uint8_t       b8[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    DP_CHECK_MSG (dp_crc_valid (&z), "a polynomial of zero is allowed");
    DP_CHECK (dp_crc_bits (&z, b4a, 4) == 0x50u);
    DP_CHECK (dp_crc_bits (&z, b4b, 4) == 0x50u);
    DP_CHECK (dp_crc_bits (&z, b8, 8) == 0u);
    DP_CHECK_MSG (dp_crc_bits (&nz, b4a, 4) != 0x50u,
                  "control: a nonzero polynomial is not the shift-out CRC");
  }

  DP_TEST_END ("test_dp_crc");
}
