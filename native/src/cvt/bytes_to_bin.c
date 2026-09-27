/*
 * bytes_to_bin.c — cvt module-level function: packed octets to bits.
 */
#include "doppler/cvt/cvt_core.h"

/* Eight bits an octet, placed through cvt_bit_slot like every other
   conversion here, so the bit order cannot mean one thing for a hex literal
   and another for a file. */
size_t
dp_bytes_to_bin (const uint8_t *octets, size_t octets_len, uint8_t *out,
                 size_t out_len, int bitorder)
{
  if (!octets || !out || octets_len == 0u || octets_len > out_len / 8u)
    return 0;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return 0;

  for (size_t k = 0; k < octets_len; k++)
    for (size_t i = 0; i < 8u; i++)
      out[8u * k + cvt_bit_slot (i, 8u, bitorder)]
          = (uint8_t)((octets[k] >> (7u - i)) & 1u);
  return 8u * octets_len;
}
