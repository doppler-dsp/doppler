/*
 * qam_demap.c — mpsk module-level function.
 *
 * Element-wise hard decision: each symbol -> nearest constellation point's
 * label. Inverse of qam_map; uses the mpsk_core.h inline slicer.
 */
#include "doppler/mpsk/mpsk_core.h"

void
dp_qam_demap (const float _Complex *x, size_t x_len, uint16_t *out, int m)
{
  float _Complex ahat;
  for (size_t i = 0; i < x_len; i++)
    out[i] = (uint16_t)qam_slice (x[i], m, &ahat);
}
