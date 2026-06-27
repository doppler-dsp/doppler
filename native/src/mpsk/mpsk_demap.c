/*
 * mpsk_demap.c — mpsk module-level function.
 *
 * Element-wise hard decision: each symbol -> nearest constellation point's
 * Gray label byte. Inverse of mpsk_map; uses the mpsk_core.h inline slicer.
 */
#include "mpsk/mpsk_core.h"

void
mpsk_demap (const float complex *x, size_t x_len, uint8_t *out, int m)
{
  float complex ahat;
  for (size_t i = 0; i < x_len; i++)
    out[i] = (uint8_t)mpsk_slice (x[i], m, &ahat);
}
