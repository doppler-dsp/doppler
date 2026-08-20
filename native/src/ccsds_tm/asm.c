/*
 * asm.c — the Attached Sync Marker (131.0-B-3, sections 9.3 and 9.4).
 *
 * Its own translation unit, small as it is, for the reason the rest of this
 * directory is split per stage: a receiver that only wants to correlate
 * against the marker should not have to link an R-S encoder to get it.
 */
#include "ccsds_tm/ccsds_tm.h"

void
ccsds_tm_asm_bits (uint8_t *out)
{
  /* Figure 9-1 draws the pattern with "FIRST TRANSMITTED BIT (Bit 0)" over
     the leading 0001 of 0x1A, so bit 0 of the marker is bit 31 of the
     constant. Shifting down from the top is that statement written once. */
  for (unsigned i = 0; i < CCSDS_TM_ASM_BITS; i++)
    out[i] = (uint8_t)((CCSDS_TM_ASM >> (CCSDS_TM_ASM_BITS - 1u - i)) & 1u);
}

int
ccsds_tm_asm_find (const uint8_t *bits, size_t n_bits, unsigned max_errors,
                   ccsds_tm_asm_hit_t *hit)
{
  /* The pattern comes from ccsds_tm_asm_bits rather than from a second
     expansion of the constant, for the reason that function exists: an
     MSB-first expansion written out twice is a transcription that can
     disagree with itself, and a receiver that disagrees with the assembler
     about the marker syncs to nothing. */
  uint8_t marker[CCSDS_TM_ASM_BITS];
  ccsds_tm_asm_bits (marker);

  /* And the SEARCH comes from dp_syncword.h for the matching reason one
     level up: correlating a known pattern against a bit stream in both
     polarities is not CCSDS's, it is what every framing with a sync word
     does. This function is the standard's pick of pattern, exactly as
     CCSDS_TM_CONV is its pick of polynomials. */
  return dp_syncword_find (bits, n_bits, marker, CCSDS_TM_ASM_BITS, max_errors,
                           hit);
}
