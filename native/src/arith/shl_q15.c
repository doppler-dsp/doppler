/*
 * shl_q15.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shl_q15 (const int16_t *a, size_t a_len, int16_t *out, int n)
{
  for (size_t i = 0; i < a_len; i++)
    {
      if (n <= 0)
        out[i] = a[i];
      else if (n >= 16)
        out[i] = a[i] > 0 ? 32767 : a[i] < 0 ? -32768 : 0;
      else
        /* MULTIPLY, never `<< n`: a left shift of a negative value is
           undefined in C99, and half of every Q15 input is negative.
           n is in [1, 15] here, so the factor is at most 32768 and the
           product at most 2^30 -- exactly representable in int32_t, so
           this is the same arithmetic with none of the undefinedness. */
        out[i] = sat16 ((int32_t)a[i] * (int32_t)(1 << n));
    }
}
