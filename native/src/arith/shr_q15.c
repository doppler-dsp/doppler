/*
 * shr_q15.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shr_q15 (const int16_t *a, size_t a_len, int16_t *out, int n)
{
  for (size_t i = 0; i < a_len; i++)
    {
      if (n <= 0)
        {
          out[i] = a[i];
        }
      else
        {
          int32_t v = (int32_t)a[i];
          if (n < 16)
            v += (int32_t)(1 << (n - 1));
          out[i] = (int16_t)(v >> (n < 31 ? n : 31));
        }
    }
}
