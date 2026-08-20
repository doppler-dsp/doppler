/*
 * ccsds_asm_bits.c — wfm module-level function.
 */
#include "wfm/wfm_core.h"

#include "ccsds_tm/ccsds_tm.h"

/* The CCSDS Attached Sync Marker as bits Python can search for -- a thin
 * public alias over ccsds_tm's own expansion of the constant, for the reason
 * that expansion exists: an MSB-first transcription written out twice is one
 * that can disagree with itself, and a receiver that disagrees with the
 * assembler about the marker syncs to nothing. Python transcribing it a
 * third time was the state of this tree until doppler#900. */
void
ccsds_asm_bits (uint8_t *out)
{
  ccsds_tm_asm_bits (out);
}
