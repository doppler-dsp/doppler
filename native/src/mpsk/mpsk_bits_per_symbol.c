/*
 * mpsk_bits_per_symbol.c — mpsk module-level function.
 *
 * Thin Python-facing wrapper over the mpsk_core.h inline mpsk_bps(): log2(M).
 */
#include "mpsk/mpsk_core.h"

int
mpsk_bits_per_symbol (int m)
{
  return mpsk_bps (m);
}
