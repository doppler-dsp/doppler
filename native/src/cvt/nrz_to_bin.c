/*
 * nrz_to_bin.c — cvt module-level function.
 */
#include "doppler/cvt/cvt_core.h"
#include "doppler/mpsk/mpsk_core.h" /* mpsk_bpsk_bit -- the ONE polarity */

/* The hard decision that inverts bin_to_nrz.
 *
 * Zero is a 0, not a coin toss. The mapping has to be TOTAL for a round trip
 * to be exact, and an exact zero is what a punctured or erased symbol
 * arrives as -- deciding it consistently is worth more than deciding it
 * cleverly, because a caller that wants erasures handled properly wants a
 * soft demapper, not this.
 */
size_t
dp_nrz_to_bin (const float *nrz, size_t nrz_len, uint8_t *out, size_t out_len)
{
  if (!nrz || !out || nrz_len == 0u || nrz_len > out_len)
    return 0;

  for (size_t i = 0; i < nrz_len; i++)
    out[i] = mpsk_bpsk_bit (nrz[i]);
  return nrz_len;
}
