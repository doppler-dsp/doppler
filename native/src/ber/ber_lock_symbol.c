/*
 * ber_lock_symbol.c — ber module-level function. Implementation notes and
 * the measured consequences of getting each one wrong live on the
 * declaration in ber/ber_core.h.
 */
#include "ber/ber_core.h"
#include "mpsk/mpsk_core.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int
ber_lock_symbol (const uint8_t *flags, size_t flags_len, size_t sustain,
                 double min_frac)
{
  size_t total = 0, before = 0, z = 0;
  if (!flags || flags_len == 0)
    return -1;
  if (sustain == 0)
    sustain = 200;
  if (!(min_frac > 0.0))
    min_frac = 0.9;
  for (size_t j = 0; j < flags_len; j++)
    total += flags[j] ? 1u : 0u;
  if (!total)
    return -1;
  for (size_t i = 0; i < flags_len; i++)
    {
      if (z < i)
        z = i;
      while (z < flags_len && flags[z])
        z++;
      if (flags_len - i < sustain)
        return -1; /* not enough record left to judge */
      if (z - i >= sustain
          && (double)(total - before) / (double)(flags_len - i) >= min_frac)
        return (long)i;
      before += flags[i] ? 1u : 0u;
    }
  return -1;
}
