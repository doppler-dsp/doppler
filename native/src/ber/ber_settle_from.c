/*
 * ber_settle_from.c — ber module-level function. The window policy lives here
 * once; see ber/ber_core.h for why each term is in the max and what measuring
 * inside the window costs.
 */
#include "ber/ber_core.h"

size_t
ber_settle_from (size_t budget, int timing_lock, int carrier_lock)
{
  size_t out = budget;

  if (timing_lock > 0 && (size_t)timing_lock > out)
    out = (size_t)timing_lock;
  if (carrier_lock > 0 && (size_t)carrier_lock > out)
    out = (size_t)carrier_lock;
  return out;
}
