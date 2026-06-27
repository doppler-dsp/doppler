/*
 * mpsk_diff_map.c — mpsk module-level function.
 *
 * Differential M-PSK map: each Gray label is a phase INCREMENT, so the running
 * constellation index accumulates it (from an implicit zero-phase reference).
 * Sequential over the array; the receiver (mpsk_diff_demap) recovers the
 * labels from phase differences, so an unknown constant carrier phase cancels.
 */
#include "mpsk/mpsk_core.h"

void
mpsk_diff_map (const uint8_t *sym, size_t sym_len, float complex *out, int m)
{
  unsigned mask = (unsigned)(m - 1);
  unsigned acc  = 0; /* running index; implicit zero-phase start */
  for (size_t i = 0; i < sym_len; i++)
    {
      acc = (acc + mpsk_gray_decode (sym[i] & mask)) & mask;
      /* point AT index acc: feed gray_encode(acc) to the (label->point) map.
       */
      out[i] = mpsk_constellation (mpsk_gray_encode (acc), m);
    }
}
