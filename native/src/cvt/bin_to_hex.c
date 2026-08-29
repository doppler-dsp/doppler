/*
 * bin_to_hex.c — cvt module-level function.
 */
#include "cvt/cvt_core.h"

size_t
bin_to_hex (const uint8_t *bits, size_t bits_len, uint8_t *out, size_t out_len,
            int bitorder)
{
  if (!bits || !out || bits_len == 0u || (bits_len & 3u) != 0)
    return 0;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return 0;

  const size_t n_digits = bits_len / 4u;
  if (n_digits + 1u > out_len)
    return 0;

  static const char DIGITS[] = "0123456789abcdef";
  for (size_t d = 0; d < n_digits; d += 2u)
    {
      const size_t pair  = (d + 1u < n_digits) ? 2u : 1u;
      const size_t width = 4u * pair;
      unsigned     v     = 0;
      for (size_t i = 0; i < width; i++)
        {
          const unsigned bit
              = bits[4u * d + cvt_bit_slot (i, width, bitorder)] ? 1u : 0u;
          v = (v << 1) | bit;
        }
      for (size_t k = 0; k < pair; k++)
        out[d + k] = (uint8_t)DIGITS[(v >> (4u * (pair - 1u - k))) & 0xFu];
    }
  out[n_digits] = '\0';
  return n_digits;
}
