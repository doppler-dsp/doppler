/*
 * qam_bits_per_symbol.c — mpsk module-level function.
 *
 * Thin Python-facing wrapper over the mpsk_core.h inline qam_bps().
 */
#include "doppler/mpsk/mpsk_core.h"

int
dp_qam_bits_per_symbol (int m)
{
  return qam_bps (m);
}
