/*
 * qam_map.c — mpsk module-level function (QAM beside the PSK maps).
 *
 * Element-wise label -> constellation point. The mapping lives in the
 * mpsk_core.h inline helper qam_constellation(); this is the loop.
 */
#include "doppler/mpsk/mpsk_core.h"

void
dp_qam_map (const uint16_t *sym, size_t sym_len, float _Complex *out, int m)
{
  for (size_t i = 0; i < sym_len; i++)
    out[i] = qam_constellation (sym[i], m);
}
