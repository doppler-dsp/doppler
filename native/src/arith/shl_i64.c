/*
 * shl_i64.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shl_i64(const int64_t *a, size_t a_len, int64_t *out, int n)
{
    for (size_t i = 0; i < a_len; i++) {
        if (n <= 0)
            out[i] = a[i];
        else if (n >= 63)
            out[i] = 0;
        else
            out[i] = a[i] << n;
    }
}
