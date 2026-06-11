/*
 * bpsk_map.c — wfmgen module-level function.
 */
#include "wfm/wfm_core.h"

/* Map bits {0,1} to BPSK symbols: 0 -> +1, 1 -> -1 (unit energy, cf32). */
void
bpsk_map (const uint8_t *bits, size_t bits_len, float complex *out)
{
  for (size_t i = 0; i < bits_len; i++)
    out[i] = (bits[i] & 1u) ? (-1.0f + 0.0f * I) : (1.0f + 0.0f * I);
}
