/*
 * ccsds415_chip_rate.c — ccsds module-level function.
 */
#include "doppler/ccsds/ccsds_core.h"

#include "doppler/ccsds/ccsds_415.h"

/* The Python face of the 415.1-B-1 chip rate: a thin alias over the
   point's own function, which owns the arithmetic and computes it with
   fast math off, as dp_asm_bits() is over dp_ccsds_tm_asm_bits(). */
double
dp_ccsds415_chip_rate (double fc)
{
  return dp_ccsds_415_chip_rate (fc);
}
