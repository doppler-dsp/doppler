/*
 * dot_q8.c — arith module-level function.
 */
#include "arith/arith_core.h"

int32_t
dot_q8 (const int8_t *a, size_t a_len, const int8_t *b, size_t b_len)
{
  size_t  n   = a_len < b_len ? a_len : b_len;
  int32_t acc = 0;
  for (size_t i = 0; i < n; i++)
    acc += (int)a[i] * (int)b[i];
  return acc;
}
