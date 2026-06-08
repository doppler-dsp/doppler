/*
 * mul_q8.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
mul_q8 (const int8_t *a, size_t a_len, const int8_t *b, size_t b_len,
        int8_t *out)
{
  size_t n = a_len < b_len ? a_len : b_len;
  for (size_t i = 0; i < n; i++)
    {
      int16_t p = (int16_t)((int)a[i] * (int)b[i] + 64);
      out[i]    = sat8 (p >> 7);
    }
}
