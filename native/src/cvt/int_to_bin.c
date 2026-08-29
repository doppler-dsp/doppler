/*
 * int_to_bin.c — cvt module-level function.
 */
#include "cvt/cvt_core.h"

/* Where the i-th bit of a unit lands, given its width and the bit order.
   BIG is the identity — the order the value reads. LITTLE reverses within
   the unit, which for a trailing half-byte is its own four bits.

   Shared with hex_to_bin through cvt_bit_slot/cvt_unit_width so the two
   cannot disagree about where a short final unit begins. That is the only
   place they could drift, and it is the place a marker would then be
   expanded two ways. */
size_t
int_to_bin (uint64_t v, uint32_t n_bits, uint8_t *out, size_t out_len,
            int bitorder)
{
  if (!out || n_bits == 0u || n_bits > 64u || n_bits > out_len)
    return 0;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return 0;

  for (size_t done = 0; done < n_bits;)
    {
      const size_t w = cvt_unit_width (done, n_bits);
      for (size_t i = 0; i < w; i++)
        {
          /* Bit 0 of the whole value is its most significant, so the shift
             counts down from n_bits regardless of where the unit starts. */
          const unsigned bit
              = (unsigned)((v >> (n_bits - 1u - (done + i))) & 1u);
          out[done + cvt_bit_slot (i, w, bitorder)] = (uint8_t)bit;
        }
      done += w;
    }
  return n_bits;
}
