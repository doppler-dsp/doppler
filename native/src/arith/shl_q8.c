/*
 * shl_q8.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shl_q8 (const int8_t *a, size_t a_len, int8_t *out, int n)
{
  for (size_t i = 0; i < a_len; i++)
    {
      if (n <= 0)
        out[i] = a[i];
      else if (n >= 8)
        out[i] = a[i] > 0 ? 127 : a[i] < 0 ? -128 : 0;
      else
        /* Multiply rather than shift, for the reason shl_q15.c gives:
           `<< n` on a negative value is undefined. n is in [1, 7], so
           the product is at most 128 * 128 and fits int16_t. */
        out[i] = sat8 ((int16_t)(a[i] * (int16_t)(1 << n)));
    }
}
