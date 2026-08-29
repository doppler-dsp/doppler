/*
 * bin_to_nrz.c — cvt module-level function.
 */
#include "cvt/cvt_core.h"

/* `1 - 2*b`, computed directly rather than through mpsk_constellation().
 *
 * The two must agree, and the sign convention's HOME is mpsk_core.h: BPSK is
 * M-PSK at m = 2, where phi0 is 0, so label 0 lands at +1 and label 1 at -1.
 * This is the same statement in the form a per-bit loop can afford -- the
 * constellation call computes a cos and a sin per symbol to return a value
 * that is always +-1 here.
 *
 * Restating a convention is exactly how two mappers drift, so the agreement
 * is not left to this comment: test_cvt_core asserts, for both bits, that
 * this equals crealf(mpsk_constellation(b, 2)). A mapper that disagreed with
 * the receiver's would decode every bit inverted while looking locked.
 */
size_t
bin_to_nrz (const uint8_t *bits, size_t bits_len, float *out, size_t out_len)
{
  if (!bits || !out || bits_len == 0u || bits_len > out_len)
    return 0;

  for (size_t i = 0; i < bits_len; i++)
    out[i] = bits[i] ? -1.0f : 1.0f; /* any non-zero byte is a set bit */
  return bits_len;
}
