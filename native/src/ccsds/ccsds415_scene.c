/*
 * ccsds415_scene.c — ccsds module-level function.
 */
#include "doppler/ccsds/ccsds_core.h"

#include "doppler/ccsds/ccsds_415.h"

/* The Python face of the 415.1-B-1 scene helper: a thin alias over the
   point's own function, which owns the arithmetic and computes it with
   fast math off, as dp_ccsds415_chip_rate() is over
   dp_ccsds_415_chip_rate(). */
dp_ccsds_415_scene_t
dp_ccsds415_scene (double fc, double spc, double start_hz)
{
  return dp_ccsds_415_scene (fc, spc, start_hz);
}
