/*
 * ccsds_asm.c — the Attached Sync Marker (131.0-B-3, sections 9.3 and 9.4).
 *
 * Its own translation unit, small as it is, for the reason the rest of this
 * directory is split per stage: a receiver that only wants to correlate
 * against the marker should not have to link an R-S encoder to get it.
 */
#include "fec/fec_ccsds.h"

void
fec_ccsds_asm_bits (uint8_t *out)
{
  /* Figure 9-1 draws the pattern with "FIRST TRANSMITTED BIT (Bit 0)" over
     the leading 0001 of 0x1A, so bit 0 of the marker is bit 31 of the
     constant. Shifting down from the top is that statement written once. */
  for (unsigned i = 0; i < FEC_CCSDS_ASM_BITS; i++)
    out[i] = (uint8_t)((FEC_CCSDS_ASM >> (FEC_CCSDS_ASM_BITS - 1u - i)) & 1u);
}
