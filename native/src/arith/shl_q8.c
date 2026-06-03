/*
 * shl_q8.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shl_q8(const int8_t *a, size_t a_len, int8_t *out, int n)
{
    for (size_t i = 0; i < a_len; i++) {
        if (n <= 0)
            out[i] = a[i];
        else if (n >= 8)
            out[i] = a[i] > 0 ? 127 : a[i] < 0 ? -128 : 0;
        else
            out[i] = sat8((int16_t)((int16_t)a[i] << n));
    }
}
