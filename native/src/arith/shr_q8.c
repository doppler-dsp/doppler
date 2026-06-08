/*
 * shr_q8.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shr_q8 (const int8_t *a, size_t a_len, int8_t *out, int n)
{
  for (size_t i = 0; i < a_len; i++)
    {
      if (n <= 0)
        {
          out[i] = a[i];
        }
      else
        {
          int16_t v = (int16_t)a[i];
          if (n < 8)
            v += (int16_t)(1 << (n - 1));
          out[i] = (int8_t)(v >> (n < 15 ? n : 15));
        }
    }
}
