/*
 * ber_settle_from.c — ber module-level function. The window policy lives here
 * once; see ber/ber_core.h for why each term is in the max and what measuring
 * inside the window costs.
 */
#include "ber/ber_core.h"

size_t
ber_settle_from (size_t budget, int timing_lock, int carrier_lock,
                 int handover)
{
  size_t out = budget;

  if (timing_lock > 0 && (size_t)timing_lock > out)
    out = (size_t)timing_lock;
  if (carrier_lock > 0 && (size_t)carrier_lock > out)
    out = (size_t)carrier_lock;
  /* A handover fires AFTER every lock indicator and starts its own transient,
     so it contributes its instant plus the budget again. A -1 here is not a
     failure: a pure-NDA receiver never publishes one. */
  if (handover >= 0 && (size_t)handover + budget > out)
    out = (size_t)handover + budget;
  return out;
}
