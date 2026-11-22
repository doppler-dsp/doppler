/**
 * crc_demo.c -- any CRC from its model, over octets and over a bit stream.
 *
 * A CRC is a model (width, polynomial, initial register, output reflection,
 * output XOR, input reflection). dp_crc.h holds the one loop and nine
 * catalogue instances. This computes each instance's
 * catalogue check value (the CRC of the nine ASCII characters 123456789) two
 * ways, once from the octets, in the bit order the model's `refin` asks for,
 * and once from the unpacked bit stream the frame paths pass around, and shows
 * what a flipped `refin` does: a different CRC, with no error.
 *
 * Shows:
 *   1. The nine named instances against their catalogue check values, from
 *      octets (dp_crc_bytes) and from bits (dp_crc_bits)
 *   2. A flipped `refin`: CRC-32/ISO-HDLC gives 1898913F, not CBF43926
 *   3. A model of your own, and dp_crc_valid() refusing one the header
 *      cannot compute
 *
 * Build and run:
 *   make build
 *   ./build/native/examples/crc_demo
 */
#include "doppler/dp_crc.h"
#include <stdio.h>
#include <string.h>

/* The nine octets' bits, in the order `refin` asks for, one per byte. */
static size_t
to_bits (const uint8_t *in, size_t n, int refin, uint8_t *out)
{
  size_t k = 0;
  for (size_t i = 0; i < n; i++)
    for (unsigned b = 0; b < 8u; b++)
      out[k++]
          = (uint8_t)(refin ? (in[i] >> b) & 1u : (in[i] >> (7u - b)) & 1u);
  return k;
}

int
main (void)
{
  printf ("=== doppler CRC demo ===\n\n");
  int ok = 1;

  static const uint8_t digits[9]
      = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
  static const struct
  {
    const char *name;
    dp_crc_t    model;
    uint64_t    check;
  } CAT[] = {
    { "CRC-32/ISO-HDLC", DP_CRC_32_ISO_HDLC, 0xCBF43926u },
    { "CRC-32/BZIP2", DP_CRC_32_BZIP2, 0xFC891918u },
    { "CRC-32C", DP_CRC_32C, 0xE3069283u },
    { "CRC-16/IBM-3740", DP_CRC_16_IBM_3740, 0x29B1u },
    { "CRC-16/XMODEM", DP_CRC_16_XMODEM, 0x31C3u },
    { "CRC-16/KERMIT", DP_CRC_16_KERMIT, 0x2189u },
    { "CRC-8/SMBUS", DP_CRC_8_SMBUS, 0xF4u },
    { "CRC-8/ROHC", DP_CRC_8_ROHC, 0xD0u },
    { "CRC-64/ECMA-182", DP_CRC_64_ECMA_182, 0x6C40DF5F0B497347ull },
  };

  /* 1. From octets, and from the bit stream the same octets make. */
  printf ("--- 1. the nine catalogue check values (CRC of \"123456789\")\n");
  for (size_t c = 0; c < sizeof CAT / sizeof CAT[0]; c++)
    {
      uint8_t        bits[8 * 9];
      const uint64_t by = dp_crc_bytes (&CAT[c].model, digits, 9);
      const size_t   nb = to_bits (digits, 9, CAT[c].model.refin, bits);
      const uint64_t bi = dp_crc_bits (&CAT[c].model, bits, nb);
      printf ("    %-16s refin %d  bytes %0*llX  bits %0*llX  check %0*llX\n",
              CAT[c].name, CAT[c].model.refin,
              (int)(CAT[c].model.width + 3u) / 4, (unsigned long long)by,
              (int)(CAT[c].model.width + 3u) / 4, (unsigned long long)bi,
              (int)(CAT[c].model.width + 3u) / 4,
              (unsigned long long)CAT[c].check);
      ok &= (by == CAT[c].check && bi == CAT[c].check);
    }

  /* 2. refin is part of the model: flip it and the CRC is another CRC, with
     no error to say so. */
  dp_crc_t       iso   = DP_CRC_32_ISO_HDLC;
  const uint64_t right = dp_crc_bytes (&iso, digits, 9);
  iso.refin            = 0;
  const uint64_t wrong = dp_crc_bytes (&iso, digits, 9);
  printf ("\n--- 2. refin 1 gives %08llX, flipped to 0 it gives %08llX: no "
          "error\n",
          (unsigned long long)right, (unsigned long long)wrong);
  ok &= (right == 0xCBF43926u && wrong == 0x1898913Fu);

  /* 3. A model of your own: CRC-8 with polynomial 0x31, reflected, and
     one the header refuses (a polynomial wider than its register). */
  const dp_crc_t mine = { .width  = 8u,
                          .poly   = 0x31u,
                          .init   = 0u,
                          .refout = 1,
                          .xorout = 0u,
                          .refin  = 1 };
  const dp_crc_t wide
      = { .width = 8u, .poly = 0x131u, .init = 0u, .refout = 0, .xorout = 0u };
  printf ("\n--- 3. a model of your own: valid %d (0x31 in 8 bits), "
          "a 9-bit polynomial in 8 bits: valid %d\n",
          dp_crc_valid (&mine), dp_crc_valid (&wide));
  ok &= (dp_crc_valid (&mine) == 1 && dp_crc_valid (&wide) == 0);

  printf ("\ncrc_demo: %s\n", ok ? "OK" : "FAILED");
  return ok ? 0 : 1;
}
