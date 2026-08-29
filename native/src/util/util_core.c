/*
 * util_core.c — Util module implementation.
 *
 * Most of util's public functions are header-only (JM_FORCEINLINE in
 * util_core.h) so every caller inlines them.  The bit/hex pair below is
 * the exception and is defined here ONCE: they walk arrays rather than
 * computing a scalar, so the inline-plus-external-definition shape the
 * scalar helpers use (see square_clip.c) would mean two copies of a loop
 * that can drift apart.  One definition, external linkage, no inline twin.
 */
#include "util/util_core.h"

#include <string.h>

/* One hex digit -> 0..15, or -1. Rejecting rather than skipping is the
   whole contract: a marker that silently shortens because of a typo is
   exactly the failure hex_to_bin exists to prevent. */
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

/* Where the i-th bit of a unit lands, given its width and the bit order.
   BIG is the identity — the order the literal is read. LITTLE reverses
   within the unit, which for a trailing half-byte is its own four bits. */
static size_t
bit_slot (size_t i, size_t width, int bitorder)
{
  return (bitorder == DP_BITORDER_BIG) ? i : (width - 1u - i);
}

/* The unit both directions walk in: 8 bits from the start, then whatever
   is left. Written once so int_to_bin and hex_to_bin cannot disagree about
   where a short final unit begins -- which is the only place the two could
   drift, and the place a marker would then be expanded two ways. */
static size_t
unit_width (size_t done, size_t total)
{
  const size_t left = total - done;
  return (left >= 8u) ? 8u : left;
}

size_t
int_to_bin (uint64_t v, unsigned n_bits, uint8_t *out, size_t max_out,
            int bitorder)
{
  if (!out || n_bits == 0u || n_bits > 64u || n_bits > max_out)
    return 0;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return 0;

  for (size_t done = 0; done < n_bits;)
    {
      const size_t w = unit_width (done, n_bits);
      for (size_t i = 0; i < w; i++)
        {
          /* Bit 0 of the whole value is its most significant, so the shift
             counts down from n_bits regardless of where the unit starts. */
          const unsigned bit
              = (unsigned)((v >> (n_bits - 1u - (done + i))) & 1u);
          out[done + bit_slot (i, w, bitorder)] = (uint8_t)bit;
        }
      done += w;
    }
  return n_bits;
}

int
bin_to_int (const uint8_t *bits, size_t n_bits, uint64_t *out, int bitorder)
{
  if (!bits || !out || n_bits == 0u || n_bits > 64u)
    return -1;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return -1;

  uint64_t v = 0;
  for (size_t done = 0; done < n_bits;)
    {
      const size_t w = unit_width (done, n_bits);
      for (size_t i = 0; i < w; i++)
        v = (v << 1) | (bits[done + bit_slot (i, w, bitorder)] ? 1u : 0u);
      done += w;
    }
  *out = v;
  return 0;
}

size_t
hex_to_bin (const char *hex, uint8_t *out, size_t max_out, int bitorder)
{
  if (!hex || !out)
    return 0;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return 0;

  const size_t n_digits = strlen (hex);
  if (n_digits == 0 || 4u * n_digits > max_out)
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
          out[4u * d + bit_slot (i, width, bitorder)] = (uint8_t)bit;
        }
    }
  return 4u * n_digits;
}

size_t
bin_to_hex (const uint8_t *bits, size_t n_bits, char *out, size_t max_out,
            int bitorder)
{
  if (!bits || !out || n_bits == 0 || (n_bits & 3u) != 0)
    return 0;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return 0;

  const size_t n_digits = n_bits / 4u;
  if (n_digits + 1u > max_out)
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
              = bits[4u * d + bit_slot (i, width, bitorder)] ? 1u : 0u;
          v = (v << 1) | bit;
        }
      for (size_t k = 0; k < pair; k++)
        out[d + k] = DIGITS[(v >> (4u * (pair - 1u - k))) & 0xFu];
    }
  out[n_digits] = '\0';
  return n_digits;
}
