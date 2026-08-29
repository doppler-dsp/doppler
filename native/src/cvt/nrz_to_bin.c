/*
 * nrz_to_bin.c — cvt module-level function.
 */
#include "cvt/cvt_core.h"

/* The hard decision that inverts bin_to_nrz.
 *
 * Zero is a 0, not a coin toss. The mapping has to be TOTAL for a round trip
 * to be exact, and an exact zero is what a punctured or erased symbol
 * arrives as -- deciding it consistently is worth more than deciding it
 * cleverly, because a caller that wants erasures handled properly wants a
 * soft demapper, not this.
 */
size_t
nrz_to_bin (const float *nrz, size_t nrz_len, uint8_t *out, size_t out_len)
{
  if (!nrz || !out || nrz_len == 0u || nrz_len > out_len)
    return 0;

  for (size_t i = 0; i < nrz_len; i++)
    out[i] = (nrz[i] < 0.0f) ? 1u : 0u;
  return nrz_len;
}
