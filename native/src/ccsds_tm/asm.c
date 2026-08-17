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
    out[i] = (uint8_t)((FEC_CCSDS_ASM >> (CCSDS_TM_ASM_BITS - 1u - i)) & 1u);
}

int
ccsds_tm_asm_find (const uint8_t *bits, size_t n_bits, unsigned max_errors,
                   ccsds_tm_asm_hit_t *hit)
{
  if (n_bits < (size_t)CCSDS_TM_ASM_BITS)
    return 0;

  /* The pattern comes from ccsds_tm_asm_bits rather than from a second
     expansion of the constant, for the reason that function exists: an
     MSB-first expansion written out twice is a transcription that can
     disagree with itself, and a receiver that disagrees with the assembler
     about the marker syncs to nothing. */
  uint8_t marker[CCSDS_TM_ASM_BITS];
  ccsds_tm_asm_bits (marker);

  const size_t last = n_bits - (size_t)CCSDS_TM_ASM_BITS;
  for (size_t off = 0; off <= last; off++)
    {
      /* Both polarities in one pass: a bit that disagrees with the marker
         agrees with its complement, so the two distances sum to the marker
         length and one comparison yields both. */
      unsigned d = 0;
      for (unsigned i = 0; i < CCSDS_TM_ASM_BITS; i++)
        d += (unsigned)((bits[off + i] & 1u) ^ marker[i]);

      const unsigned dinv = (unsigned)CCSDS_TM_ASM_BITS - d;
      if (d <= max_errors || dinv <= max_errors)
        {
          const int inv = dinv < d;
          hit->offset   = off;
          hit->inverted = inv;
          hit->errors   = inv ? dinv : d;
          return 1;
        }
    }
  return 0;
}
