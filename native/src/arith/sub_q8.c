/*
 * sub_q8.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
sub_q8(const int8_t *a, size_t a_len,
       const int8_t *b, size_t b_len,
       int8_t *out)
{
    size_t n = a_len < b_len ? a_len : b_len;
    for (size_t i = 0; i < n; i++)
        out[i] = sat8((int16_t)a[i] - b[i]);
}
