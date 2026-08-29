/*
 * bin_to_int.c — cvt module-level function.
 */
#include "cvt/cvt_core.h"

/* Returns the value rather than a status, because that is the shape a
   binding can carry. 0 is therefore both "the value zero" and "refused" --
   acceptable only because every refusal here is a programming error in the
   WIDTH the caller chose (0, or over 64) or the bit order it named, never a
   property of the data. Validate the width, then trust the result. */
uint64_t
bin_to_int (const uint8_t *bits, size_t bits_len, int bitorder)
{
  if (!bits || bits_len == 0u || bits_len > 64u)
    return 0;
  if (bitorder != DP_BITORDER_BIG && bitorder != DP_BITORDER_LITTLE)
    return 0;

  uint64_t v = 0;
  for (size_t done = 0; done < bits_len;)
    {
      const size_t w = cvt_unit_width (done, bits_len);
      for (size_t i = 0; i < w; i++)
        v = (v << 1) | (bits[done + cvt_bit_slot (i, w, bitorder)] ? 1u : 0u);
      done += w;
    }
  return v;
}
