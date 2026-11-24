/*
 * ccsds415_cn0.c — ccsds module-level function.
 */
#include "doppler/ccsds/ccsds_core.h"

#include "doppler/ccsds/ccsds_415.h"

/* The Python face of the 415.1-B-1 C/N0: a thin alias over the point's
   own function (dp_ccsds_415_cn0()). */
double
dp_ccsds415_cn0 (double esn0_db)
{
  return dp_ccsds_415_cn0 (esn0_db);
}
