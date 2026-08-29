/*
 * hex_to_bin.c — cvt module-level function.
 */
#include "cvt/cvt_core.h"

#include <string.h>

/* One hex digit -> 0..15, or -1. Rejecting rather than skipping is the whole
   contract: a marker that silently shortens because of a typo is exactly the
   failure this exists to prevent. */
static int
hex_digit (char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  return -1;
}

size_t
hex_to_bin (const char *hex, uint8_t *out, size_t out_len, int bitorder)
{
  if (!hex || !out)
    return 0;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return 0;

  const size_t n_digits = strlen (hex);
  if (n_digits == 0 || 4u * n_digits > out_len)
    return 0;

  /* Validate the WHOLE string before writing a bit, so a refusal leaves
     `out` untouched rather than half-expanded. */
  for (size_t d = 0; d < n_digits; d++)
    if (hex_digit (hex[d]) < 0)
      return 0;

  /* Whole bytes are two digits; an odd final digit is a 4-bit unit. */
  for (size_t d = 0; d < n_digits; d += 2u)
    {
      const size_t pair  = (d + 1u < n_digits) ? 2u : 1u;
      const size_t width = 4u * pair;
      unsigned     v     = 0;
      for (size_t k = 0; k < pair; k++)
        v = (v << 4) | (unsigned)hex_digit (hex[d + k]);

      for (size_t i = 0; i < width; i++)
        {
          const unsigned bit = (v >> (width - 1u - i)) & 1u;
          out[4u * d + cvt_bit_slot (i, width, bitorder)] = (uint8_t)bit;
        }
    }
  return 4u * n_digits;
}
