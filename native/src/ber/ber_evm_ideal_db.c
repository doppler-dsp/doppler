/*
 * ber_evm_ideal_db.c — ber module-level function. What the self-referenced
 * EVM meter reads on an ideal stream; the reasoning lives on the declaration
 * in ber/ber_core.h.
 */
#include "doppler/ber/ber_core.h"
#include <math.h>

double
dp_ber_evm_ideal_db (double esn0_db)
{
  /* dp_ber_evm_db() scales the window to unit TOTAL power, signal plus
     noise, so with r = Es/N0 the symbol's own share is r/(1+r) and the
     error against its unit decision is 2 - 2 sqrt(r/(1+r)), not 1/r. */
  const double r = pow (10.0, esn0_db / 10.0);
  return 10.0 * log10 (2.0 - 2.0 * sqrt (r / (1.0 + r)));
}
