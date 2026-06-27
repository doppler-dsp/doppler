/*
 * mpsk_map.c — mpsk module-level function.
 *
 * Element-wise Gray-label -> constellation-point map (the transmit inverse of
 * mpsk_demap). Logic lives in the mpsk_core.h inline helpers; this is the
 * loop.
 */
#include "mpsk/mpsk_core.h"

void
mpsk_map (const uint8_t *sym, size_t sym_len, float complex *out, int m)
{
  for (size_t i = 0; i < sym_len; i++)
    out[i] = mpsk_constellation (sym[i], m);
}
