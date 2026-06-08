/*
 * sub_q15.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
sub_q15 (const int16_t *a, size_t a_len, const int16_t *b, size_t b_len,
         int16_t *out)
{
  size_t n = a_len < b_len ? a_len : b_len;
  for (size_t i = 0; i < n; i++)
    out[i] = sat16 ((int32_t)a[i] - b[i]);
}
