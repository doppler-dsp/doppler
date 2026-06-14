/*
 * measure_rec_nfft.c — recommended zero-padded length: next_pow2(n * pad).
 */
#include "measure/measure_core.h"

size_t
measure_rec_nfft (size_t n, size_t pad)
{
  if (pad < 1)
    pad = 1;
  size_t need = n * pad;
  size_t p    = 1;
  while (p < need)
    p <<= 1;
  return p;
}
