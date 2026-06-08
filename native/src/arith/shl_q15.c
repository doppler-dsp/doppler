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
        out[i] = sat16 ((int32_t)a[i] << n);
    }
}
