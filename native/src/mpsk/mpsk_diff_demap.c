/*
 * mpsk_diff_demap.c — mpsk module-level function.
 *
 * Differential M-PSK demap: decide each label from the phase DIFFERENCE
 * between consecutive sliced indices (first references an implicit zero-phase
 * start). Inverse of mpsk_diff_map; invariant to an unknown constant carrier
 * phase.
 */
#include "mpsk/mpsk_core.h"

void
mpsk_diff_demap (const float complex *x, size_t x_len, uint8_t *out, int m)
{
  float complex ahat;
  unsigned      mask   = (unsigned)(m - 1);
  unsigned      prev_k = 0; /* implicit zero-phase reference */
  for (size_t i = 0; i < x_len; i++)
    {
      unsigned k = mpsk_gray_decode (mpsk_slice (x[i], m, &ahat));
      unsigned d = (k - prev_k) & mask; /* phase-difference index */
      out[i]     = (uint8_t)mpsk_gray_encode (d);
      prev_k     = k;
    }
}
